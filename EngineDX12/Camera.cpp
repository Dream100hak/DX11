#include "Camera.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Define.h"
#include <algorithm>

using namespace DirectX;

// 현재 씬의 렌더러를 렌더 큐별로 분류 (DX11 Engine Camera::SortGameObject 대응 1차).
// 큐: <2000 Background / 2000~2999 Opaque(+AlphaTest 2450) / >=3000 Transparent·Overlay.
// (버킷 내 거리 정렬은 추후 — 우선 패스 분리만)
void Camera::SortGameObject()
{
	_vecBackground.clear();
	_vecOpaque.clear();
	_vecTransparent.clear();

	auto scene = CUR_SCENE;
	if (!scene) return;

	// 절두체 갱신 (VP) — 추후 컬링에 사용
	{
		XMMATRIX vp = XMLoadFloat4x4(&_matView) * XMLoadFloat4x4(&_matProjection);
		Matrix vpf; XMStoreFloat4x4(&vpf, vp);
		_frustum.Update(vpf);
	}

	// 카메라 월드 위치 (뷰 역행렬 평행이동) — 거리 정렬 기준
	XMVECTOR camPos = XMMatrixInverse(nullptr, XMLoadFloat4x4(&_matView)).r[3];

	for (auto& obj : scene->GetObjects())
	{
		if (!obj) continue;
		auto r = obj->GetRenderer();
		if (!r) continue;

		r->TransformBoundingBox(); // 월드 AABB 갱신

		int32 q = static_cast<int32>(r->GetRenderQueue());
		if (q < static_cast<int32>(RenderQueue::Opaque))            _vecBackground.push_back(obj);
		else if (q < static_cast<int32>(RenderQueue::Transparent))  _vecOpaque.push_back(obj);
		else                                                        _vecTransparent.push_back(obj);
	}

	// 거리 계산 (바운딩 박스 중심 → 카메라)
	auto dist2 = [&](const shared_ptr<GameObject>& o)
	{
		const XMFLOAT3& c = o->GetRenderer()->GetBoundingBox().Center;
		XMVECTOR d = XMVectorSubtract(XMVectorSet(c.x, c.y, c.z, 0.f), camPos);
		return XMVectorGetX(XMVector3LengthSq(d));
	};
	// 불투명: 가까운 것부터 (Early-Z) / 반투명: 먼 것부터 (알파 블렌딩)
	std::sort(_vecOpaque.begin(), _vecOpaque.end(),      [&](auto& a, auto& b) { return dist2(a) < dist2(b); });
	std::sort(_vecTransparent.begin(), _vecTransparent.end(), [&](auto& a, auto& b) { return dist2(a) > dist2(b); });
}
