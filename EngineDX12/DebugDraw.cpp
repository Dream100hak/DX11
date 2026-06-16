#include "DebugDraw.h"
#include <dxcapi.h>

using namespace DirectX;

// D3D12Device.cpp 의 공용 셰이더 컴파일 헬퍼
ComPtr<IDxcBlob> CompileDxc(const char* src, const wchar_t* entry, const wchar_t* target);

// 디버그 라인 셰이더 — b0 의 SceneCB 중 gMVP(첫 멤버)만 사용. pos+color 라인.
static const char* kDbgLineShader = R"(
cbuffer SceneCB : register(b0) { row_major float4x4 gMVP; };
struct VIn { float3 pos:POSITION; float3 col:COLOR; };
struct VOut { float4 pos:SV_POSITION; float3 col:COLOR; };
VOut VSMain(VIn i) { VOut o; o.pos = mul(float4(i.pos, 1.0), gMVP); o.col = i.col; return o; }
float4 PSMain(VOut i) : SV_TARGET { return float4(i.col, 1.0); }
)";

void DebugDraw::Init(ID3D12Device* device, ID3D12RootSignature* rootSig, DXGI_FORMAT sceneFmt)
{
	_device = device;
	_rootSig = rootSig;

	ComPtr<IDxcBlob> vs = CompileDxc(kDbgLineShader, L"VSMain", L"vs_6_5");
	ComPtr<IDxcBlob> ps = CompileDxc(kDbgLineShader, L"PSMain", L"ps_6_5");

	D3D12_INPUT_ELEMENT_DESC il[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC rast{};
	rast.FillMode = D3D12_FILL_MODE_SOLID;
	rast.CullMode = D3D12_CULL_MODE_NONE;
	rast.DepthClipEnable = TRUE;

	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = _rootSig;
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.InputLayout = { il, 2 };
	pso.RasterizerState = rast;
	pso.BlendState = blend;
	pso.DepthStencilState.DepthEnable = TRUE;
	pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = sceneFmt;
	pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso.SampleDesc.Count = 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pso)), "CreateDbgPSO");

	// 오버레이 — 깊이 테스트 OFF (스켈레톤이 메시 안에 묻혀도 항상 보이도록)
	pso.DepthStencilState.DepthEnable = FALSE;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_overlayPSO)), "CreateDbgOverlayPSO");
}

void DebugDraw::Begin()
{
	_depth.data.clear();
	_overlay.data.clear();
}

void DebugDraw::Line(XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, bool overlay)
{
	Batch& batch = overlay ? _overlay : _depth;
	float arr[12] = { a.x, a.y, a.z, c.x, c.y, c.z, b.x, b.y, b.z, c.x, c.y, c.z };
	batch.data.insert(batch.data.end(), arr, arr + 12);
}

void DebugDraw::Cross(XMFLOAT3 p, XMFLOAT3 c, float s, bool overlay)
{
	Line({ p.x - s, p.y, p.z }, { p.x + s, p.y, p.z }, c, overlay);
	Line({ p.x, p.y - s, p.z }, { p.x, p.y + s, p.z }, c, overlay);
	Line({ p.x, p.y, p.z - s }, { p.x, p.y, p.z + s }, c, overlay);
}

void DebugDraw::DrawBatch(ID3D12GraphicsCommandList* cmd, Batch& b, ID3D12PipelineState* pso)
{
	if (b.data.empty()) return;
	UINT bytes = (UINT)(b.data.size() * sizeof(float));
	if (b.cap < bytes)
	{
		D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = bytes + 8192; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		b.vb.Reset();
		ThrowIfFailed(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&b.vb)), "DbgVB");
		b.cap = bytes + 8192;
		D3D12_RANGE nr{ 0, 0 }; b.vb->Map(0, &nr, &b.mapped);
	}
	memcpy(b.mapped, b.data.data(), bytes);
	D3D12_VERTEX_BUFFER_VIEW vbv{ b.vb->GetGPUVirtualAddress(), bytes, 24 };
	cmd->SetPipelineState(pso);
	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->DrawInstanced((UINT)(b.data.size() / 6), 1, 0, 0);
}

void DebugDraw::Flush(ID3D12GraphicsCommandList* cmd, D3D12_GPU_VIRTUAL_ADDRESS cbAddr)
{
	if (_depth.data.empty() && _overlay.data.empty()) return;
	cmd->SetGraphicsRootSignature(_rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	DrawBatch(cmd, _depth, _pso.Get());
	DrawBatch(cmd, _overlay, _overlayPSO.Get());
}
