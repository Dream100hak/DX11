#include "pch.h"
#include "SceneCamera.h"
#include "GameObject.h"
#include "Camera.h"
#include "Transform.h"

void SceneCamera::Start()
{

}

void SceneCamera::Update()
{
	float dt = TIME->GetDeltaTime();

	Vec3 pos = GetTransform()->GetPosition();

	if (INPUT->GetButton(KEY_TYPE::W))
		pos += GetTransform()->GetLook() * _speed * dt;

	if (INPUT->GetButton(KEY_TYPE::S))
		pos -= GetTransform()->GetLook() * _speed * dt;

	if (INPUT->GetButton(KEY_TYPE::A))
		pos -= GetTransform()->GetRight() * _speed * dt;

	if (INPUT->GetButton(KEY_TYPE::D))
		pos += GetTransform()->GetRight() * _speed * dt;

	GetTransform()->SetPosition(pos);


	// 얘만 윈도우 메세지롱 
	if (INPUT->GetButton(KEY_TYPE::RBUTTON))
	{
		RotateCam();
	}
}

void SceneCamera::RotateCam()
{
	POINT pos = INPUT->GetPrevMousePos();

	if (INPUT->GetButton(KEY_TYPE::RBUTTON))
	{
		POINT currentMousePos = INPUT->GetMousePos();

		float dx = XMConvertToRadians(0.1f * static_cast<float>(currentMousePos.x - pos.x));
		float dy = XMConvertToRadians(0.1f * static_cast<float>(currentMousePos.y - pos.y));

		GetTransform()->Pitch(dy);
		GetTransform()->Yaw(dx);
	}
}

void SceneCamera::MoveCam(int32 scrollAmount)
{
	Vec3 camPos = GetTransform()->GetPosition();
	Vec3 camLookDir = GetTransform()->GetLook();

	// 이동 거리 계산 (양수 값은 앞으로, 음수 값은 뒤로 이동)
	float moveDistance = 0.6f * static_cast<float>(scrollAmount);

	// 카메라 위치 조절
	camPos += camLookDir * moveDistance;

	GetTransform()->SetPosition(camPos);
}

