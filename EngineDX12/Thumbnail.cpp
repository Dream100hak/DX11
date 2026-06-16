#include "Thumbnail.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include <dxcapi.h>
#include <algorithm>

using namespace DirectX;

// D3D12Device.cpp 의 공용 셰이더 컴파일 헬퍼
ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target);

// 미니 람베르트 셰이더(pos+nrm) — b0 = 32비트 상수(mvp16 + lightDir4 + baseCol4)
static const char* kThumbShader = R"(
cbuffer ThumbCB : register(b0)
{
    row_major float4x4 gMVP;
    float4 gLightDir; // xyz = 빛 진행방향
    float4 gBaseCol;  // rgb = 기본색
};
struct VSIn  { float3 pos : POSITION; float3 nrm : NORMAL; };
struct VSOut { float4 pos : SV_POSITION; float3 nrm : NORMAL; };
VSOut VSMain(VSIn i) { VSOut o; o.pos = mul(float4(i.pos, 1.0), gMVP); o.nrm = i.nrm; return o; }
float4 PSMain(VSOut i) : SV_Target
{
    float3 n = normalize(i.nrm);
    float ndl = saturate(dot(n, -normalize(gLightDir.xyz)));
    float3 c = gBaseCol.rgb * (0.30 + 0.70 * ndl);
    return float4(c, 1.0);
}
)";

ComPtr<ID3D12Resource> Thumbnail::UploadBuffer(const void* data, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "Thumb UploadBuffer");
	if (data)
	{
		void* p = nullptr; D3D12_RANGE noRead{ 0, 0 };
		ThrowIfFailed(buf->Map(0, &noRead, &p), "Map upload");
		memcpy(p, data, size); buf->Unmap(0, nullptr);
	}
	return buf;
}

void Thumbnail::Init(ID3D12Device* device, ID3D12CommandQueue* queue, ImGuiDx12* imgui)
{
	_device = device; _queue = queue; _imgui = imgui;

	// 루트시그: b0 = 32비트 상수 24개 (mvp16 + lightDir4 + baseCol4)
	D3D12_ROOT_PARAMETER rp{};
	rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rp.Constants.ShaderRegister = 0;
	rp.Constants.Num32BitValues = 24;
	rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 1; rs.pParameters = &rp;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "Thumb RootSig");
	ThrowIfFailed(_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "Thumb CreateRootSig");

	ComPtr<IDxcBlob> vs = CompileDxc(kThumbShader, L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ps = CompileDxc(kThumbShader, L"PSMain", L"ps_6_5");

	D3D12_INPUT_ELEMENT_DESC il[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _rootSig.Get();
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.InputLayout = { il, 2 };
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.RasterizerState.DepthClipEnable = TRUE;
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.DepthStencilState.DepthEnable = TRUE;
	pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pso)), "Thumb PSO");

	// RTV / DSV 힙 (각 1슬롯) — 렌더가 직렬이라 재사용
	D3D12_DESCRIPTOR_HEAP_DESC rh{}; rh.NumDescriptors = 1; rh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&rh, IID_PPV_ARGS(&_rtvHeap)), "Thumb RTV heap");
	D3D12_DESCRIPTOR_HEAP_DESC dh{}; dh.NumDescriptors = 1; dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(_device->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&_dsvHeap)), "Thumb DSV heap");

	// 공유 깊이
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC dd{};
	dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = RES; dd.Height = RES; dd.DepthOrArraySize = 1; dd.MipLevels = 1;
	dd.Format = DXGI_FORMAT_D32_FLOAT; dd.SampleDesc.Count = 1;
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE dcv{}; dcv.Format = DXGI_FORMAT_D32_FLOAT; dcv.DepthStencil.Depth = 1.0f;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &dcv, IID_PPV_ARGS(&_depth)), "Thumb depth");
	_device->CreateDepthStencilView(_depth.Get(), nullptr, _dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Thumbnail::NewFrame(int budget) { _budget = budget; }

uint64 Thumbnail::GetMesh(const std::wstring& meshPath)
{
	auto it = _id.find(meshPath);
	if (it != _id.end()) return it->second; // 캐시 히트

	if (!_pso) return 0;
	if (_budget <= 0) return 0; // 프레임당 생성 예산 초과 → 다음 프레임에

	std::vector<MeshPN> verts; std::vector<uint32> idx;
	if (!LoadMeshPN(meshPath, verts, idx) || idx.empty())
	{
		_id[meshPath] = 0; // 실패 캐시(재시도 안 함)
		return 0;
	}
	_budget--;

	// AABB → 핏 뷰/프로젝션
	XMVECTOR mn = XMVectorReplicate(1e30f), mx = XMVectorReplicate(-1e30f);
	for (auto& v : verts) { XMVECTOR p = XMLoadFloat3(&v.pos); mn = XMVectorMin(mn, p); mx = XMVectorMax(mx, p); }
	XMVECTOR cen = XMVectorScale(XMVectorAdd(mn, mx), 0.5f);
	float radius = XMVectorGetX(XMVector3Length(XMVectorScale(XMVectorSubtract(mx, mn), 0.5f)));
	if (radius < 1e-4f) radius = 1.0f;
	const float fov = XMConvertToRadians(35.0f);
	float dist = radius / sinf(fov * 0.5f) * 1.1f;
	XMVECTOR viewDir = XMVector3Normalize(XMVectorSet(0.4f, -0.35f, 0.85f, 0.0f)); // 우상-전방 3/4 뷰
	XMVECTOR eye = XMVectorSubtract(cen, XMVectorScale(viewDir, dist));
	XMMATRIX V = XMMatrixLookAtLH(eye, cen, XMVectorSet(0, 1, 0, 0));
	XMMATRIX P = XMMatrixPerspectiveFovLH(fov, 1.0f, (std::max)(0.01f, dist - radius * 2.0f), dist + radius * 2.0f);
	XMFLOAT4X4 mvp; XMStoreFloat4x4(&mvp, XMMatrixMultiply(V, P)); // world=I

	// 정점/인덱스 업로드 버퍼
	ComPtr<ID3D12Resource> vb = UploadBuffer(verts.data(), verts.size() * sizeof(MeshPN));
	ComPtr<ID3D12Resource> ib = UploadBuffer(idx.data(), idx.size() * sizeof(uint32));
	D3D12_VERTEX_BUFFER_VIEW vbv{ vb->GetGPUVirtualAddress(), (UINT)(verts.size() * sizeof(MeshPN)), sizeof(MeshPN) };
	D3D12_INDEX_BUFFER_VIEW  ibv{ ib->GetGPUVirtualAddress(), (UINT)(idx.size() * sizeof(uint32)), DXGI_FORMAT_R32_UINT };

	// 색 텍스처 (RENDER_TARGET → PIXEL_SHADER_RESOURCE)
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC cd{};
	cd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	cd.Width = RES; cd.Height = RES; cd.DepthOrArraySize = 1; cd.MipLevels = 1;
	cd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; cd.SampleDesc.Count = 1;
	cd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE ccv{}; ccv.Format = cd.Format;
	ccv.Color[0] = 0.12f; ccv.Color[1] = 0.12f; ccv.Color[2] = 0.14f; ccv.Color[3] = 1.0f;
	ComPtr<ID3D12Resource> color;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &cd,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &ccv, IID_PPV_ARGS(&color)), "Thumb color");
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateRenderTargetView(color.Get(), nullptr, rtv);
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	// 임시 커맨드리스트로 렌더
	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList> cl;
	_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
	_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), _pso.Get(), IID_PPV_ARGS(&cl));

	D3D12_VIEWPORT vp{ 0, 0, (float)RES, (float)RES, 0, 1 };
	D3D12_RECT scis{ 0, 0, (LONG)RES, (LONG)RES };
	cl->RSSetViewports(1, &vp);
	cl->RSSetScissorRects(1, &scis);
	cl->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	cl->ClearRenderTargetView(rtv, ccv.Color, 0, nullptr);
	cl->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	cl->SetGraphicsRootSignature(_rootSig.Get());
	float consts[24];
	memcpy(&consts[0], &mvp, 64);
	XMFLOAT4 lightDir(-0.4f, -0.7f, -0.5f, 0.0f); memcpy(&consts[16], &lightDir, 16);
	XMFLOAT4 baseCol(0.82f, 0.82f, 0.86f, 1.0f);  memcpy(&consts[20], &baseCol, 16);
	cl->SetGraphicsRoot32BitConstants(0, 24, consts, 0);
	cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->IASetVertexBuffers(0, 1, &vbv);
	cl->IASetIndexBuffer(&ibv);
	cl->DrawIndexedInstanced((UINT)idx.size(), 1, 0, 0, 0);

	D3D12_RESOURCE_BARRIER bar{};
	bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	bar.Transition.pResource = color.Get();
	bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cl->ResourceBarrier(1, &bar);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() };
	_queue->ExecuteCommandLists(1, lists);

	// 펜스 대기 (VB/IB/alloc 안전 해제 보장)
	ComPtr<ID3D12Fence> fence; _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	_queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, INFINITE); }
	CloseHandle(ev);

	uint64 id = _imgui->RegisterTexture(color.Get());
	_tex[meshPath] = color; // 생존 유지
	_id[meshPath] = id;
	return id;
}

uint64 Thumbnail::GetImage(const std::wstring& imgPath)
{
	auto it = _id.find(imgPath);
	if (it != _id.end()) return it->second; // 캐시 히트(0=디코드 실패도 캐시 → 재시도 안 함)

	if (_budget <= 0) return 0;

	std::vector<uint8_t> pixels; uint32 sw = 0, sh = 0;
	if (!LoadImageRGBA(imgPath, pixels, sw, sh) || sw == 0 || sh == 0)
	{
		_id[imgPath] = 0; // 실패 캐시(예: WIC 미지원 .dds) → IMG 아이콘 폴백
		return 0;
	}
	_budget--;

	// 썸네일용 다운스케일 (최대변 128, 최근접) — 원본 고해상도 텍스처를 SRV 풀에 그대로 두지 않음
	const uint32 MAXD = 128;
	uint32 tw = sw, th = sh;
	if (sw > MAXD || sh > MAXD)
	{
		float s = (float)MAXD / (float)(std::max)(sw, sh);
		tw = (std::max)(1u, (uint32)(sw * s));
		th = (std::max)(1u, (uint32)(sh * s));
	}
	std::vector<uint8_t> dnPix((size_t)tw * th * 4);
	for (uint32 y = 0; y < th; ++y)
	{
		uint32 syi = (uint32)((uint64)y * sh / th);
		for (uint32 x = 0; x < tw; ++x)
		{
			uint32 sxi = (uint32)((uint64)x * sw / tw);
			const uint8_t* sp = &pixels[((size_t)syi * sw + sxi) * 4];
			uint8_t* dp = &dnPix[((size_t)y * tw + x) * 4];
			dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
		}
	}

	// DEFAULT 텍스처 (COPY_DEST → PIXEL_SHADER_RESOURCE)
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC td{};
	td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	td.Width = tw; td.Height = th; td.DepthOrArraySize = 1; td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
	ComPtr<ID3D12Resource> tex;
	ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex)), "Img thumb tex");

	// 업로드 버퍼
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT rows = 0; UINT64 rowSize = 0, total = 0;
	_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);
	D3D12_HEAP_PROPERTIES up{}; up.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC bd{};
	bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
	bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN;
	bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> upload;
	ThrowIfFailed(_device->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)), "Img thumb upload");

	uint8_t* mapped = nullptr; D3D12_RANGE nr{ 0, 0 };
	upload->Map(0, &nr, (void**)&mapped);
	for (UINT y = 0; y < rows; ++y)
		memcpy(mapped + fp.Offset + (size_t)y * fp.Footprint.RowPitch, dnPix.data() + (size_t)y * tw * 4, (size_t)tw * 4);
	upload->Unmap(0, nullptr);

	// 임시 커맨드리스트로 복사 + 배리어
	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList> cl;
	_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
	_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cl));
	D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = tex.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = upload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
	cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	D3D12_RESOURCE_BARRIER bar{};
	bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	bar.Transition.pResource = tex.Get();
	bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cl->ResourceBarrier(1, &bar);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() };
	_queue->ExecuteCommandLists(1, lists);

	// 펜스 대기 (upload/alloc 안전 해제)
	ComPtr<ID3D12Fence> fence; _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	_queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, INFINITE); }
	CloseHandle(ev);

	uint64 id = _imgui->RegisterTexture(tex.Get());
	_tex[imgPath] = tex; // 생존 유지
	_id[imgPath] = id;
	return id;
}
