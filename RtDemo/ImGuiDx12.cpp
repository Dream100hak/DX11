#include "ImGuiDx12.h"
#include "imgui.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

// ImGui 2D 셰이더 (SM5.0, FXC) — 공식 imgui_impl_dx12 와 동일 수식
static const char* kImGuiVS = R"(
cbuffer vertexBuffer : register(b0) { float4x4 ProjectionMatrix; };
struct VS_INPUT { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR0; };
struct PS_INPUT { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
PS_INPUT main(VS_INPUT i)
{
    PS_INPUT o;
    o.pos = mul(ProjectionMatrix, float4(i.pos.xy, 0.0, 1.0));
    o.col = i.col;
    o.uv  = i.uv;
    return o;
}
)";

static const char* kImGuiPS = R"(
struct PS_INPUT { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
SamplerState s0 : register(s0);
Texture2D    t0 : register(t0);
float4 main(PS_INPUT i) : SV_Target { return i.col * t0.Sample(s0, i.uv); }
)";

void ImGuiDx12::Init(ID3D12Device* dev, ID3D12CommandQueue* queue, DXGI_FORMAT rtvFormat, UINT framesInFlight)
{
	_dev = dev;
	_frames = framesInFlight;
	_frameBufs.resize(framesInFlight);

	// 루트시그: b0 루트상수(mvp 16) / t0 테이블 / s0 정적샘플러
	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = 1;
	range.BaseShaderRegister = 0;
	range.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER params[2]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[0].Constants.ShaderRegister = 0; params[0].Constants.Num32BitValues = 16;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &range;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC samp{};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samp.ShaderRegister = 0;
	samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 2; rs.pParameters = params;
	rs.NumStaticSamplers = 1; rs.pStaticSamplers = &samp;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> sig, err;
	ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err), "ImGui RootSig");
	ThrowIfFailed(_dev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&_rootSig)), "ImGui CreateRootSig");

	ComPtr<ID3DBlob> vs, ps, e2;
	ThrowIfFailed(D3DCompile(kImGuiVS, strlen(kImGuiVS), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vs, &e2), "ImGui VS");
	ThrowIfFailed(D3DCompile(kImGuiPS, strlen(kImGuiPS), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &ps, &e2), "ImGui PS");

	D3D12_INPUT_ELEMENT_DESC il[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _rootSig.Get();
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.InputLayout = { il, 3 };
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = rtvFormat;
	pso.SampleDesc.Count = 1;
	pso.SampleMask = UINT_MAX;
	// 래스터(컬링 없음, 스시저 ON)
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.RasterizerState.DepthClipEnable = TRUE;
	// 알파 블렌드
	pso.BlendState.RenderTarget[0].BlendEnable = TRUE;
	pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// 깊이 없음
	pso.DepthStencilState.DepthEnable = FALSE;
	pso.DepthStencilState.StencilEnable = FALSE;
	ThrowIfFailed(_dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pso)), "ImGui PSO");

	// SRV 힙 (slot0 폰트 / slot1 씬RT / 여유)
	D3D12_DESCRIPTOR_HEAP_DESC hd{};
	hd.NumDescriptors = 8;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_srvHeap)), "ImGui SRV heap");
	_srvInc = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CreateFontTexture(queue);
}

void ImGuiDx12::CreateFontTexture(ID3D12CommandQueue* queue)
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels = nullptr; int w = 0, h = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

	// DEFAULT 텍스처
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC td{};
	td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	td.Width = w; td.Height = h; td.DepthOrArraySize = 1; td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
	ThrowIfFailed(_dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_fontTex)), "ImGui font tex");

	// 업로드 버퍼
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT rows = 0; UINT64 rowSize = 0, total = 0;
	_dev->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);
	D3D12_HEAP_PROPERTIES up{}; up.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC bd{};
	bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
	bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN;
	bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> upload;
	ThrowIfFailed(_dev->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)), "ImGui font upload");

	uint8_t* mapped = nullptr; D3D12_RANGE nr{ 0, 0 };
	upload->Map(0, &nr, (void**)&mapped);
	for (UINT y = 0; y < rows; ++y)
		memcpy(mapped + fp.Offset + (size_t)y * fp.Footprint.RowPitch, pixels + (size_t)y * w * 4, (size_t)w * 4);
	upload->Unmap(0, nullptr);

	// 임시 커맨드리스트로 복사 + 배리어
	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList> cl;
	_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
	_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cl));

	D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = _fontTex.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = upload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
	cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	D3D12_RESOURCE_BARRIER bar{};
	bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	bar.Transition.pResource = _fontTex.Get();
	bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cl->ResourceBarrier(1, &bar);
	cl->Close();
	ID3D12CommandList* lists[] = { cl.Get() };
	queue->ExecuteCommandLists(1, lists);

	// 펜스 대기
	ComPtr<ID3D12Fence> fence; _dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, INFINITE); }
	CloseHandle(ev);

	// SRV 생성
	D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sd.Texture2D.MipLevels = 1;
	_dev->CreateShaderResourceView(_fontTex.Get(), &sd, _srvHeap->GetCPUDescriptorHandleForHeapStart());
	_fontGpu = _srvHeap->GetGPUDescriptorHandleForHeapStart();
	io.Fonts->SetTexID((ImTextureID)_fontGpu.ptr);
}

uint64 ImGuiDx12::SetSceneTexture(ID3D12Resource* tex)
{
	const UINT slot = 1;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = _srvHeap->GetCPUDescriptorHandleForHeapStart();
	cpu.ptr += (SIZE_T)slot * _srvInc;
	D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sd.Texture2D.MipLevels = 1;
	_dev->CreateShaderResourceView(tex, &sd, cpu);
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = _srvHeap->GetGPUDescriptorHandleForHeapStart();
	gpu.ptr += (UINT64)slot * _srvInc;
	return gpu.ptr;
}

void ImGuiDx12::Render(ID3D12GraphicsCommandList* cmd, UINT frameIndex)
{
	ImDrawData* dd = ImGui::GetDrawData();
	if (!dd || dd->TotalVtxCount == 0) return;
	FrameBuf& fb = _frameBufs[frameIndex % _frames];

	// VB/IB 용량 확보 (증가 시 재생성)
	auto makeUpload = [&](ComPtr<ID3D12Resource>& res, UINT64 size)
	{
		D3D12_HEAP_PROPERTIES up{}; up.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC bd{};
		bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = size; bd.Height = 1;
		bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN;
		bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		res.Reset();
		_dev->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&res));
	};
	if (fb.vbCount < (UINT)dd->TotalVtxCount) { fb.vbCount = dd->TotalVtxCount + 5000; makeUpload(fb.vb, fb.vbCount * sizeof(ImDrawVert)); }
	if (fb.ibCount < (UINT)dd->TotalIdxCount) { fb.ibCount = dd->TotalIdxCount + 10000; makeUpload(fb.ib, fb.ibCount * sizeof(ImDrawIdx)); }

	// 정점/인덱스 채우기
	ImDrawVert* vtx = nullptr; ImDrawIdx* idx = nullptr; D3D12_RANGE nr{ 0, 0 };
	fb.vb->Map(0, &nr, (void**)&vtx);
	fb.ib->Map(0, &nr, (void**)&idx);
	for (int n = 0; n < dd->CmdListsCount; ++n)
	{
		const ImDrawList* cl = dd->CmdLists[n];
		memcpy(vtx, cl->VtxBuffer.Data, cl->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx, cl->IdxBuffer.Data, cl->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx += cl->VtxBuffer.Size; idx += cl->IdxBuffer.Size;
	}
	fb.vb->Unmap(0, nullptr); fb.ib->Unmap(0, nullptr);

	// ortho 투영 (공식 imgui_impl_dx12 레이아웃)
	float L = dd->DisplayPos.x, R = dd->DisplayPos.x + dd->DisplaySize.x;
	float T = dd->DisplayPos.y, B = dd->DisplayPos.y + dd->DisplaySize.y;
	float mvp[16] = {
		2.0f / (R - L),   0,                0,    0,
		0,                2.0f / (T - B),   0,    0,
		0,                0,                0.5f, 0,
		(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f,
	};

	// 파이프라인 상태
	D3D12_VIEWPORT vp{ 0, 0, dd->DisplaySize.x, dd->DisplaySize.y, 0, 1 };
	cmd->RSSetViewports(1, &vp);
	cmd->SetPipelineState(_pso.Get());
	cmd->SetGraphicsRootSignature(_rootSig.Get());
	cmd->SetGraphicsRoot32BitConstants(0, 16, mvp, 0);
	ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
	cmd->SetDescriptorHeaps(1, heaps);
	cmd->SetGraphicsRootDescriptorTable(1, _fontGpu);
	float blend[4] = { 0, 0, 0, 0 };
	cmd->OMSetBlendFactor(blend);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VERTEX_BUFFER_VIEW vbv{ fb.vb->GetGPUVirtualAddress(), (UINT)(fb.vbCount * sizeof(ImDrawVert)), sizeof(ImDrawVert) };
	D3D12_INDEX_BUFFER_VIEW  ibv{ fb.ib->GetGPUVirtualAddress(), (UINT)(fb.ibCount * sizeof(ImDrawIdx)), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->IASetIndexBuffer(&ibv);

	int globalVtx = 0, globalIdx = 0;
	ImVec2 clipOff = dd->DisplayPos;
	for (int n = 0; n < dd->CmdListsCount; ++n)
	{
		const ImDrawList* cl = dd->CmdLists[n];
		for (int c = 0; c < cl->CmdBuffer.Size; ++c)
		{
			const ImDrawCmd* pcmd = &cl->CmdBuffer[c];
			ImVec2 cmin(pcmd->ClipRect.x - clipOff.x, pcmd->ClipRect.y - clipOff.y);
			ImVec2 cmax(pcmd->ClipRect.z - clipOff.x, pcmd->ClipRect.w - clipOff.y);
			if (cmax.x <= cmin.x || cmax.y <= cmin.y) continue;
			D3D12_RECT sc{ (LONG)cmin.x, (LONG)cmin.y, (LONG)cmax.x, (LONG)cmax.y };
			cmd->RSSetScissorRects(1, &sc);
			// 텍스처(폰트만 사용 — TextureId 가 폰트 핸들)
			D3D12_GPU_DESCRIPTOR_HANDLE tex{ (UINT64)pcmd->GetTexID() };
			cmd->SetGraphicsRootDescriptorTable(1, tex.ptr ? tex : _fontGpu);
			cmd->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + globalIdx, pcmd->VtxOffset + globalVtx, 0);
		}
		globalVtx += cl->VtxBuffer.Size;
		globalIdx += cl->IdxBuffer.Size;
	}
}

void ImGuiDx12::Shutdown()
{
	_pso.Reset(); _rootSig.Reset(); _fontTex.Reset(); _srvHeap.Reset();
	_frameBufs.clear();
}
