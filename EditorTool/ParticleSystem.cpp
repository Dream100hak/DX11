#include "pch.h"
#include "ParticleSystem.h"
#include "Camera.h"
#include "Utils.h"
#include "Light.h"

ParticleSystem::ParticleSystem()
{
	SetBehaviorName(Utils::ToWString(Utils::GetClassNameEX<ParticleSystem>()));
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

	// 타입별 HLSL 셰이더 (SO 패스 + Draw 패스)
	if (_type == PT_FIRE)
	{
		_soShader   = RESOURCES->Get<HlslShader>(L"FireSO_HLSL");
		_drawShader = RESOURCES->Get<HlslShader>(L"FireDraw_HLSL");
	}
	else // PT_RAIN (기본)
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

	shared_ptr<Camera> camera = CUR_SCENE->GetMainCamera()->GetCamera();
	Matrix V = camera->GetViewMatrix();
	Matrix P = camera->GetProjectionMatrix();
	Vec3 pos = CUR_SCENE->GetMainCamera()->GetOrAddTransform()->GetPosition();
	
	JOB_POST_RENDER->DoPush([=]()
	{
		Draw(pos, V , P);
	});
}

void ParticleSystem::Draw(Vec3 pos, Matrix V, Matrix P)
{
	if (_soShader == nullptr || _drawShader == nullptr)
		return;

	if (_type == PT_RAIN)
	{
		SetEmitPos(pos);
	}
	else if (_type == PT_FIRE)
	{
		Vec3 worldPos = GetTransform()->GetPosition();
		SetEmitPos(worldPos);
	}

	// ParticleBuffer (b8) — SO GS(생성/소멸) + Draw 패스 공용
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

	// ── Pass 1: Stream-Out (입자 생성/소멸, 래스터라이즈 없음) ──
	_soShader->Bind();
	_soShader->SetGSConstantBuffer(8, cb);
	_soShader->SetGSSRV(1, _randomTex->GetComPtr().Get()); // RandomTex (t1)
	RENDER_STATES->BindAllSamplersGS();                    // LinearSampler (s0)

	// 첫 프레임은 이미터 1개짜리 초기화 VB, 이후엔 현재 입자 리스트 VB
	if (_firstRun)
		DCT->IASetVertexBuffers(0, 1, _initVB.GetAddressOf(), &stride, &offset);
	else
		DCT->IASetVertexBuffers(0, 1, _drawVB.GetAddressOf(), &stride, &offset);

	DCT->SOSetTargets(1, _streamOutVB.GetAddressOf(), &offset);

	if (_firstRun)
	{
		// 주의: HlslShader::Draw 는 토폴로지를 TRIANGLELIST 로 강제하므로 직접 호출 (POINTLIST 유지)
		DCT->Draw(1, 0);
		_firstRun = false;
	}
	else
	{
		_soShader->DrawAuto();
	}

	// SO 타깃 해제
	ID3D11Buffer* bufferArray[1] = { 0 };
	DCT->SOSetTargets(1, bufferArray, &offset);

	// 핑퐁
	std::swap(_drawVB, _streamOutVB);

	// ── Pass 2: Draw (방금 스트림아웃한 입자 렌더) ──
	_drawShader->Bind();
	_drawShader->PushGlobalData(V, P); // b0: VP + VInv(카메라 위치) — GS 에서 사용
	_drawShader->SetGSConstantBuffer(8, cb);
	_drawShader->SetPSSRV(0, _texArray->GetComPtr().Get()); // TexArray (t0)
	RENDER_STATES->BindAllSamplersPS();

	DCT->IASetVertexBuffers(0, 1, _drawVB.GetAddressOf(), &stride, &offset);
	_drawShader->DrawAuto();

	// GS 가 이후 드로우에 누수되지 않도록 해제 + 블렌드/뎁스 상태 복원
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


