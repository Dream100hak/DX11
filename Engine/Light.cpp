#include "pch.h"
#include "Light.h"
#include "Camera.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"

Matrix Light::S_MatView = Matrix::Identity;
Matrix Light::S_MatProjection = Matrix::Identity;
Matrix Light::S_Shadow = Matrix::Identity;

Matrix Light::S_CascadeView[CASCADE_COUNT];
Matrix Light::S_CascadeProj[CASCADE_COUNT];
Matrix Light::S_CascadeVPT[CASCADE_COUNT];
float  Light::S_CascadeSplitView[CASCADE_COUNT] = {};

Light::Light() : Component(ComponentType::Light)
{
	SetShadowBoundingSphere();
}

Light::~Light()
{
}

void Light::Start()
{
	if (_type == Directional)
	{
		auto go = GetGameObject();
		go->SetUIPickable(false);
		GetGameObject()->SetIgnoredTransformEdit(true);
	}
}

void Light::Update()
{
	if (_type == Directional)
	{
		UpdateMatrix();
	}
}

void Light::UpdateMatrix()
{
	// 디렉셔널 섀도우 = Cascaded Shadow Maps. 메인 카메라 프러스텀을 분할해 캐스케이드별 ortho 산출.
	UpdateCascades();
}

// 섀도우맵 텍셀 격자 스냅용 (Shadow.hlsli SMAP_SIZE 와 일치시켜야 일렁임 최소)
static const float CSM_SMAP_SIZE = 2048.0f;
// 캐스케이드가 커버할 카메라 거리 (터레인이 거대해 근거리만 그림자 — 원거리는 컷)
static const float CSM_SHADOW_DISTANCE = 250.0f;

void Light::UpdateCascades()
{
	Vec3 lightDir = _desc.direction;
	if (lightDir.LengthSquared() < 1e-6f)
		lightDir = Vec3(0.f, -1.f, 0.f);
	lightDir.Normalize();

	auto scene = CUR_SCENE;
	if (scene == nullptr)
		return;
	auto camObj = scene->GetMainCamera();
	if (camObj == nullptr || camObj->GetCamera() == nullptr)
		return;
	auto cam = camObj->GetCamera();

	// 카메라 기저/파라미터
	Vec3 camPos = camObj->GetTransform()->GetPosition();
	Vec3 fwd = camObj->GetTransform()->GetLook();
	fwd.Normalize();
	Vec3 worldUp = Vec3(0.f, 1.f, 0.f);
	if (fabsf(fwd.Dot(worldUp)) > 0.99f) // 거의 수직이면 다른 업벡터
		worldUp = Vec3(0.f, 0.f, 1.f);
	Vec3 right = worldUp.Cross(fwd); right.Normalize();
	Vec3 up = fwd.Cross(right);      up.Normalize();

	const float fov = cam->GetFov();
	const float aspect = (cam->GetHeight() > 0.f) ? cam->GetWidth() / cam->GetHeight() : 16.f / 9.f;
	const float tanHalfV = tanf(fov * 0.5f);
	const float tanHalfH = tanHalfV * aspect;

	const float camNear = max(cam->GetNear(), 0.05f);
	const float camFar = min(cam->GetFar(), CSM_SHADOW_DISTANCE);

	// 분할 거리 (uniform/log 블렌드)
	const float lambda = 0.7f;
	float splits[CASCADE_COUNT + 1];
	splits[0] = camNear;
	for (int32 i = 1; i <= CASCADE_COUNT; ++i)
	{
		float p = (float)i / (float)CASCADE_COUNT;
		float logS = camNear * powf(camFar / camNear, p);
		float uniS = camNear + (camFar - camNear) * p;
		splits[i] = lambda * logS + (1.f - lambda) * uniS;
	}

	const Matrix T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	for (int32 c = 0; c < CASCADE_COUNT; ++c)
	{
		const float zn = splits[c];
		const float zf = splits[c + 1];

		// 서브 프러스텀 8코너 (월드)
		Vec3 corners[8];
		int32 k = 0;
		for (float z : { zn, zf })
		{
			Vec3 cc = camPos + fwd * z;
			float hh = z * tanHalfV;
			float hw = z * tanHalfH;
			corners[k++] = cc + up * hh + right * hw;
			corners[k++] = cc + up * hh - right * hw;
			corners[k++] = cc - up * hh + right * hw;
			corners[k++] = cc - up * hh - right * hw;
		}

		// 바운딩 스피어 (회전에 안정적 → 일렁임 감소)
		Vec3 center = Vec3::Zero;
		for (auto& p : corners) center += p;
		center /= 8.f;
		float radius = 0.f;
		for (auto& p : corners) radius = max(radius, (p - center).Length());
		radius = ceilf(radius * 16.f) / 16.f; // 약간의 양자화로 흔들림 완화

		// 텍셀 격자 스냅 — 라이트 공간에서 center 를 텍셀 단위로 반올림
		const float texelsPerUnit = CSM_SMAP_SIZE / (radius * 2.f);
		Matrix snapView = ::XMMatrixLookAtLH(center - lightDir * radius, center, up);
		Vec3 cLS = Vec3::Transform(center, snapView);
		cLS.x = floorf(cLS.x * texelsPerUnit) / texelsPerUnit;
		cLS.y = floorf(cLS.y * texelsPerUnit) / texelsPerUnit;
		Vec3 snappedCenter = Vec3::Transform(cLS, snapView.Invert());

		// 캐스터를 더 담기 위해 라이트를 뒤로 충분히 빼고 깊이 범위 확장
		const float zMargin = radius * 2.f;
		Vec3 eye = snappedCenter - lightDir * (radius + zMargin);
		Matrix V = ::XMMatrixLookAtLH(eye, snappedCenter, up);
		Matrix P = ::XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius, 0.f, 2.f * radius + zMargin);

		S_CascadeView[c] = V;
		S_CascadeProj[c] = P;
		S_CascadeVPT[c] = V * P * T;
		S_CascadeSplitView[c] = zf; // 카메라 뷰공간 far 거리 (PS 캐스케이드 선택용)
	}

	// 레거시 단일 섀도우 참조(포워드/프리뷰)는 캐스케이드 0 으로 채워 호환 유지
	S_MatView = S_CascadeView[0];
	S_MatProjection = S_CascadeProj[0];
	S_Shadow = S_CascadeVPT[0];
}

void Light::SetShadowBoundingSphere()
{
	_sceneBounds.Center = _center;
	_sceneBounds.Radius = _radius;
}

void Light::CreateRasterizer()
{
	D3D11_RASTERIZER_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(D3D11_RASTERIZER_DESC));

	depthDesc.FillMode = D3D11_FILL_SOLID;
	depthDesc.CullMode = D3D11_CULL_BACK;
	depthDesc.FrontCounterClockwise = false;
	depthDesc.DepthBias = static_cast<INT>(_depthBias);
	depthDesc.DepthBiasClamp = _depthBiasClamp;
	depthDesc.SlopeScaledDepthBias = _slopeScaledDepthBias;
	depthDesc.DepthClipEnable = true;

	HRESULT hr = DEVICE->CreateRasterizerState(&depthDesc, &_depthRS);
	CHECK(hr);
}
