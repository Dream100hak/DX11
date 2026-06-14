#pragma once
#include "MonoBehaviour.h"

// 디퍼드 데칼 — GameObject 트랜스폼을 박스 볼륨으로, 텍스처를 GBuffer 표면에 투영.
// Camera::Render_Deferred 가 GBuffer fill 직후(라이팅 전) 박스를 그려 albedo 에 블렌드.
class Decal : public MonoBehaviour
{
	using Super = MonoBehaviour;

public:
	Decal();
	virtual ~Decal();

	void Init(const wstring& texPath);
	virtual void OnInspectorGUI() override;

	shared_ptr<class Texture> GetTexture() { return _tex; }
	float GetOpacity() const { return _opacity; }
	const wstring& GetTexPath() const { return _texPath; } // 직렬화용

private:
	shared_ptr<class Texture> _tex;
	wstring _texPath;
	float _opacity = 1.f;
};
