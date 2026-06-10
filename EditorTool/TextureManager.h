#pragma once

class ShadowMap;
class Ssao;

class TextureManager
{
	DECLARE_SINGLE(TextureManager);

public:
	void Init();
	void Update();

	void DrawTextureMap();

	shared_ptr<ShadowMap>& GetShadowMap() { return _smap; }
	shared_ptr<Ssao>& GetSsao() { return _ssao; }

private:

	shared_ptr<ShadowMap> _smap = nullptr; //shadow Map
	shared_ptr<Ssao> _ssao = nullptr; //ssao
};

