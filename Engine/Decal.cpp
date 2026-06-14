#include "pch.h"
#include "Decal.h"
#include "Texture.h"

Decal::Decal() : Super()
{
}

Decal::~Decal()
{
}

void Decal::Init(const wstring& texPath)
{
	_texPath = texPath;
	if (texPath.empty() == false)
		_tex = RESOURCES->Load<Texture>(texPath, texPath);
}

void Decal::OnInspectorGUI()
{
	Super::OnInspectorGUI();

	ImGui::DragFloat("Opacity", &_opacity, 0.01f, 0.f, 1.f);
	ImGui::TextDisabled("Transform(박스)로 위치/크기/회전. Y축 아래로 투영.");

	if (_tex != nullptr && _tex->GetComPtr() != nullptr)
		ImGui::Image(_tex->GetComPtr().Get(), ImVec2(80, 80));
}
