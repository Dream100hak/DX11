#include "pch.h"
#include "ParticleSystem.h"
#include "Camera.h"
#include "Utils.h"
#include "Light.h"

ParticleSystem::ParticleSystem() : Super(RendererType::Particle)
{
	_firstRun = true;
	_age = 0.0f;

	_emitPosW = Vec3::Zero;
	_emitDirW = Vec3::Up;
}
ParticleSystem::~ParticleSystem()
{
}

void ParticleSystem::OnInspectorGUI()
{
	ImVec4 color = ImVec4(0.85f, 0.94f, 0.f, 1.f);

	ImGui::BeginGroup();
	ImGui::Image(_texArray->GetComPtr().Get(), ImVec2(75, 75));
	ImGui::TextColored(color, "Texture Map");
	ImGui::EndGroup();

	ImGui::DragFloat("AGE", (float*)&_age, 1.0f);

}

void ParticleSystem::Init(int32 type, std::vector<wstring> names , uint32 maxParticles)
{
	_type = type;

	// ??낅퀎 HLSL ?곗씠??(SO ?⑥뒪 + Draw ?⑥뒪)
	if (_type == PT_FIRE)
	{
		_soShader   = RESOURCES->Get<HlslShader>(L"FireSO_HLSL");
		_drawShader = RESOURCES->Get<HlslShader>(L"FireDraw_HLSL");
	}
	else // PT_RAIN (湲곕낯)
	{
		_soShader   = RESOURCES->Get<HlslShader>(L"RainSO_HLSL");
		_drawShader = RESOURCES->Get<HlslShader>(L"RainDraw_HLSL");
	}

	_particleCB = make_shared<ConstantBuffer<ParticleBuffer>>();
	_particleCB->Create();

	_maxParticles = maxParticles;

	_randomTex = make_shared<Texture>();
	_randomTex->CreateRandomTexture1DSRV();

	 _texArray = make_shared<Texture>();
	 _texArray->CreateTexture2DArraySRV(names);

	CreateBuffer();
}

void ParticleSystem::Reset()
{
	_firstRun = true;
	_age = 0.0f;
}

void ParticleSystem::Update()
{
	if (INPUT->GetButtonDown(KEY_TYPE::V))
		Reset();

	_timeStep = DT;
	_age += TIME->GetDeltaTime();

	// ?쒕줈?곕뒗 Camera Pass 3 (Transparent ?? ?먯꽌 Draw(ctx) 濡??섑뻾
}

void ParticleSystem::Draw(const RenderContext& ctx)
{
	// ?뚰떚?댁? 硫붿씤 而щ윭 ?⑥뒪 ?꾩슜 ??GBuffer/洹몃┝??SSAO ?⑥뒪?먯꽑 洹몃━吏 ?딆쓬
	if (ctx.deferredPass || ctx.shadowPass || ctx.ssaoPass)
		return;

	if (_soShader == nullptr || _drawShader == nullptr)
		return;

	Matrix V = ctx.view;
	Matrix P = ctx.proj;

	if (_type == PT_RAIN)
	{
		// 鍮꾨뒗 移대찓??二쇰????대???怨좎젙
		SetEmitPos(CUR_SCENE->GetMainCamera()->GetOrAddTransform()->GetPosition());
	}
	else if (_type == PT_FIRE)
	{
		Vec3 worldPos = GetTransform()->GetPosition();
		SetEmitPos(worldPos);
	}

	// ParticleBuffer (b8) ??SO GS(?앹꽦/?뚮㈇) + Draw ?⑥뒪 怨듭슜
	ParticleBuffer desc;
	desc.EmitPosW = _emitPosW;
	desc.GameTime = _age;
	desc.EmitDirW = _emitDirW;
	desc.TimeStep = _timeStep;
	_particleCB->CopyData(desc);
	auto cb = _particleCB->GetComPtr().Get();

	uint32 stride = sizeof(VertexParticle);
	uint32 offset = 0;

	DCT->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// ?? Pass 1: Stream-Out (?낆옄 ?앹꽦/?뚮㈇, ?섏뒪?곕씪?댁쫰 ?놁쓬) ??
	_soShader->Bind();
	_soShader->SetGSConstantBuffer(8, cb);
	_soShader->SetGSSRV(1, _randomTex->GetComPtr().Get()); // RandomTex (t1)
	RENDER_STATES->BindAllSamplersGS();                    // LinearSampler (s0)

	// 泥??꾨젅?꾩? ?대???1媛쒖쭨由?珥덇린??VB, ?댄썑???꾩옱 ?낆옄 由ъ뒪??VB
	if (_firstRun)
		DCT->IASetVertexBuffers(0, 1, _initVB.GetAddressOf(), &stride, &offset);
	else
		DCT->IASetVertexBuffers(0, 1, _drawVB.GetAddressOf(), &stride, &offset);

	DCT->SOSetTargets(1, _streamOutVB.GetAddressOf(), &offset);

	if (_firstRun)
	{
		// 二쇱쓽: HlslShader::Draw ???좏뤃濡쒖?瑜?TRIANGLELIST 濡?媛뺤젣?섎?濡?吏곸젒 ?몄텧 (POINTLIST ?좎?)
		DCT->Draw(1, 0);
		_firstRun = false;
	}
	else
	{
		_soShader->DrawAuto();
	}

	// SO ?源??댁젣
	ID3D11Buffer* bufferArray[1] = { 0 };
	DCT->SOSetTargets(1, bufferArray, &offset);

	// ?묓릟
	std::swap(_drawVB, _streamOutVB);

	// ?? Pass 2: Draw (諛⑷툑 ?ㅽ듃由쇱븘?껎븳 ?낆옄 ?뚮뜑) ??
	_drawShader->Bind();
	_drawShader->PushGlobalData(V, P); // b0: VP + VInv(移대찓???꾩튂) ??GS ?먯꽌 ?ъ슜
	_drawShader->SetGSConstantBuffer(8, cb);
	_drawShader->SetPSSRV(0, _texArray->GetComPtr().Get()); // TexArray (t0)
	RENDER_STATES->BindAllSamplersPS();

	DCT->IASetVertexBuffers(0, 1, _drawVB.GetAddressOf(), &stride, &offset);
	_drawShader->DrawAuto();

	// GS 媛 ?댄썑 ?쒕줈?곗뿉 ?꾩닔?섏? ?딅룄濡??댁젣 + 釉붾젋???곸뒪 ?곹깭 蹂듭썝
	DCT->GSSetShader(nullptr, nullptr, 0);
	DCT->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	DCT->OMSetDepthStencilState(nullptr, 0);
}


void ParticleSystem::CreateBuffer()
{

	//
	// Create the buffer to kick-off the particle system.
	//

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(VertexParticle) * 1;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	// The initial particle emitter has type 0 and age 0.  The rest
	// of the particle attributes do not apply to an emitter.
	VertexParticle p;
	ZeroMemory(&p, sizeof(VertexParticle));
	p.Age = 0.0f;
	p.Type = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &p;

	HRESULT hr;
	hr = DEVICE->CreateBuffer(&vbd, &vinitData, _initVB.GetAddressOf());
	CHECK(hr);

	//
	// Create the ping-pong buffers for stream-out and drawing.
	//
	vbd.ByteWidth = sizeof(VertexParticle) * _maxParticles;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_STREAM_OUTPUT;

	hr = DEVICE->CreateBuffer(&vbd, 0, _drawVB.GetAddressOf());
	CHECK(hr);
	hr = DEVICE->CreateBuffer(&vbd, 0, _streamOutVB.GetAddressOf());
	CHECK(hr);


}


