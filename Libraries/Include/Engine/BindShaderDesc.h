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

// ��������������������������������������������������������������������������������������������������������������������
// ��Ƽ ����Ʈ ���� (Directional Light Array)
// ��������������������������������������������������������������������������������������������������������������������
// GPU light data (Directional / Point / Spot unified)
// type: 0=Directional, 1=Point, 2=Spot
struct LightData
{
	Color diffuse;
	Color ambient;
	Vec3  direction;
	float intensity;
	Vec3  position;
	float range;
	Vec3  attenuation;   // (constant, linear, quadratic)
	float spotAngle;     // cos(half-angle)
	int32 type;          // 0=Directional, 1=Point, 2=Spot
	Vec3  padding;
};

struct LightArrayDesc
{
	LightData lights[MAX_LIGHTS];
	int32 lightCount = 0;
	Vec3 padding;
};


struct MaterialDesc
{
	Color ambient  = Color(0.3f, 0.3f, 0.3f, 1.f);
	Color diffuse  = Color(1.f,  1.f,  1.f,  1.f);
	Color specular = Color(0.f,  0.f,  0.f,  1.f);
	Color emissive = Color(0.f,  0.f,  0.f,  1.f);
	// �� HLSL Common.hlsli MaterialBuffer(b3) ������ �ݵ�� ��ġ
	// UseTexture, UseAlphaClip, UseSsao, padding
	int useTexture  = 0; // 0 = ����, 1 = �ؽ�ó ����
	int useAlphaclip = 0;  // 0 = Ŭ�� ����
	int useSsao     = 0;   // 0 = SSAO ������
	int lightCount = 0;   // 16����Ʈ ����
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

