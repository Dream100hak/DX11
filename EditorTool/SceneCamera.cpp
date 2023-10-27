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


	// ¾ê¸¸ À©µµ¿ì ¸Þ¼¼Áö·Õ 
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

	float moveDistance = 0.6f * static_cast<float>(scrollAmount);
	camPos += camLookDir * moveDistance;

	GetTransform()->SetPosition(camPos);
}

