#pragma once

// IBL (Image-Based Lighting) 프리컴퓨트 홀더
// 시작 시 환경 큐브맵에서 irradiance / prefiltered env / BRDF LUT 를 1회 베이크.
// DeferredLighting 패스가 t5(irradiance)/t6(prefiltered)/t7(BRDF LUT) 로 소비.
class Ibl
{
public:
	enum { IRRADIANCE_SIZE = 32, PREFILTER_SIZE = 128, PREFILTER_MIPS = 5, BRDF_LUT_SIZE = 512 };

	// 환경 큐브맵(.dds cube) 로드 + 전체 베이크. ResourceManager 초기화 이후 1회 호출.
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
