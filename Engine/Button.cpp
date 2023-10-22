#include "pch.h"
#include "Button.h"
#include "MeshRenderer.h"
#include "Material.h"


Button::Button() : Super(ComponentType::Button)
{

}

Button::~Button()
{

}
bool Button::Picked(POINT screenPos)
{
	return ::PtInRect(&_rect, screenPos);
}

void Button::Create(Vec2 screenPos, Vec2 size, shared_ptr<class Material> material)
{
	auto go = _gameObject.lock();

	float height = GRAPHICS->GetViewport().GetHeight();
	float width = GRAPHICS->GetViewport().GetWidth();

	float x = screenPos.x - width / 2;
	float y = height / 2 - screenPos.y;
	Vec3 position = Vec3(x, y, 0);

	go->GetOrAddTransform()->SetPosition(position);
	go->GetOrAddTransform()->SetScale(Vec3(size.x, size.y, 1));

	go->SetLayerIndex(UI);

	if (go->GetMeshRenderer() == nullptr)
		go->AddComponent(make_shared<MeshRenderer>());

	go->GetMeshRenderer()->SetMaterial(material);

	auto mesh = RESOURCES->Get<Mesh>(L"Quad");
	go->GetMeshRenderer()->SetMesh(mesh);
	go->GetMeshRenderer()->SetPass(0);

	// Picking
	_rect.left = screenPos.x - size.x / 2;
	_rect.right = screenPos.x + size.x / 2;
	_rect.top = screenPos.y - size.y / 2;
	_rect.bottom = screenPos.y + size.y / 2;
}

void Button::AddOnClickedEvent(std::function<void(void)> func)
{
	_onClicked = func;
}

void Button::InvokeOnClicked()
{
	if (_onClicked)
		_onClicked();
}