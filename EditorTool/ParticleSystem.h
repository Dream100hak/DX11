#pragma once
#include "Renderer.h"

#define PT_RAIN 1
#define PT_FIRE 2
#define PT_SNOW 3

struct VertexParticle
{
	Vec3 InitialPosW;
	Vec3 InitialVelW;
	Vec2 Size;
	float Age;
	uint32 Type;
};

// HLSL ParticleBuffer (b8) — Fire.hlsl / Rain.hlsl 과 레이아웃 일치 (16바이트 정렬)
struct ParticleBuffer
{
	Vec3  EmitPosW = Vec3::Zero;
	float GameTime = 0.f;
	Vec3  EmitDirW = Vec3::Up;
	float TimeStep = 0.f;
	Vec3  AccelW = Vec3::Zero;
	float EmitInterval = 0.005f;   // 방출 주기 (초)
	float Lifetime = 1.f;          // 입자 수명 (초)
	float InitialSpeed = 1.f;      // 초기 속도 배율 (Rain 은 분산 반경)
	Vec2  ParticleSize = Vec2(1.f, 1.f);
};

// Renderer 파생 — Camera 의 Transparent 큐(Pass 3)에서 HDR sceneColor 로 렌더
// (예전엔 MonoBehaviour + JOB_POST_RENDER 로 스테이지 밖 LDR 백버퍼에 그려서
//  -> Bloom 미적용 + 씬 깊이 차폐 없음 문제)
class ParticleSystem : public Renderer
{
	using Super = Renderer;

public:

	ParticleSystem();
	virtual ~ParticleSystem();

	void OnInspectorGUI() override;

	void Init(int32 type,
		std::vector<wstring> names,
		uint32 maxParticles);

	void Reset();
	void Update() override;
	void Draw(const RenderContext& ctx) override;

	// 인스턴스별 고유 ID (인스턴싱 배칭 방지)
	virtual InstanceID GetInstanceID() override
	{
		return make_pair(reinterpret_cast<uint64>(this), static_cast<uint64>(1));
	}

	// 파티클은 이미터 주변 공간을 차지 — 기본(오브젝트 위치 1m 고정 박스)이면
	// 카메라가 오브젝트 지점을 안 볼 때 시스템 전체가 절두체 컬링된다 (Rain 안 보이던 원인)
	void TransformBoundingBox() override;

private:

	void CreateBuffer();

public:

	void SetEmitPos(const Vec3& emitPosW) { _emitPosW = emitPosW; }
	void SetEmitDir(const Vec3& emitDirW) { _emitDirW = emitDirW; }

	// 씬 직렬화용 접근자
	int32 GetType() const { return _type; }
	uint32 GetMaxParticles() const { return _maxParticles; }
	const std::vector<wstring>& GetTextureNames() const { return _textureNames; }
	Vec3 GetEmitDir() const { return _emitDirW; }

	Vec3  GetAccel() const { return _accelW; }
	float GetEmitInterval() const { return _emitInterval; }
	float GetLifetime() const { return _lifetime; }
	float GetInitialSpeed() const { return _initialSpeed; }
	Vec2  GetParticleSize() const { return _particleSize; }

	void SetAccel(const Vec3& v) { _accelW = v; }
	void SetEmitInterval(float v) { _emitInterval = v; }
	void SetLifetime(float v) { _lifetime = v; }
	void SetInitialSpeed(float v) { _initialSpeed = v; }
	void SetParticleSize(const Vec2& v) { _particleSize = v; }

private:

	// HLSL 셰이더 (FX 제거): SO 패스 + Draw 패스
	shared_ptr<HlslShader> _soShader = nullptr;
	shared_ptr<HlslShader> _drawShader = nullptr;

	shared_ptr<ConstantBuffer<ParticleBuffer>> _particleCB;

	uint32 _maxParticles = 0;
	bool _firstRun;

	float _timeStep;
	float _age;

	int32 _type = 0;

	Vec3 _emitPosW;
	Vec3 _emitDirW;

	// 인스펙터에서 편집 가능한 물리 파라미터 (Init 에서 타입별 기본값 세팅)
	Vec3  _accelW = Vec3::Zero;
	float _emitInterval = 0.005f;
	float _lifetime = 1.f;
	float _initialSpeed = 1.f;
	Vec2  _particleSize = Vec2(1.f, 1.f);

	std::vector<wstring> _textureNames; // Init 인자 보존 (씬 직렬화용)

	ComPtr<ID3D11Buffer> _initVB;
	ComPtr<ID3D11Buffer> _drawVB;
	ComPtr<ID3D11Buffer> _streamOutVB;

	shared_ptr<Texture>  _texArray;
	shared_ptr<Texture> _randomTex;
};
