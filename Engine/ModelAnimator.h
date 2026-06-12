#pragma once
#include "Renderer.h"
#include "RenderContext.h"

class Model;

struct AnimTransform
{
	// [ ][ ][ ][ ][ ][ ][ ] ... 250행
	using TransformArrayType = array<Matrix, MAX_MODEL_TRANSFORMS>;
	// [ ][ ][ ][ ][ ][ ][ ] ... 500 열
	array<TransformArrayType, MAX_MODEL_KEYFRAMES> transforms;
};

class ModelAnimator : public Renderer
{
	using Super = Renderer;

public:
	ModelAnimator();
	~ModelAnimator();

	void OnInspectorGUI() override;

	// 애니메이션 프레임 업데이트
	void Update() override; // 함수 설명: 애니메이션 프레임 업데이트
	void Draw(const RenderContext& ctx) override;

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) override;

	void UpdateTweenData(); // 트윈 업데이트

public:
	void SetModel(shared_ptr<Model> model);

	InstanceID GetInstanceID() override;
	shared_ptr<Model>& GetModel() { return _model; }
	TweenDesc& GetTweenDesc()     { return _tweenDesc; }
	ComPtr<ID3D11ShaderResourceView> GetTransformMapSRV() { return _srv; } // 아웃라인 패스용 (t5)

private:
	void CreateTexture();
	void CreateAnimationTransform(uint32 index);

private:
	vector<AnimTransform>   _animTransforms;
	ComPtr<ID3D11Texture2D>    _texture;
	ComPtr<ID3D11ShaderResourceView> _srv;
	TweenDesc  _tweenDesc;
	shared_ptr<Model>  _model;
};

