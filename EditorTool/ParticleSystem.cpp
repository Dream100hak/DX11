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

void ParticleSystem::Init(int32 type, shared_ptr<Shader> shader, shared_ptr<Texture> texArray, uint32 maxParticles)
{
	_type = type;

	ChangeShader(shader);

	_maxParticles = maxParticles;

	_randomTex = make_shared<Texture>();
	_randomTex->CreateRandomTexture1DSRV();
	_texArray = texArray;

	CreateBuffer();
}

void ParticleSystem::ChangeShader(shared_ptr<Shader> shader)
{
	_shader = shader;

	_texArrayBuffer = _shader->GetSRV("TexArray");
	_randomTexBuffer = _shader->GetSRV("RandomTex");

	_gametimeBuffer = _shader->GetScalar("GameTime");
	_timeStepBuffer = _shader->GetScalar("TimeStep");
	_emitPosBuffer = _shader->GetVector("EmitPosW");
	_emitDirBuffer = _shader->GetVector("EmitDirW");
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
	if(_shader == nullptr)
		return;
	
	if (_type == PT_RAIN)
	{
		SetEmitPos(pos);
	}
	else if (_type == PT_FIRE)
	{
		Vec3 worldPos = GetTransform()->GetPosition();
		//SetEmitPos(Vec3(0.0f, 1.0f, 120.0f));
		SetEmitPos(worldPos);
	}
	
	_shader->PushGlobalData(V , P);

	_gametimeBuffer->SetFloat(_age);
	_timeStepBuffer->SetFloat(_timeStep);
	_emitPosBuffer->SetRawValue(&_emitPosW, 0, sizeof(Vec3));
	_emitDirBuffer->SetRawValue(&_emitDirW, 0, sizeof(Vec3));
	
	_texArrayBuffer->SetResource(_texArray->GetComPtr().Get());
	_randomTexBuffer->SetResource(_randomTex->GetComPtr().Get());
	
	uint32 stride = sizeof(VertexParticle);
	uint32 offset = 0;

	// On the first pass, use the initialization VB.  Otherwise, use
	// the VB that contains the current particle list.
	if (_firstRun)
		DCT->IASetVertexBuffers(0, 1, _initVB.GetAddressOf(), &stride, &offset);
	else
		DCT->IASetVertexBuffers(0, 1, _drawVB.GetAddressOf(), &stride, &offset);

	// Draw the current particle list using stream-out only to update them.  
	// The updated vertices are streamed-out to the target VB. 

	DCT->SOSetTargets(1, _streamOutVB.GetAddressOf(), &offset);

	if (_firstRun)
	{
		_shader->DrawParticle(0,0,1);
		_firstRun = false;
	}
	else
	{
		_shader->DrawParticleAuto(0, 0);
	}

	// done streaming-out--unbind the vertex buffer
	ID3D11Buffer* bufferArray[1] = { 0 };
	DCT->SOSetTargets(1, bufferArray, &offset);

	// ping-pong the vertex buffers
	std::swap(_drawVB, _streamOutVB);
	
	// Draw the updated particle system we just streamed-out. 
	//

	DCT->IASetVertexBuffers(0, 1, _drawVB.GetAddressOf(), &stride, &offset);
	_shader->DrawParticleAuto(1, 0);
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


