#pragma once
#include "ConstantBuffer.h"

// ? Forward declaration
class Light;

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

// 멀티라이트 배열 구조 정의
// 멀티라이트 배열 (Directional Light Array)
// GPU 라이트 데이터
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
	// 대응 HLSL Common.hlsli MaterialBuffer(b3) 레이아웃과 일치
	// UseTexture, UseAlphaClip, UseSsao, padding
	int useTexture  = 0; // 0 = 미사용, 1 = 텍스처 사용
	int useAlphaclip = 0;  // 0 = 클리핑 비활성, 1 = 클리핑 활성
	int useSsao     = 0;   // 0 = SSAO 비활성, 1 = SSAO 활성
	int lightCount = 0;

	// 대응 HLSL Common.hlsli MaterialBuffer(b3) 레이아웃과 일치
	float roughness = 0.5f;
	float metallic  = 0.f;
	Vec2  pbrPadding;   // 16바이트 정렬
};

// 포스트프로세싱(b8) 버퍼 HLSL PostProcess.hlsl / Fxaa.hlsl PostProcessBuffer와 일치
struct PostProcessDesc
{
	Vec2  texelSize;              // 1/width, 1/height
	float bloomThreshold = 1.0f;  // 밝기 판단 임계값(HDR)
	float bloomIntensity = 0.6f;  // BrightPass 추출 배율
};

// IBL 베이킹(b8) 버퍼 HLSL IblBake.hlsl IblBakeBuffer와 일치
struct IblBakeDesc
{
	int32 faceIndex = 0;   // 큐브맵 face 0..5
	float roughness = 0.f; // prefilter mip 별 roughness
	Vec2  padding;
};

// IBL 런타임(b8, DeferredLighting) 버퍼 HLSL IblBuffer와 일치
struct IblDesc
{
	int32 useIbl = 0;
	float envIntensity = 1.f;
	Vec2  padding;
};

// 패스 뷰어(b8) 버퍼 HLSL PassViewer.hlsl PassViewerBuffer와 일치
struct PassViewerDesc
{
	int32 viewMode = 0; // 0=Final 1=Albedo 2=Normal 3=Roughness 4=Metallic 5=WorldPos 6=Depth 7=SSAO 8=Shadow
	Vec3  padding;
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

