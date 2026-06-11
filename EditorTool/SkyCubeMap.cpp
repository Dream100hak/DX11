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
	// ?ㅻ? ?뚯씪 寃쎈줈濡????덉쟾 怨좎젙 ??"CubeMap")???섍꼍留?援먯껜 ??罹먯떆???댁쟾 留듭쓣 ?뚮젮以ъ쓬
	_cubeMap = RESOURCES->Load<Texture>(fileName, fileName);

	// FX11 01. CubeMap.fx ??HLSL CubeMap.hlsl. ?먮툕 ?띿뒪泥섎뒗 癒명떚由ъ뼹 DiffuseMap(t0)?쇰줈 ?꾨떖.
	shared_ptr<Material> material = make_shared<Material>();
	material->SetHlslShader(RESOURCES->Get<HlslShader>(L"CubeMap_HLSL"));
	material->SetDiffuseMap(_cubeMap);
	material->SetRenderQueue(RenderQueue::Background); // 遺덊닾紐??댄썑 (?뷀띁??Pass 3)

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

// SkyBox ?좏깮 ???몄뒪?숉꽣 ???섍꼍留?肄ㅻ낫 (蹂寃?利됱떆 ?ㅼ뭅??援먯껜 + IBL 由щ쿋?댄겕)
void SkyCubeMap::OnInspectorGUI()
{
	// Textures ?대뜑?먯꽌 ?먮툕留?.dds ?ㅼ틪 (1??罹먯떆)
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
				Init(file);      // ?ㅼ뭅?대컯??援먯껜
				Ibl::Init(file); // IBL 由щ쿋?댄겕 (irradiance/prefiltered ??BRDF LUT ???섍꼍 臾닿??댁?留??④퍡 媛깆떊)
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
