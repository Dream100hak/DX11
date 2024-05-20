//#pragma once
//class Effect
//{
//public:
//	Effect(const std::wstring& filename);
//	virtual ~Effect();
//
//private:
//	Effect(const Effect& rhs);
//	Effect& operator=(const Effect& rhs);
//
//protected:
//	ComPtr<ID3DX11Effect> _fx;
//
//};
//
//class ParticleEffect : public Effect
//{
//public:
//	ParticleEffect(const std::wstring& filename);
//	~ParticleEffect();
//
//	void SetViewProj(CXMMATRIX M) { VP->SetMatrix(reinterpret_cast<const float*>(&M)); }
//
//	void SetGameTime(float f) { GameTime->SetFloat(f); }
//	void SetTimeStep(float f) { TimeStep->SetFloat(f); }
//	
//	void SetEyePosW(const XMFLOAT3& v) { EyePosW->SetRawValue(&v, 0, sizeof(XMFLOAT3)); }
//	void SetEmitPosW(const XMFLOAT3& v) { EmitPosW->SetRawValue(&v, 0, sizeof(XMFLOAT3)); }
//	void SetEmitDirW(const XMFLOAT3& v) { EmitDirW->SetRawValue(&v, 0, sizeof(XMFLOAT3)); }
//
//	void SetTexArray(ID3D11ShaderResourceView* tex) { TexArray->SetResource(tex); }
//	void SetRandomTex(ID3D11ShaderResourceView* tex) { RandomTex->SetResource(tex); }
//
//	ComPtr<ID3DX11EffectTechnique> StreamOutTech;
//	ComPtr<ID3DX11EffectTechnique> DrawTech;
//
//	ComPtr<ID3DX11EffectMatrixVariable> VP;
//	ComPtr<ID3DX11EffectScalarVariable> GameTime;
//	ComPtr<ID3DX11EffectScalarVariable> TimeStep;
//	ComPtr<ID3DX11EffectVectorVariable> EyePosW;
//	ComPtr<ID3DX11EffectVectorVariable> EmitPosW;
//	ComPtr<ID3DX11EffectVectorVariable> EmitDirW;
//	ComPtr<ID3DX11EffectShaderResourceVariable> TexArray;
//	ComPtr<ID3DX11EffectShaderResourceVariable> RandomTex;
//};
//
//class Effects
//{
//public:
//	static void InitAll();
//	static void DestroyAll();
//
//	static shared_ptr<ParticleEffect> RainFX;
//};
//
//
//class InputLayoutDesc
//{
//public:
//	// Init like const int A::a[4] = {0, 1, 2, 3}; in .cpp file.
//	static const D3D11_INPUT_ELEMENT_DESC Particles[5];
//};
//
//class InputLayouts
//{
//public:
//	static void InitAll();
//	static void DestroyAll();
//
//	static ComPtr<ID3D11InputLayout> Particle;
//};