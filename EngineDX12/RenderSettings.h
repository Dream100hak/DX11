#pragma once
#include <DirectXMath.h>

// ───────────────────────────────────────────────────────────
// RenderSettings — D3D12Device 에서 분리한 "룩/포스트" 튜닝 파라미터 묶음.
// 인스펙터가 편집하고 SceneCB/PostFX 로 흘러가는 시각 파라미터만 모았다(디바이스/리소스/씬그래프/툴 상태 제외).
// D3D12Device 가 public 상속 → 기존 _bloomOn 등 참조는 그대로 컴파일(호출부 무변경).
// Look Profile(스타일라이즈드/사실적 등)은 이 구조체 필드 묶음을 프리셋으로 세팅하는 형태로 확장.
// ───────────────────────────────────────────────────────────
struct RenderSettings
{
	// ── 톤맵 / 노출 ──
	float             _exposure = 1.0f;       // 수동 노출 (×2^EV)
	float             _ev = 0.0f;             // 노출 보정(스톱)
	int               _tonemapOp = 0;         // 0 ACES / 1 Reinhard / 2 Filmic
	bool              _autoExp = false; float _expTarget = 0.5f; // 자동 노출 목표 휘도

	// ── 블룸 ──
	bool              _bloomOn = true;
	float             _bloomIntensity = 0.6f;
	float             _bloomThreshold = 1.0f;

	// ── 안티에일리어싱 ──
	bool              _taaOn = false; float _taaSharp = 0.35f; // TAA(시간적) + 언샤프
	bool              _fxaaOn = true;

	// ── 모션블러 (카메라 + 오브젝트 속도버퍼) ──
	bool              _motionBlurOn = false; float _motionBlurIntensity = 1.5f;

	// ── 컬러 그레이딩 / 렌즈 FX ──
	float             _contrast = 1.0f, _saturation = 1.0f, _temperature = 0.0f, _vignette = 0.25f;
	float             _chroma = 0.0f, _grain = 0.0f, _sharpen = 0.0f;
	float             _lensDistort = 0.0f, _posterize = 0.0f;
	int               _filterMode = 0;        // none/sepia/gray/invert
	bool              _anamorphic = false;

	// ── 셰이딩(툰/림/아웃라인/노멀) ──
	int               _toonLevels = 0;        // 0=off
	float             _rimPower = 0.0f; DirectX::XMFLOAT3 _rimColor{ 0.3f, 0.55f, 1.0f };
	DirectX::XMFLOAT3 _outlineColor{ 1.7f, 0.85f, 0.12f }; float _outlineThick = 0.005f;
	float             _normalIntensity = 1.0f;

	// ── GI / AO / 앰비언트 ──
	float             _giStrength = 0.45f;
	float             _ambient = 0.03f;
	float             _hemiAmbient = 0.0f;
	bool              _aoOn = false; float _aoIntensity = 1.0f, _aoRadius = 0.6f;

	// ── 반사 / DOF / 볼류메트릭 ──
	bool              _reflectOn = false; float _reflectStrength = 0.5f;
	bool              _dofOn = false; float _dofFocus = 6.0f, _dofRange = 4.0f;
	bool              _volOn = false; float _volStrength = 0.5f;

	// ── 안개 ──
	DirectX::XMFLOAT3 _fogColor{ 0.55f, 0.62f, 0.72f }; float _fogDensity = 0.0f;
	bool              _heightFog = false; float _fogHeight = 3.0f, _fogFalloff = 0.3f;

	// ── 그림자 ──
	float             _shadowSoft = 0.0f;     // RT 소프트 그림자 반경
	float             _shadowStrength = 1.0f;

	// ── 하늘 / 배경 ──
	DirectX::XMFLOAT3 _skyZenith{ 0.13f, 0.22f, 0.44f }, _skyHorizon{ 0.52f, 0.60f, 0.72f }; float _sunSize = 900.0f;
	int               _bgMode = 0;            // 0 sky / 1 solid
	DirectX::XMFLOAT3 _bgColor{ 0.06f, 0.07f, 0.10f };
	bool              _showSky = true;
	float             _cloudAmt = 0.0f;
	bool              _stars = false;
	bool              _flicker = false; float _flickerV = 1.0f;

	// ── IBL / 태양 ──
	bool              _iblOn = true; float _iblIntensity = 1.0f;
	float             _envIntensity = 1.0f;
	DirectX::XMFLOAT3 _sunColor{ 1.0f, 0.96f, 0.88f };
	float             _lightIntensity = 1.2f;

	// ── 바닥 / 그리드 / 구도 ──
	bool              _showFloor = true;
	DirectX::XMFLOAT3 _floorColor{ 0.85f, 0.13f, 0.11f }; float _floorMetallic = 0.0f, _floorRough = 0.6f;
	bool              _checker = false;
	bool              _showGrid = true;
	float             _gridCell = 1.0f, _gridFade = 60.0f;
	float             _letterbox = 0.0f;
	bool              _overlay = false;
};
