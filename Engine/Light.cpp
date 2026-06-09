#include "pch.h"
#include "Light.h"
#include "Scene.h"
#include "Camera.h"

Matrix Light::S_MatView = Matrix::Identity;
Matrix Light::S_MatProjection = Matrix::Identity;
Matrix Light::S_Shadow = Matrix::Identity;

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
	// Only the first "main" light casts a shadow.
	Vec3 lightDir = _desc.direction;
	lightDir.Normalize();

	const float radius = _sceneBounds.Radius;

	// 그림자 스피어 중심을 카메라 포커스 지점으로 이동.
	// 고정 center(0,0,0)일 때 반경 밖 오브젝트가 그림자 캐스팅/수신을 못 하던 문제 해결.
	if (_autoFitShadow)
	{
		if (auto scene = SCENE->GetCurrentScene())
		{
			if (auto camGo = scene->GetMainCamera())
			{
				Vec3 camPos = camGo->GetTransform()->GetPosition();
				Vec3 look   = camGo->GetTransform()->GetLook();
				_sceneBounds.Center = camPos + look * (radius * 0.5f);
			}
		}
	}

	Vec3 targetPos = _sceneBounds.Center;

	// 텍셀 스냅: 라이트 공간 XY를 셰도우맵 텍셀 단위로 고정.
	// auto-fit 으로 중심이 매 프레임 움직이므로, 스냅 없이는 그림자 가장자리가 흔들린다(shimmer).
	{
		Matrix rotView = ::XMMatrixLookAtLH(Vec3::Zero, lightDir, Vec3::Up);
		Vec3 t = Vec3::Transform(targetPos, rotView);
		const float texel = (2.f * radius) / SHADOW_MAP_SIZE;
		t.x = floorf(t.x / texel) * texel;
		t.y = floorf(t.y / texel) * texel;
		targetPos = Vec3::Transform(t, rotView.Invert());
	}

	// 기존엔 lightPos 가 원점 기준(-2r*dir)이라 center 이동 시 어긋남 — target 기준으로 수정
	Vec3 lightPos = targetPos - 2.0f * radius * lightDir;

	Matrix V = ::XMMatrixLookAtLH(lightPos, targetPos, Vec3::Up);

	S_MatView = V;
	// Transform bounding sphere to light space.

	Vec3 sphereCenter = Vec3::Transform(targetPos, V);

	// Ortho frustum in light space encloses scene.
	float l = sphereCenter.x - radius;
	float b = sphereCenter.y - radius;
	float n = sphereCenter.z - radius;
	float r = sphereCenter.x + radius;
	float t = sphereCenter.y + radius;
	float f = sphereCenter.z + radius;
	Matrix P = ::XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
	S_MatProjection = P;

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	Matrix T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	S_Shadow = V * P * T;
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
