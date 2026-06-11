#include "pch.h"
#include "SkyCubeMap.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "HlslShader.h"
#include "Utils.h"
#include "Ibl.h"
#include <filesystem>

SkyCubeMap::SkyCubeMap()
{
	SetBehaviorName(Utils::ToWString(Utils::GetClassNameEX<SkyCubeMap>()));
}

SkyCubeMap::~SkyCubeMap()
{
}

void SkyCubeMap::Init(wstring fileName)
{
	_fileName = fileName;
	// 키를 파일 경로로 — 예전 고정 키("CubeMap")는 환경맵 교체 시 캐시된 이전 맵을 돌려줬음
	_cubeMap = RESOURCES->Load<Texture>(fileName, fileName);

	// FX11 01. CubeMap.fx → HLSL CubeMap.hlsl. 큐브 텍스처는 머티리얼 DiffuseMap(t0)으로 전달.
	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(RESOURCES->Get<HlslShader>(L"CubeMap_HLSL"));
	material->SetDiffuseMap(_cubeMap);
	material->SetRenderQueue(RenderQueue::Background); // 불투명 이후 (디퍼드 Pass 3)

	if (GetGameObject()->GetMeshRenderer() == nullptr)
	{
		GetGameObject()->AddComponent(make_shared<MeshRenderer>());
		{
			auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
			GetGameObject()->GetMeshRenderer()->SetMesh(mesh);
		}
	}

	GetGameObject()->GetMeshRenderer()->SetMaterial(material);
}

// SkyBox 선택 시 인스펙터 — 환경맵 콤보 (변경 즉시 스카이 교체 + IBL 리베이크)
void SkyCubeMap::OnInspectorGUI()
{
	// Textures 폴더에서 큐브맵 .dds 스캔 (1회 캐시)
	if (!_scanned)
	{
		_scanned = true;
		const std::filesystem::path dir = L"../Resources/Assets/Textures";
		std::error_code ec;
		for (auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
		{
			if (entry.path().extension() != L".dds")
				continue;

			DirectX::TexMetadata meta;
			if (SUCCEEDED(DirectX::GetMetadataFromDDSFile(entry.path().wstring().c_str(), DirectX::DDS_FLAGS_NONE, meta))
				&& meta.IsCubemap())
			{
				_cubeFiles.push_back(entry.path().wstring());
			}
		}
	}

	ImGui::SeparatorText("Environment Map");

	string currentName = Utils::ToString(std::filesystem::path(_fileName).filename().wstring());
	if (ImGui::BeginCombo("Cube Map", currentName.c_str()))
	{
		for (const wstring& file : _cubeFiles)
		{
			string label = Utils::ToString(std::filesystem::path(file).filename().wstring());
			bool selected = (file == _fileName);
			if (ImGui::Selectable(label.c_str(), selected) && !selected)
			{
				Init(file);      // 스카이박스 교체
				Ibl::Init(file); // IBL 리베이크 (irradiance/prefiltered — BRDF LUT 는 환경 무관이지만 함께 갱신)
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Rebake IBL"))
		Ibl::Init(_fileName);

	ImGui::SameLine();
	ImGui::TextDisabled(Ibl::IsReady() ? "IBL: ready" : "IBL: off");
}
