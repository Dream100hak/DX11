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

class ParticleSystem : public MonoBehaviour
{
public:
	
	ParticleSystem();
	virtual ~ParticleSystem();

	void OnInspectorGUI() override;

	void Init(int32 type , shared_ptr<Shader> shader,
		std::vector<wstring> names,
		uint32 maxParticles);

	void Reset();
	void Update() override;
	void Draw(Vec3 pos, Matrix V, Matrix P);

private:

	void ChangeShader(shared_ptr<Shader> shader);
	void CreateBuffer();
	
public:

	void SetEmitPos(const Vec3& emitPosW) { _emitPosW = emitPosW; }
	void SetEmitDir(const Vec3& emitDirW) { _emitDirW = emitDirW; }

private:
	
	shared_ptr<Shader> _shader = nullptr;

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

	ComPtr<ID3DX11EffectShaderResourceVariable>  _texArrayBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable>  _randomTexBuffer;

	ComPtr<ID3DX11EffectScalarVariable> _gametimeBuffer;
	ComPtr<ID3DX11EffectScalarVariable> _timeStepBuffer;
	ComPtr<ID3DX11EffectVectorVariable> _emitPosBuffer;
	ComPtr<ID3DX11EffectVectorVariable> _emitDirBuffer;

};

