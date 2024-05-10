#pragma once

class ShadowMap;
class Ssao;
class TextureRenderer; 

class TextureManager
{
	DECLARE_SINGLE(TextureManager);

public:
	void Init();
	void Update();

	void DrawTextureMap();

	shared_ptr<ShadowMap>& GetShadowMap() { return _smap; }
	shared_ptr<Ssao>& GetSsao() { return _ssao; }
	shared_ptr<TextureRenderer>& GetShadowMapDebugTexture() { return _smapDebugTexture; }
	shared_ptr<class TesTerrain>& GetTerrain() { return _tesTerrain; }

private:
	
	shared_ptr<ShadowMap> _smap = nullptr; //shadow Map
	shared_ptr<Ssao> _ssao = nullptr; //ssao
	shared_ptr<TextureRenderer> _smapDebugTexture = nullptr; //shadow Map Debug Texture
	shared_ptr<TextureRenderer> _ssaoAmbientDebugTexture = nullptr; //shadow Map Debug Texture
	shared_ptr<TextureRenderer> _ssaoNormalDebugTexture = nullptr; //shadow Map Debug Texture


	shared_ptr<class TesTerrain> _tesTerrain = nullptr;

	ComPtr<ID3D11RasterizerState> _wireframeRS;
};

