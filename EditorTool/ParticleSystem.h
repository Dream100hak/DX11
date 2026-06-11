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

// HLSL ParticleBuffer (b8) — Fire.hlsl / Rain.hlsl 과 레이아웃 일치
struct ParticleBuffer
{
	Vec3  EmitPosW = Vec3::Zero;
	float GameTime = 0.f;
	Vec3  EmitDirW = Vec3::Up;
	float TimeStep = 0.f;
};

// Renderer 파생 — Camera 의 Transparent 큐(Pass 3)에서 HDR sceneColor 로 렌더
// (예전엔 MonoBehaviour + JOB_POST_RENDER 로 톤매핑 뒤 LDR 백버퍼에 그렸음
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

	// 이미터별 고유 ID (인스턴싱 배칭 방지)
	virtual InstanceID GetInstanceID() override
	{
		return make_pair(reinterpret_cast<uint64>(this), static_cast<uint64>(1));
	}

private:

	void CreateBuffer();

public:

	void SetEmitPos(const Vec3& emitPosW) { _emitPosW = emitPosW; }
	void SetEmitDir(const Vec3& emitDirW) { _emitDirW = emitDirW; }

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

	ComPtr<ID3D11Buffer> _initVB;
	ComPtr<ID3D11Buffer> _drawVB;
	ComPtr<ID3D11Buffer> _streamOutVB;

	shared_ptr<Texture>  _texArray;
	shared_ptr<Texture> _randomTex;
};

