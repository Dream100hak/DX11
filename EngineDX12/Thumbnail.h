#pragma once
#include "Common.h"
#include "ImGuiDx12.h"
#include <string>
#include <unordered_map>

// ───────────────────────────────────────────────────────────
// Thumbnail — FolderContents 그리드용 에셋 썸네일 생성/캐시.
//  · 메시(.mesh): 작은 RT 에 미니 람베르트로 1회 렌더 후 ImGui 텍스처 등록.
//  · 이미지(PNG/JPG): 128px 다운스케일 후 텍스처 업로드.
// 메인 커맨드리스트와 독립 — 자체 임시 커맨드리스트/펜스로 즉시 렌더 후 대기.
// 프레임당 신규 생성 수를 NewFrame(budget) 으로 제한해 일괄 플러시 스터터를 막는다.
// ───────────────────────────────────────────────────────────
class Thumbnail
{
public:
	void Init(ID3D12Device* device, ID3D12CommandQueue* queue, ImGuiDx12* imgui);
	void NewFrame(int budget);  // 프레임 시작 — 신규 생성 예산 리셋

	uint64 GetMesh(const std::wstring& meshPath);  // 캐시 or 즉시 렌더 → ImTextureID (0=실패/예산초과)
	uint64 GetImage(const std::wstring& imgPath);  // PNG/JPG 디코드(128 다운스케일) → ImTextureID (0=실패/예산초과)

private:
	ComPtr<ID3D12Resource> UploadBuffer(const void* data, size_t size);

	ID3D12Device*                _device = nullptr;
	ID3D12CommandQueue*          _queue = nullptr;
	ImGuiDx12*                   _imgui = nullptr;

	ComPtr<ID3D12RootSignature>  _rootSig;
	ComPtr<ID3D12PipelineState>  _pso;
	ComPtr<ID3D12DescriptorHeap> _rtvHeap;   // 1슬롯(렌더는 직렬 → 재사용)
	ComPtr<ID3D12DescriptorHeap> _dsvHeap;   // 1슬롯
	ComPtr<ID3D12Resource>       _depth;     // 공유 깊이

	std::unordered_map<std::wstring, ComPtr<ID3D12Resource>> _tex; // 텍스처(생존 유지)
	std::unordered_map<std::wstring, uint64>                 _id;  // path → ImTextureID
	int                          _budget = 0; // 프레임당 신규 생성 제한

	static const UINT            RES = 128;   // 썸네일 RT 해상도
};
