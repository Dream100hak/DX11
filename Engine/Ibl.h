#pragma once

// IBL (Image-Based Lighting) ?꾨━而댄벂?????// ?쒖옉 ???섍꼍 ?먮툕留듭뿉??irradiance / prefiltered env / BRDF LUT 瑜?1??踰좎씠??
// DeferredLighting ?⑥뒪媛 t5(irradiance)/t6(prefiltered)/t7(BRDF LUT) 濡??뚮퉬.
class Ibl
{
public:
	enum { IRRADIANCE_SIZE = 32, PREFILTER_SIZE = 128, PREFILTER_MIPS = 5, BRDF_LUT_SIZE = 512 };

	// ?섍꼍 ?먮툕留?.dds cube) 濡쒕뱶 + ?꾩껜 踰좎씠?? ResourceManager 珥덇린???댄썑 1???몄텧.
	static void Init(const wstring& envCubePath);

	static bool IsReady() { return _ready; }
	static ComPtr<ID3D11ShaderResourceView> GetEnvMap()      { return _envSRV; }
	static ComPtr<ID3D11ShaderResourceView> GetIrradiance()  { return _irradianceSRV; }
	static ComPtr<ID3D11ShaderResourceView> GetPrefiltered() { return _prefilteredSRV; }
	static ComPtr<ID3D11ShaderResourceView> GetBrdfLut()     { return _brdfSRV; }

private:
	static void BakeIrradiance();
	static void BakePrefiltered();
	static void BakeBrdfLut();

	static inline bool _ready = false;
	static inline ComPtr<ID3D11ShaderResourceView> _envSRV;
	static inline ComPtr<ID3D11ShaderResourceView> _irradianceSRV;
	static inline ComPtr<ID3D11ShaderResourceView> _prefilteredSRV;
	static inline ComPtr<ID3D11ShaderResourceView> _brdfSRV;
};
