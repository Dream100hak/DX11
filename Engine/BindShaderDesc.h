#pragma once
#include "ConstantBuffer.h"

// ? Forward declaration
class Light;

class Shader;

struct GlobalDesc
{
	Matrix V = Matrix::Identity;
	Matrix P = Matrix::Identity;
	Matrix VP = Matrix::Identity;
	Matrix VInv = Matrix::Identity;
	Matrix Shadow = Matrix::Identity;
	Matrix T = Matrix::Identity;

};

struct TransformDesc
{
	Matrix W = Matrix::Identity;
};

// Light
struct LightDesc
{
	Color ambient = Color(1.f, 1.f, 1.f, 1.f);
	Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
	Color specular = Color(1.f, 1.f, 1.f, 1.f);
	Color emissive = Color(1.f, 1.f, 1.f, 1.f);

	Vec3 direction;
	float intensity = 1.f;
};

// ──────────────────────────────────────────────────────────
// 멀티 라이트 지원 (Directional Light Array)
// ──────────────────────────────────────────────────────────
struct DirectionalLightData
{
	Color diffuse;
	Color ambient;
	float intensity;
	Vec3 direction;
};

struct LightArrayDesc
{
	DirectionalLightData lights[MAX_LIGHTS];
	int32 lightCount = 0;
	Vec3 padding;  // Padding for alignment (총 16바이트 정렬)
};

struct PointLightDesc
{
	Color ambient = Color(1.f, 1.f, 1.f, 1.f);
	Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
	Color specular = Color(1.f, 1.f, 1.f, 1.f);

	Vec3 position;
	float range;

	// Packed into 4D vector: (A0, A1, A2, Pad)
	Vec3 att;
	float padding0; // Pad the last float so we can set an array of lights if we wanted.
};

struct SpotLightDesc
{
	Color ambient = Color(1.f, 1.f, 1.f, 1.f);
	Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
	Color specular = Color(1.f, 1.f, 1.f, 1.f);

	// Packed into 4D vector: (Position, Range)
	Vec3 Position;
	float Range;

	// Packed into 4D vector: (Direction, Spot)
	Vec3 Direction;
	float Spot;

	// Packed into 4D vector: (Att, Pad)
	Vec3 Att;
	float Pad; // Pad the last float so we can set an array of lights if we wanted.
};


struct MaterialDesc
{
	Color ambient  = Color(0.3f, 0.3f, 0.3f, 1.f);
	Color diffuse  = Color(1.f,  1.f,  1.f,  1.f);
	Color specular = Color(0.f,  0.f,  0.f,  1.f);
	Color emissive = Color(0.f,  0.f,  0.f,  1.f);
	// ※ HLSL Common.hlsli MaterialBuffer(b3) 순서와 반드시 일치
	// UseTexture, UseAlphaClip, UseSsao, padding
	int useTexture  = 0; // 0 = 색상만, 1 = 텍스처 샘플
	int useAlphaclip = 0;  // 0 = 클립 없음
	int useSsao     = 0;   // 0 = SSAO 미적용
	int lightCount = 0;   // 16바이트 정렬
};

// Bone
#define MAX_MODEL_TRANSFORMS 1000
#define MAX_MODEL_KEYFRAMES 500
#define MAX_MODEL_INSTANCE 500

struct BoneDesc
{
	Matrix transforms[MAX_MODEL_TRANSFORMS];
};

// Animation
struct KeyframeDesc
{
	int32 animIndex = 0;
	uint32 currFrame = 0;
	uint32 nextFrame = 0;
	float ratio = 0.f;
	float sumTime = 0.f;
	float speed = 1.f;
	Vec2 padding;
};

struct TweenDesc
{
	TweenDesc()
	{
		curr.animIndex = 0;
		next.animIndex = -1;
	}

	void ClearNextAnim()
	{
		next.animIndex = -1;
		next.currFrame = 0;
		next.nextFrame = 0;
		next.sumTime = 0;
		tweenSumTime = 0;
		tweenRatio = 0;
	}

	float tweenDuration = 1.0f;
	float tweenRatio = 0.f;
	float tweenSumTime = 0.f;
	float padding = 0.f;
	KeyframeDesc curr;
	KeyframeDesc next;
};

struct InstancedTweenDesc
{
	TweenDesc tweens[MAX_MODEL_INSTANCE];
};

