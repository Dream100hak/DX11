#pragma once

class ShadowMap;
class PunctualShadowMap;
class Ssao;

class TextureManager
{
	DECLARE_SINGLE(TextureManager);

public:
	void Init();
	void Update();

	void DrawTextureMap();

	shared_ptr<ShadowMap>& GetShadowMap() { return _smap; }
	shared_ptr<PunctualShadowMap>& GetPunctualShadowMap() { return _punctual; }
	shared_ptr<Ssao>& GetSsao() { return _ssao; }

private:

	shared_ptr<ShadowMap> _smap = nullptr; //shadow Map (디렉셔널 CSM)
	shared_ptr<PunctualShadowMap> _punctual = nullptr; // 점/스팟 그림자
	shared_ptr<Ssao> _ssao = nullptr; //ssao
};

