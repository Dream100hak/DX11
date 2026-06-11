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

// HLSL ParticleBuffer (b8) ??Fire.hlsl / Rain.hlsl 怨??덉씠?꾩썐 ?쇱튂
struct ParticleBuffer
{
	Vec3  EmitPosW = Vec3::Zero;
	float GameTime = 0.f;
	Vec3  EmitDirW = Vec3::Up;
	float TimeStep = 0.f;
};

// Renderer ?뚯깮 ??Camera ??Transparent ??Pass 3)?먯꽌 HDR sceneColor 濡??뚮뜑
// (?덉쟾??MonoBehaviour + JOB_POST_RENDER 濡??ㅻℓ????LDR 諛깅쾭?쇱뿉 洹몃졇??
//  -> Bloom 誘몄쟻??+ ??源딆씠 李⑦룓 ?놁쓬 臾몄젣)
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

	// ?대??곕퀎 怨좎쑀 ID (?몄뒪?댁떛 諛곗묶 諛⑹?)
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

	// HLSL ?곗씠??(FX ?쒓굅): SO ?⑥뒪 + Draw ?⑥뒪
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

