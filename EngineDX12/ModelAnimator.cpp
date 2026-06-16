#include "ModelAnimator.h"
#include "D3D12Device.h"
#include "GameObject.h"
#include "Transform.h"
#include "Define.h"
#include "TimeManager.h"
#include "imgui.h"
#include <filesystem>

using namespace DirectX;
namespace fs = std::filesystem;

static ComPtr<ID3D12Resource> MakeUploadA(ID3D12Device* dev, size_t size)
{
	D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buf;
	ThrowIfFailed(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)), "ModelAnimator buffer");
	return buf;
}

static std::wstring FindClipA(const std::wstring& dir, const std::wstring& stem)
{
	std::error_code ec;
	for (const std::wstring& cand : { stem + L".clip", std::wstring(L"Idle.clip"), std::wstring(L"Sprint.clip") })
		if (fs::exists(dir + cand, ec)) return dir + cand;
	for (auto& e : fs::directory_iterator(dir, ec))
		if (e.path().extension() == L".clip") return e.path().wstring();
	return L"";
}

void ModelAnimator::ScanClips()
{
	_clips.clear(); std::error_code ec;
	for (auto& e : fs::directory_iterator(_modelDir, ec))
		if (e.path().extension() == L".clip") _clips.push_back(e.path().wstring());
}

bool ModelAnimator::Load(const std::wstring& meshPath)
{
	if (!_dev) return false;
	fs::path mp(meshPath);
	_modelDir = mp.parent_path().wstring() + L"\\";
	_modelStem = mp.stem().wstring();

	_bonesData.clear(); _skinSrc.clear(); _indices.clear(); _submeshes.clear();
	_clip = AnimClip{}; _animated = false; _curClip = 0; _animTime = 0.f;

	if (!LoadMeshSkinned(meshPath, _bonesData, _skinSrc, _indices, &_submeshes)) return false;
	GenerateTangents(_skinSrc, _indices);

	// 바인드 AABB → 높이 2.2 정규화, x/z 중앙, 바닥 안착 (ModelScene 과 동일)
	XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
	for (auto& v : _skinSrc)
	{
		mn.x = min(mn.x, v.pos.x); mn.y = min(mn.y, v.pos.y); mn.z = min(mn.z, v.pos.z);
		mx.x = max(mx.x, v.pos.x); mx.y = max(mx.y, v.pos.y); mx.z = max(mx.z, v.pos.z);
	}
	_modelScale = 2.2f / max(mx.y - mn.y, 0.001f);
	_modelOffset = XMFLOAT3((mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f);
	_vtxCount = (uint32)_skinSrc.size();
	_idxCount = (uint32)_indices.size();
	_world.assign(_vtxCount, Vtx{});

	ScanClips();
	std::wstring clipPath = FindClipA(_modelDir, _modelStem);
	if (!clipPath.empty()) _animated = LoadClip(clipPath, _clip);

	// 월드 VB (영속 매핑, 매 프레임 스키닝 갱신) + IB
	const size_t vbSize = (size_t)_vtxCount * sizeof(Vtx);
	const size_t ibSize = (size_t)_idxCount * sizeof(uint32);
	_vb = MakeUploadA(_dev->_device.Get(), vbSize);
	D3D12_RANGE nr{ 0, 0 }; _vb->Map(0, &nr, &_vbMapped);
	_vbv.BufferLocation = _vb->GetGPUVirtualAddress(); _vbv.StrideInBytes = sizeof(Vtx); _vbv.SizeInBytes = (UINT)vbSize;
	_ib = MakeUploadA(_dev->_device.Get(), ibSize);
	void* p = nullptr; _ib->Map(0, &nr, &p); memcpy(p, _indices.data(), ibSize); _ib->Unmap(0, nullptr);
	_ibv.BufferLocation = _ib->GetGPUVirtualAddress(); _ibv.Format = DXGI_FORMAT_R32_UINT; _ibv.SizeInBytes = (UINT)ibSize;

	Skin(); // 초기 포즈
	return true;
}

void ModelAnimator::SetClipIndex(int i)
{
	if (i < 0 || i >= (int)_clips.size()) return;
	_curClip = i; _animTime = 0.f;
	_animated = LoadClip(_clips[i], _clip);
}

void ModelAnimator::Skin()
{
	if (_vtxCount == 0) return;
	const size_t nb = _bonesData.size();
	const bool anim = _animated && _clip.frameCount > 0 && nb > 0;

	std::vector<XMMATRIX> global, skin;
	if (anim)
	{
		uint32 frame = (uint32)(_animTime * _clip.frameRate) % _clip.frameCount;
		global.resize(nb); skin.resize(nb);
		for (size_t b = 0; b < nb; ++b)
		{
			const LoadedBone& bone = _bonesData[b];
			XMMATRIX matAnim = XMMatrixIdentity();
			auto it = _clip.bones.find(bone.name);
			if (it != _clip.bones.end() && frame < it->second.size())
			{
				const ClipFrameT& k = it->second[frame];
				matAnim = XMMatrixScaling(k.s.x, k.s.y, k.s.z) *
				          XMMatrixRotationQuaternion(XMLoadFloat4(&k.r)) *
				          XMMatrixTranslation(k.t.x, k.t.y, k.t.z);
			}
			XMMATRIX parent = (bone.parent >= 0 && bone.parent < (int32_t)nb) ? global[bone.parent] : XMMatrixIdentity();
			global[b] = matAnim * parent;
			skin[b] = XMMatrixInverse(nullptr, XMLoadFloat4x4(&bone.bind)) * global[b];
		}
	}

	// GameObject 월드행렬 = 배치 트랜스폼 (없으면 단위)
	XMMATRIX M = XMMatrixIdentity();
	if (auto t = GetTransform()) { Matrix wm = t->GetWorldMatrix(); M = XMLoadFloat4x4(&wm); }
	XMVECTOR off = XMVectorSet(_modelOffset.x, _modelOffset.y, _modelOffset.z, 0.f);
	const XMFLOAT3 modelC(0.82f, 0.78f, 0.72f);
	XMFLOAT3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);

	for (uint32 i = 0; i < _vtxCount; ++i)
	{
		const SkinVtx& sv = _skinSrc[i];
		XMVECTOR bp = XMLoadFloat3(&sv.pos), bn = XMLoadFloat3(&sv.nrm), bt = XMLoadFloat3(&sv.tan);
		XMVECTOR p, n, t;
		if (anim)
		{
			p = n = t = XMVectorZero(); float wsum = 0.f;
			for (int j = 0; j < 4; ++j)
			{
				float w = sv.wgt[j]; uint32 bi = sv.idx[j];
				if (w <= 0.f || bi >= nb) continue;
				p = XMVectorAdd(p, XMVectorScale(XMVector3Transform(bp, skin[bi]), w));
				n = XMVectorAdd(n, XMVectorScale(XMVector3TransformNormal(bn, skin[bi]), w));
				t = XMVectorAdd(t, XMVectorScale(XMVector3TransformNormal(bt, skin[bi]), w));
				wsum += w;
			}
			if (wsum < 1e-4f) { p = bp; n = bn; t = bt; }
		}
		else { p = bp; n = bn; t = bt; }

		XMVECTOR wp = XMVectorScale(XMVectorSubtract(p, off), _modelScale);
		wp = XMVector3Transform(wp, M);
		n = XMVector3TransformNormal(n, M);
		t = XMVector3TransformNormal(t, M);
		_world[i].pos = {}; XMStoreFloat3(&_world[i].pos, wp);
		XMStoreFloat3(&_world[i].nrm, XMVector3Normalize(n));
		XMStoreFloat3(&_world[i].tan, XMVector3Normalize(t));
		_world[i].col = modelC; _world[i].uv = sv.uv;
		XMFLOAT3 P = _world[i].pos;
		mn.x = min(mn.x, P.x); mn.y = min(mn.y, P.y); mn.z = min(mn.z, P.z);
		mx.x = max(mx.x, P.x); mx.y = max(mx.y, P.y); mx.z = max(mx.z, P.z);
	}
	_aabbMin = mn; _aabbMax = mx;
	memcpy(_vbMapped, _world.data(), (size_t)_vtxCount * sizeof(Vtx));
}

void ModelAnimator::TransformBoundingBox()
{
	if (_vtxCount == 0) { Renderer::TransformBoundingBox(); return; }
	_boundingBox.Center  = XMFLOAT3((_aabbMin.x + _aabbMax.x) * 0.5f, (_aabbMin.y + _aabbMax.y) * 0.5f, (_aabbMin.z + _aabbMax.z) * 0.5f);
	_boundingBox.Extents = XMFLOAT3((_aabbMax.x - _aabbMin.x) * 0.5f, (_aabbMax.y - _aabbMin.y) * 0.5f, (_aabbMax.z - _aabbMin.z) * 0.5f);
}

void ModelAnimator::Draw(const RenderContext& ctx)
{
	if (!_dev || _vtxCount == 0) return;
	if (_playing) _animTime += DT * _speed;
	Skin(); // 매 프레임 스키닝 (트랜스폼/포즈 반영)

	D3D12Device& d = *_dev;
	auto* cmd = ctx.cmd;
	cmd->SetPipelineState(d._wireframe ? d._wirePSO.Get() : d._pso.Get());
	cmd->SetGraphicsRootSignature(d._rootSig.Get());
	cmd->SetGraphicsRootConstantBufferView(0, d._cb[d._frameIndex]->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(1, d._scene._tlas->GetGPUVirtualAddress());
	cmd->SetGraphicsRootShaderResourceView(2, d._ddgi.ProbesAddr());
	cmd->SetGraphicsRootShaderResourceView(5, d._ddgi.ProbeDepthAddr());
	cmd->SetGraphicsRoot32BitConstant(4, 0u, 0); // useTex=0 (정점색 — Stage 1b 에서 머티리얼)
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &_vbv);
	cmd->IASetIndexBuffer(&_ibv);
	cmd->DrawIndexedInstanced(_idxCount, 1, 0, 0, 0);
}

void ModelAnimator::OnInspectorGUI()
{
	ImGui::SeparatorText("ModelAnimator");
	ImGui::Text("Mesh: %ls  (%u verts, %u bones)", _modelStem.c_str(), _vtxCount, (uint32)_bonesData.size());
	if (!_clips.empty())
	{
		std::string names; // \0 구분 콤보 항목
		for (auto& c : _clips) { names += std::string(fs::path(c).stem().string()); names.push_back('\0'); }
		names.push_back('\0');
		int idx = _curClip;
		if (ImGui::Combo("Clip", &idx, names.c_str())) SetClipIndex(idx);
		ImGui::Text("Frames %u  @ %.0f fps", _clip.frameCount, _clip.frameRate);
	}
	else ImGui::TextDisabled("(no clips)");
	ImGui::Checkbox("Playing", &_playing); ImGui::SameLine();
	ImGui::Checkbox("Loop", &_loop);
	ImGui::DragFloat("Speed", &_speed, 0.02f, 0.f, 4.f);
}
