#pragma once
#include "MonoBehaviour.h"

class SkyCubeMap : public MonoBehaviour
{
	using Super = Component;

public:
	SkyCubeMap();
	virtual ~SkyCubeMap();

	void Init(wstring fileName);

	// SkyBox 선택 시 환경맵 교체 콤보 (스카이 + IBL 리베이크)
	void OnInspectorGUI() override;

private:

	shared_ptr<Texture> _cubeMap = nullptr;
	wstring _fileName;

	// Textures 폴더에서 스캔한 큐브맵 .dds 목록 (캐시)
	std::vector<wstring> _cubeFiles;
	bool _scanned = false;
};

