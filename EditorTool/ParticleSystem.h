#pragma once
#include "MonoBehaviour.h"

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

class ParticleSystem : public MonoBehaviour
{
public:

	ParticleSystem();
	virtual ~ParticleSystem();

	void OnInspectorGUI() override;

	void Init(int32 type,
		std::vector<wstring> names,
		uint32 maxParticles);

	void Reset();
	void Update() override;
	void Draw(Vec3 pos, Matrix V, Matrix P);

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

