#pragma once
#include "MonoBehaviour.h"

class SkyCubeMap : public MonoBehaviour
{
	using Super = Component;

public:
	SkyCubeMap();
	virtual ~SkyCubeMap();

	void Init(wstring fileName);

	// SkyBox ?좏깮 ???섍꼍留?援먯껜 肄ㅻ낫 (?ㅼ뭅??+ IBL 由щ쿋?댄겕)
	void OnInspectorGUI() override;

private:

	shared_ptr<Texture> _cubeMap = nullptr;
	wstring _fileName;

	// Textures ?대뜑?먯꽌 ?ㅼ틪???먮툕留?.dds 紐⑸줉 (罹먯떆)
	std::vector<wstring> _cubeFiles;
	bool _scanned = false;
};

