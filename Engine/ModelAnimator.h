#pragma once
#include "Renderer.h"
#include "RenderContext.h"

class Model;

struct AnimTransform
{
	// [ ][ ][ ][ ][ ][ ][ ] ... 250개
	using TransformArrayType = array<Matrix, MAX_MODEL_TRANSFORMS>;
	// [ ][ ][ ][ ][ ][ ][ ] ... 500 개
	array<TransformArrayType, MAX_MODEL_KEYFRAMES> transforms;
};

class ModelAnimator : public Renderer
{
	using Super = Renderer;

public:
	ModelAnimator(shared_ptr<Shader> shader);
	~ModelAnimator();

	void OnInspectorGUI() override;

	// ── 신규 단일 진입점 ─────────────────────────────────────
	void Draw(const RenderContext& ctx) override;

	bool Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance) override;

	void UpdateTweenData();

public:
	void SetModel(shared_ptr<Model> model);
	shared_ptr<Shader> GetShader() { return _shader; }

	InstanceID GetInstanceID() override;
	shared_ptr<Model>& GetModel() { return _model; }
	TweenDesc& GetTweenDesc()     { return _tweenDesc; }

private:
	void PushBufferInstancing(uint8 technique, uint8 pass, shared_ptr<Light> light, shared_ptr<InstancingBuffer>& buffer);
	void CreateTexture();
	void CreateAnimationTransform(uint32 index);

private:
	vector<AnimTransform>            _animTransforms;
	ComPtr<ID3D11Texture2D>    _texture;
	ComPtr<ID3D11ShaderResourceView> _srv;
	TweenDesc  _tweenDesc;
	shared_ptr<Shader> _shader;
	shared_ptr<Model>  _model;
};

