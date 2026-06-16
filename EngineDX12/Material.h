#pragma once
#include "Common.h"

// DX11 Engine/Material 이식(1차) — PBR 파라미터 + 텍스처 경로. (DX12 SRV 바인딩은 리소스 포팅 후)
class Material
{
public:
	Vec3   _diffuse{ 1.f, 1.f, 1.f }; // 틴트
	float  _metallic = 0.f;
	float  _roughness = 0.5f;
	float  _emissive = 0.f;

	wstring _diffuseTex;
	wstring _normalTex;
	wstring _specTex;
};
