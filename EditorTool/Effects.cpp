#include "pch.h"
//#include "Effects.h"
//#include <fstream>
//
//Effect::Effect(const std::wstring& filename)
//{
//	WORD shaderFlags = 0;
//#if defined( DEBUG ) || defined( _DEBUG )
//	shaderFlags |= D3D10_SHADER_DEBUG;
//	shaderFlags |= D3D10_SHADER_SKIP_OPTIMIZATION;
//#endif
//
//	ComPtr<ID3D10Blob> compiledShader = 0;
//	ComPtr<ID3D10Blob> compilationMsgs = 0;
//
//	HRESULT hr = ::D3DCompileFromFile(filename.c_str(), 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, 0, "fx_5_0", shaderFlags, 0, compiledShader.GetAddressOf(), compilationMsgs.GetAddressOf());
//
//	// compilationMsgs can store errors or warnings.
//	if (FAILED(hr))
//	{
//		if (compilationMsgs != 0)
//			::MessageBoxA(0, (char*)compilationMsgs->GetBufferPointer(), 0, 0);
//
//		assert(false);
//	}
//
//	CHECK(::D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), 0, DEVICE.Get(), _fx.GetAddressOf()));
//}
//Effect::~Effect()
//{
//
//}
//
//Effect::Effect(const Effect& rhs)
//{
//
//}
//
//
//void Effects::InitAll()
//{
//	RainFX = make_shared<ParticleEffect>(L"../Shaders/01. Rain.fx");
//}
//
//void Effects::DestroyAll()
//{
//
//}
//
//std::shared_ptr<ParticleEffect> Effects::RainFX;
//
//ParticleEffect::ParticleEffect(const std::wstring& filename) 
// :Effect(filename)
//{
//	StreamOutTech = _fx->GetTechniqueByName("StreamOutTech");
//	DrawTech = _fx->GetTechniqueByName("DrawTech");
//
//	VP = _fx->GetVariableByName("VP")->AsMatrix();
//	GameTime = _fx->GetVariableByName("GameTime")->AsScalar();
//	TimeStep = _fx->GetVariableByName("TimeStep")->AsScalar();
//	EyePosW = _fx->GetVariableByName("EyePosW")->AsVector();
//	EmitPosW = _fx->GetVariableByName("EmitPosW")->AsVector();
//	EmitDirW = _fx->GetVariableByName("EmitDirW")->AsVector();
//	TexArray = _fx->GetVariableByName("TexArray")->AsShaderResource();
//	RandomTex = _fx->GetVariableByName("RandomTex")->AsShaderResource();
//}
//
//ParticleEffect::~ParticleEffect()
//{
//
//}
//
//
//ComPtr<ID3D11InputLayout> InputLayouts::Particle;
//
//void InputLayouts::InitAll()
//{
//	D3DX11_PASS_DESC passDesc;
//
//	Effects::RainFX->StreamOutTech->GetPassByIndex(0)->GetDesc(&passDesc);
//	HRESULT hr = DEVICE->CreateInputLayout(InputLayoutDesc::Particles, 5, passDesc.pIAInputSignature,
//		passDesc.IAInputSignatureSize, &Particle);
//
//	CHECK(hr);
//}
//
//void InputLayouts::DestroyAll()
//{
//
//}
//
//
//
//const D3D11_INPUT_ELEMENT_DESC InputLayoutDesc::Particles[5]
//{
//	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
//	{"VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
//	{"SIZE",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
//	{"AGE",      0, DXGI_FORMAT_R32_FLOAT,       0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
//	{"TYPE",     0, DXGI_FORMAT_R32_UINT,        0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
//};
