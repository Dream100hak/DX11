#include "PostFX.h"
#include <dxcapi.h>

// D3D12Device.cpp 의 공용 셰이더 컴파일 헬퍼
ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target);

// 포스트프로세스 공용 — 풀스크린 삼각형 VS
static const char* kPostCommon = R"(
struct VOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
VOut VSFull(uint id : SV_VertexID)
{
    VOut o; float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv; o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); return o;
}
)";
// ACES 톤맵 + 노출 + 감마 (+ S4 블룸 합성) + 브라이트패스/블러/FXAA
static const std::string kTonemapShader = std::string(kPostCommon) + R"(
Texture2D gHDR : register(t0);
Texture2D gBloom : register(t1);
SamplerState gS : register(s0);
cbuffer PostCB : register(b0) { float gExposure; float gBloomI; float gBloomOn; float gTonemapOp;
                                float gContrast; float gSaturation; float gTemperature; float gVignette; };
cbuffer PostCB2 : register(b1) { float gChroma; float gGrain; float gSharpen; float gPostTime; float gTexelX; float gTexelY; float gExpScale; float gAutoExp; };
cbuffer PostCB3 : register(b2) { float gSunSX; float gSunSY; float gVolStr; float gDofFocus; float gDofRange; float gDofOn; float gVolOn; float _p3; };
cbuffer PostCB4 : register(b3) { float gLensDistort; float gPosterize; float gAnamorphic; float gFilterMode; float _q0; float _q1; float _q2; float _q3; };
Texture2D gDepth : register(t2);
float3 ACES(float3 x){ float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14; return saturate((x*(a*x+b))/(x*(c*x+d)+e)); }
float3 Filmic(float3 x){ x=max(0,x-0.004); return (x*(6.2*x+0.5))/(x*(6.2*x+1.7)+0.06); } // 감마 내장
float4 PSTonemap(VOut i) : SV_TARGET
{
    // V10 렌즈 왜곡 (배럴/핀쿠션) — 이후 모든 샘플에 적용
    if (gLensDistort != 0.0) { float2 cc = i.uv - 0.5; i.uv = 0.5 + cc * (1.0 + gLensDistort * dot(cc, cc)); }
    // 색수차 (채널별 오프셋 샘플)
    float2 caoff = gChroma * (i.uv - 0.5);
    float3 hdr;
    hdr.r = gHDR.Sample(gS, i.uv + caoff).r;
    hdr.g = gHDR.Sample(gS, i.uv).g;
    hdr.b = gHDR.Sample(gS, i.uv - caoff).b;
    if (gBloomOn > 0.5) hdr += gBloom.Sample(gS, i.uv).rgb * gBloomI;
    // V19 아나모픽 블룸 (수평 스트릭)
    if (gAnamorphic > 0.5)
        hdr += (gBloom.Sample(gS, i.uv + float2(gTexelX * 9.0, 0)).rgb + gBloom.Sample(gS, i.uv - float2(gTexelX * 9.0, 0)).rgb) * gBloomI * 0.5 * float3(0.4, 0.6, 1.0);
    // 샤픈 (언샤프 마스크)
    if (gSharpen > 0.001)
    {
        float2 tx = float2(gTexelX, gTexelY);
        float3 nb = (gHDR.Sample(gS, i.uv + float2(tx.x,0)).rgb + gHDR.Sample(gS, i.uv - float2(tx.x,0)).rgb
                   + gHDR.Sample(gS, i.uv + float2(0,tx.y)).rgb + gHDR.Sample(gS, i.uv - float2(0,tx.y)).rgb) * 0.25;
        hdr += (hdr - nb) * gSharpen;
    }
    // 피사계심도 (깊이 기반 CoC 디스크 블러)
    if (gDofOn > 0.5)
    {
        float zr = gDepth.Sample(gS, i.uv).r;
        float vz = (0.1 * 200.0) / (200.0 - zr * (200.0 - 0.1));
        float coc = saturate(abs(vz - gDofFocus) / max(gDofRange, 0.01));
        if (coc > 0.02)
        {
            float2 tx = float2(gTexelX, gTexelY) * coc * 7.0;
            float3 acc = hdr; float wsum = 1.0;
            [unroll] for (int k = 0; k < 8; ++k) { float a = k * 0.7853982; float2 o = float2(cos(a), sin(a)) * tx; acc += gHDR.Sample(gS, i.uv + o).rgb; wsum += 1.0; }
            hdr = acc / wsum;
        }
    }
    // 볼류메트릭 갓레이 (스크린스페이스 방사형, 태양 화면위치로)
    if (gVolOn > 0.5)
    {
        float2 dir = (float2(gSunSX, gSunSY) - i.uv) / 24.0;
        float2 uv2 = i.uv; float dec = 1.0; float3 gr = 0;
        [unroll] for (int k = 0; k < 24; ++k) { uv2 += dir; float3 sm = gHDR.Sample(gS, uv2).rgb; float l = max(dot(sm, float3(0.3,0.6,0.1)) - 1.0, 0.0); gr += sm * l * dec; dec *= 0.93; }
        hdr += gr * (gVolStr / 24.0);
    }
    hdr *= gExposure * gExpScale; // 수동 × 자동노출
    float3 c;
    if (gTonemapOp < 0.5)      { c = pow(ACES(hdr), 1.0/2.2); }
    else if (gTonemapOp < 1.5) { c = pow(hdr / (1.0 + hdr), 1.0/2.2); } // Reinhard
    else                       { c = Filmic(hdr); }                     // Filmic(감마 포함)
    // 컬러 그레이딩
    c.r *= 1.0 + gTemperature * 0.12; c.b *= 1.0 - gTemperature * 0.12;  // 색온도
    c = saturate((c - 0.5) * gContrast + 0.5);                          // 대비
    float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(luma, luma, luma), c, gSaturation);                 // 채도
    // V11 포스터라이즈
    if (gPosterize > 1.5) c = floor(c * gPosterize) / gPosterize;
    // V12 필터 (1 세피아 / 2 흑백 / 3 반전)
    if (gFilterMode > 0.5)
    {
        float lm = dot(c, float3(0.2126, 0.7152, 0.0722));
        if (gFilterMode < 1.5)      c = lm * float3(1.07, 0.82, 0.57); // 세피아
        else if (gFilterMode < 2.5) c = float3(lm, lm, lm);            // 흑백
        else                        c = 1.0 - c;                       // 반전
    }
    // 필름 그레인
    if (gGrain > 0.001) { float n = frac(sin(dot(i.uv * (gPostTime + 1.0), float2(12.9898, 78.233))) * 43758.5453); c += (n - 0.5) * gGrain; }
    // 비네트
    float2 dd = i.uv - 0.5;
    c *= saturate(1.0 - dot(dd, dd) * gVignette * 2.8);
    return float4(c, 1.0);
}
// 브라이트패스 — 휘도 임계값(gExposure 재사용) 초과분 추출
float4 PSBright(VOut i) : SV_TARGET
{
    float3 c = gHDR.Sample(gS, i.uv).rgb;
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float contrib = max(l - gExposure, 0.0);
    return float4(c * (contrib / (l + 1e-4)), 1.0);
}
// 분리형 가우시안 (cbuffer 재해석: gExposure,gBloomI=texel / gBloomOn,gTonemapOp=방향)
float4 PSBlur(VOut i) : SV_TARGET
{
    float2 texel = float2(gExposure, gBloomI);
    float2 dir   = float2(gBloomOn, gTonemapOp);
    float w[5] = { 0.227027, 0.194594, 0.121622, 0.054054, 0.016216 };
    float3 sum = gHDR.Sample(gS, i.uv).rgb * w[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 o = dir * texel * (float)k;
        sum += gHDR.Sample(gS, i.uv + o).rgb * w[k];
        sum += gHDR.Sample(gS, i.uv - o).rgb * w[k];
    }
    return float4(sum, 1.0);
}
// FXAA (콘솔 컴팩트판) — gExposure,gBloomI = 1/해상도
float4 PSFxaa(VOut i) : SV_TARGET
{
    float2 rcp = float2(gExposure, gBloomI);
    float3 luma = float3(0.299, 0.587, 0.114);
    float3 rgbM  = gHDR.Sample(gS, i.uv).rgb;
    float3 rgbNW = gHDR.Sample(gS, i.uv + float2(-1,-1) * rcp).rgb;
    float3 rgbNE = gHDR.Sample(gS, i.uv + float2( 1,-1) * rcp).rgb;
    float3 rgbSW = gHDR.Sample(gS, i.uv + float2(-1, 1) * rcp).rgb;
    float3 rgbSE = gHDR.Sample(gS, i.uv + float2( 1, 1) * rcp).rgb;
    float lNW = dot(rgbNW, luma), lNE = dot(rgbNE, luma), lSW = dot(rgbSW, luma), lSE = dot(rgbSE, luma), lM = dot(rgbM, luma);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));
    float reduce = max((lNW + lNE + lSW + lSE) * 0.25 * 0.125, 1e-5);
    float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpMin, -8.0, 8.0) * rcp;
    float3 rgbA = 0.5 * (gHDR.Sample(gS, i.uv + dir * (1.0/3.0 - 0.5)).rgb + gHDR.Sample(gS, i.uv + dir * (2.0/3.0 - 0.5)).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (gHDR.Sample(gS, i.uv + dir * -0.5).rgb + gHDR.Sample(gS, i.uv + dir * 0.5).rgb);
    float lB = dot(rgbB, luma);
    return float4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
)";

void PostFX::Init(ID3D12Device* device, DXGI_FORMAT sceneFmt)
{
	_device = device;
	_sceneFmt = sceneFmt;

	// 공용 SRV 힙 (shader-visible)
	D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = 8;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_srvHeap)), "post srv heap");
	_srvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// RTV 힙: 0 LDR / 1 LDR2 / 2 bloomA / 3 bloomB
	D3D12_DESCRIPTOR_HEAP_DESC rh{}; rh.NumDescriptors = 4; rh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&rh, IID_PPV_ARGS(&_rtvHeap)), "post rtv heap");
	_rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 루트시그: t0..t1 테이블 + b0/b1/b2 루트상수 + t2 depth 테이블(DOF) + b3 + s0
	D3D12_DESCRIPTOR_RANGE range{}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = 2; range.BaseShaderRegister = 0; range.OffsetInDescriptorsFromTableStart = 0;
	D3D12_DESCRIPTOR_RANGE rangeD{}; rangeD.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeD.NumDescriptors = 1; rangeD.BaseShaderRegister = 2; rangeD.OffsetInDescriptorsFromTableStart = 0;
	D3D12_ROOT_PARAMETER p[6]{};
	p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	p[0].DescriptorTable.NumDescriptorRanges = 1; p[0].DescriptorTable.pDescriptorRanges = &range;
	p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	p[1].Constants.ShaderRegister = 0; p[1].Constants.Num32BitValues = 8;
	p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	p[2].Constants.ShaderRegister = 1; p[2].Constants.Num32BitValues = 8; // b1
	p[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	p[3].Constants.ShaderRegister = 2; p[3].Constants.Num32BitValues = 8; // b2: DOF/갓레이
	p[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	p[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // t2 depth
	p[4].DescriptorTable.NumDescriptorRanges = 1; p[4].DescriptorTable.pDescriptorRanges = &rangeD;
	p[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	p[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	p[5].Constants.ShaderRegister = 3; p[5].Constants.Num32BitValues = 8; // b3: 렌즈왜곡/포스터/아나모픽/필터
	p[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	D3D12_STATIC_SAMPLER_DESC s{}; s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; s.ShaderRegister = 0;
	s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; s.MaxLOD = D3D12_FLOAT32_MAX;
	D3D12_ROOT_SIGNATURE_DESC rs{}; rs.NumParameters = 6; rs.pParameters = p; rs.NumStaticSamplers = 1; rs.pStaticSamplers = &s;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "post rootsig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "post rootsig create");

	auto makePSO = [&](const std::string& shader, const wchar_t* psEntry, DXGI_FORMAT fmt, ComPtr<ID3D12PipelineState>& out)
	{
		ComPtr<IDxcBlob> vs = CompileDxc(shader.c_str(), L"VSFull", L"vs_6_5");
		ComPtr<IDxcBlob> ps = CompileDxc(shader.c_str(), psEntry, L"ps_6_5");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
		d.pRootSignature = _rootSig.Get();
		d.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		d.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		d.DepthStencilState.DepthEnable = FALSE;
		d.SampleMask = UINT_MAX; d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		d.NumRenderTargets = 1; d.RTVFormats[0] = fmt; d.SampleDesc.Count = 1;
		ThrowIfFailed(_device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&out)), "post pso");
	};
	makePSO(kTonemapShader, L"PSTonemap", DXGI_FORMAT_R8G8B8A8_UNORM, _tonemapPSO);
	makePSO(kTonemapShader, L"PSBright", DXGI_FORMAT_R16G16B16A16_FLOAT, _brightPSO);
	makePSO(kTonemapShader, L"PSBlur",   DXGI_FORMAT_R16G16B16A16_FLOAT, _blurPSO);
	makePSO(kTonemapShader, L"PSFxaa",   DXGI_FORMAT_R8G8B8A8_UNORM,     _fxaaPSO);
}

void PostFX::Resize(UINT w, UINT h, ID3D12Resource* sceneRT, ID3D12Resource* sceneDepth)
{
	_w = w; _h = h;
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();

	// LDR / LDR2 (R8G8B8A8, ImGui 표시·톤맵·FXAA 결과)
	D3D12_RESOURCE_DESC ld{};
	ld.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	ld.Width = w; ld.Height = h; ld.DepthOrArraySize = 1; ld.MipLevels = 1;
	ld.Format = DXGI_FORMAT_R8G8B8A8_UNORM; ld.SampleDesc.Count = 1;
	ld.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cvl{}; cvl.Format = ld.Format; cvl.Color[3] = 1.0f;
	_sceneLDR.Reset(); _sceneLDR2.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &ld,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvl, IID_PPV_ARGS(&_sceneLDR)), "scene LDR");
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &ld,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvl, IID_PPV_ARGS(&_sceneLDR2)), "scene LDR2");
	_ldrState = _ldr2State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_CPU_DESCRIPTOR_HANDLE rLDR = rtv;                          _device->CreateRenderTargetView(_sceneLDR.Get(), nullptr, rLDR);   // slot0
	D3D12_CPU_DESCRIPTOR_HANDLE rLDR2 = rtv; rLDR2.ptr += _rtvInc;   _device->CreateRenderTargetView(_sceneLDR2.Get(), nullptr, rLDR2); // slot1

	// 블룸 핑퐁 (반해상도, sceneFmt)
	_bloomW = max(1u, w / 2); _bloomH = max(1u, h / 2);
	D3D12_RESOURCE_DESC bd = ld; bd.Width = _bloomW; bd.Height = _bloomH; bd.Format = _sceneFmt;
	D3D12_CLEAR_VALUE cvb{}; cvb.Format = _sceneFmt;
	_bloomA.Reset(); _bloomB.Reset();
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvb, IID_PPV_ARGS(&_bloomA)), "bloomA");
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cvb, IID_PPV_ARGS(&_bloomB)), "bloomB");
	_bloomAState = _bloomBState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_CPU_DESCRIPTOR_HANDLE rbA = rtv; rbA.ptr += _rtvInc * 2;   _device->CreateRenderTargetView(_bloomA.Get(), nullptr, rbA);  // slot2
	D3D12_CPU_DESCRIPTOR_HANDLE rbB = rtv; rbB.ptr += _rtvInc * 3;   _device->CreateRenderTargetView(_bloomB.Get(), nullptr, rbB);  // slot3

	// SRV 힙: 0 HDR씬 / 1 bloomA / 2 depth / 3 bloomB / 4 LDR / 5 LDR2
	D3D12_SHADER_RESOURCE_VIEW_DESC hsd{};
	hsd.Format = _sceneFmt; hsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	hsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; hsd.Texture2D.MipLevels = 1;
	D3D12_CPU_DESCRIPTOR_HANDLE ph = _srvHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateShaderResourceView(sceneRT, &hsd, ph); ph.ptr += _srvInc;   // 0 HDR
	_device->CreateShaderResourceView(_bloomA.Get(), &hsd, ph); ph.ptr += _srvInc; // 1 bloomA
	D3D12_SHADER_RESOURCE_VIEW_DESC dsd{}; dsd.Format = DXGI_FORMAT_R32_FLOAT;  // 2 depth
	dsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; dsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; dsd.Texture2D.MipLevels = 1;
	_device->CreateShaderResourceView(sceneDepth, &dsd, ph); ph.ptr += _srvInc;
	_device->CreateShaderResourceView(_bloomB.Get(), &hsd, ph); ph.ptr += _srvInc; // 3 bloomB
	D3D12_SHADER_RESOURCE_VIEW_DESC lsd{}; lsd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	lsd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; lsd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; lsd.Texture2D.MipLevels = 1;
	_device->CreateShaderResourceView(_sceneLDR.Get(), &lsd, ph); ph.ptr += _srvInc;  // 4 LDR
	_device->CreateShaderResourceView(_sceneLDR2.Get(), &lsd, ph);                     // 5 LDR2

	_bloomReady = true;
}

void PostFX::Transition(ID3D12GraphicsCommandList4* cmd, ID3D12Resource* res, D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES to)
{
	if (cur == to) return;
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = res;
	b.Transition.StateBefore = cur;
	b.Transition.StateAfter = to;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd->ResourceBarrier(1, &b);
	cur = to;
}

void PostFX::Bloom(ID3D12GraphicsCommandList4* cmd, float threshold)
{
	if (!_bloomReady) return;

	D3D12_CPU_DESCRIPTOR_HANDLE bA = _rtvHeap->GetCPUDescriptorHandleForHeapStart(); bA.ptr += _rtvInc * 2; // bloomA slot2
	D3D12_CPU_DESCRIPTOR_HANDLE bB = bA; bB.ptr += _rtvInc;                                                 // bloomB slot3
	D3D12_GPU_DESCRIPTOR_HANDLE post = _srvHeap->GetGPUDescriptorHandleForHeapStart();
	ID3D12DescriptorHeap* ph[] = { _srvHeap.Get() };
	D3D12_VIEWPORT bvp{ 0,0,float(_bloomW),float(_bloomH),0,1 };
	D3D12_RECT bsc{ 0,0,LONG(_bloomW),LONG(_bloomH) };
	float tx = 1.0f / float(_bloomW), ty = 1.0f / float(_bloomH);

	auto pass = [&](ID3D12PipelineState* pso, D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT srcSlot, const float c[8])
	{
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		cmd->RSSetViewports(1, &bvp); cmd->RSSetScissorRects(1, &bsc);
		cmd->SetPipelineState(pso);
		cmd->SetGraphicsRootSignature(_rootSig.Get());
		cmd->SetDescriptorHeaps(1, ph);
		D3D12_GPU_DESCRIPTOR_HANDLE t = post; t.ptr += UINT64(srcSlot) * _srvInc;
		cmd->SetGraphicsRootDescriptorTable(0, t);
		cmd->SetGraphicsRoot32BitConstants(1, 8, c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);
	};
	float bright[8] = { threshold, 0,0,0,0,0,0,0 };
	float blurH[8] = { tx, ty, 1.0f, 0.0f, 0,0,0,0 };
	float blurV[8] = { tx, ty, 0.0f, 1.0f, 0,0,0,0 };
	// 브라이트패스: 씬(slot0) → bloomA
	Transition(cmd, _bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pass(_brightPSO.Get(), bA, 0, bright);
	Transition(cmd, _bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	// BlurH: bloomA(slot1) → bloomB
	Transition(cmd, _bloomB.Get(), _bloomBState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pass(_blurPSO.Get(), bB, 1, blurH);
	Transition(cmd, _bloomB.Get(), _bloomBState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	// BlurV: bloomB(slot3) → bloomA (최종)
	Transition(cmd, _bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pass(_blurPSO.Get(), bA, 3, blurV);
	Transition(cmd, _bloomA.Get(), _bloomAState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void PostFX::Tonemap(ID3D12GraphicsCommandList4* cmd, const TonemapParams& p)
{
	Transition(cmd, _sceneLDR.Get(), _ldrState, D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_CPU_DESCRIPTOR_HANDLE ldrRtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart(); // slot0 LDR
	cmd->OMSetRenderTargets(1, &ldrRtv, FALSE, nullptr);
	D3D12_VIEWPORT vp{ 0, 0, float(_w), float(_h), 0, 1 };
	D3D12_RECT sc{ 0, 0, LONG(_w), LONG(_h) };
	cmd->RSSetViewports(1, &vp); cmd->RSSetScissorRects(1, &sc);
	cmd->SetPipelineState(_tonemapPSO.Get());
	cmd->SetGraphicsRootSignature(_rootSig.Get());
	ID3D12DescriptorHeap* ph[] = { _srvHeap.Get() };
	cmd->SetDescriptorHeaps(1, ph);
	cmd->SetGraphicsRootDescriptorTable(0, _srvHeap->GetGPUDescriptorHandleForHeapStart());
	float pc[8] = { p.exposure, p.bloomIntensity, p.bloomEnabled ? 1.0f : 0.0f, float(p.tonemapOp),
	                p.contrast, p.saturation, p.temperature, p.vignette };
	cmd->SetGraphicsRoot32BitConstants(1, 8, pc, 0);
	float pc2[8] = { p.chroma, p.grain, p.sharpen, p.time, 1.0f / float(_w), 1.0f / float(_h), p.expScale, 0.0f };
	cmd->SetGraphicsRoot32BitConstants(2, 8, pc2, 0);
	float pc3[8] = { p.sunSX, p.sunSY, p.volStrength, p.dofFocus, p.dofRange, p.dofOn ? 1.0f : 0.0f, (p.volOn && p.sunVisible) ? 1.0f : 0.0f, 0.0f };
	cmd->SetGraphicsRoot32BitConstants(3, 8, pc3, 0);
	D3D12_GPU_DESCRIPTOR_HANDLE depthH = _srvHeap->GetGPUDescriptorHandleForHeapStart(); depthH.ptr += UINT64(2) * _srvInc; // slot2 depth
	cmd->SetGraphicsRootDescriptorTable(4, depthH);
	float pc4[8] = { p.lensDistort, p.posterize, p.anamorphic ? 1.0f : 0.0f, float(p.filterMode), 0,0,0,0 };
	cmd->SetGraphicsRoot32BitConstants(5, 8, pc4, 0);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);

	Transition(cmd, _sceneLDR.Get(), _ldrState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

ID3D12Resource* PostFX::Fxaa(ID3D12GraphicsCommandList4* cmd, bool fxaaOn)
{
	if (!fxaaOn) return _sceneLDR.Get();

	Transition(cmd, _sceneLDR2.Get(), _ldr2State, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv2 = _rtvHeap->GetCPUDescriptorHandleForHeapStart(); rtv2.ptr += _rtvInc; // slot1 LDR2
	cmd->OMSetRenderTargets(1, &rtv2, FALSE, nullptr);
	D3D12_VIEWPORT vp{ 0, 0, float(_w), float(_h), 0, 1 };
	D3D12_RECT sc{ 0, 0, LONG(_w), LONG(_h) };
	cmd->RSSetViewports(1, &vp); cmd->RSSetScissorRects(1, &sc);
	cmd->SetPipelineState(_fxaaPSO.Get());
	cmd->SetGraphicsRootSignature(_rootSig.Get());
	ID3D12DescriptorHeap* ph[] = { _srvHeap.Get() };
	cmd->SetDescriptorHeaps(1, ph);
	D3D12_GPU_DESCRIPTOR_HANDLE t = _srvHeap->GetGPUDescriptorHandleForHeapStart(); t.ptr += UINT64(4) * _srvInc; // slot4 = LDR
	cmd->SetGraphicsRootDescriptorTable(0, t);
	float fc[8] = { 1.0f / float(_w), 1.0f / float(_h), 0,0,0,0,0,0 };
	cmd->SetGraphicsRoot32BitConstants(1, 8, fc, 0);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
	Transition(cmd, _sceneLDR2.Get(), _ldr2State, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	return _sceneLDR2.Get();
}
