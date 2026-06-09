#include "pch.h"
#include "ModelAnimator.h"
#include "Material.h"
#include "ModelMesh.h"
#include "Model.h"
#include "ModelAnimation.h"
#include "Camera.h"
#include "Light.h"
#include "MathUtils.h"
#include "HlslShader.h"
#include "RenderStateManager.h"
#include "Texture.h"
#include "RenderContext.h"
#include "ResourceManager.h"

ModelAnimator::ModelAnimator()
	: Super(RendererType::Animator)
{
}

ModelAnimator::~ModelAnimator()
{

}

void ModelAnimator::OnInspectorGUI()
{
	if (ImGui::InputInt("Current Animation", (int*)&_tweenDesc.curr.animIndex))
	{
		_tweenDesc.curr.animIndex %= _model->GetAnimationCount();
	}

	ImGui::DragFloat("Current Speed", &_tweenDesc.curr.speed);

	ImGui::SliderInt("Current Frame", (int*)&_tweenDesc.curr.currFrame, 0, 1000);
	ImGui::SliderInt("Next Frame", (int*)&_tweenDesc.curr.nextFrame, 0, 1000);
}

// ������������������������������������������������������������������������������������������������������������������������������������
// Update: �ִϸ��̼� ������ ���� (������ �ƴ� Update �ܰ迡��)
// ������������������������������������������������������������������������������������������������������������������������������������
void ModelAnimator::Update()
{
	if (_model == nullptr) return;
	UpdateTweenData();
}

void ModelAnimator::Draw(const RenderContext& ctx)
{
	if (_model == nullptr) return;
	if (_texture == nullptr) CreateTexture();

	// ── Deferred G-Buffer 경로 (HLSL, 애니메이션 스키닝) ──
	if (ctx.deferredPass)
	{
		auto gbuf = RESOURCES->Get<HlslShader>(L"GBufferAnim_HLSL");
		if (!gbuf || ctx.buffer == nullptr) return;

		gbuf->Bind();
		gbuf->PushGlobalData(ctx.view, ctx.proj);
		gbuf->SetVSSRV(5, _srv.Get()); // TransformMap (t5) — 애니 본 변환 텍스처
		// 트윈 데이터(b6)는 InstancingManager 에서 push 됨

		const auto& meshes = _model->GetMeshes();
		for (auto& mesh : meshes)
		{
			if (mesh->material)
			{
				MaterialDesc& md = mesh->material->GetMaterialDesc();
				md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
				gbuf->PushMaterialData(md);

				auto diffuse = mesh->material->GetDiffuseMap();
				auto normal  = mesh->material->GetNormalMap();
				gbuf->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr);
				gbuf->SetPSSRV(2, normal  ? normal->GetComPtr().Get()  : nullptr);
			}
			RENDER_STATES->BindAllSamplersPS();

			mesh->vertexBuffer->PushData();
			mesh->indexBuffer->PushData();
			ctx.buffer->PushData();

			gbuf->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
		}
		return;
	}

	// ── Shadow(depth-only) / SSAO(normal-depth) 패스 (HLSL, 애니메이션 스키닝) ──
	if (ctx.shadowPass || ctx.ssaoPass)
	{
		auto shader = RESOURCES->Get<HlslShader>(ctx.shadowPass ? L"ShadowAnim_HLSL" : L"SsaoNormalDepthAnim_HLSL");
		if (!shader || ctx.buffer == nullptr) return;

		shader->Bind();
		shader->PushGlobalData(ctx.view, ctx.proj);
		shader->SetVSSRV(5, _srv.Get()); // TransformMap (t5)
		// 트윈 데이터(b6)는 InstancingManager 에서 push 됨

		const auto& meshes = _model->GetMeshes();
		for (auto& mesh : meshes)
		{
			if (mesh->material)
			{
				MaterialDesc& md = mesh->material->GetMaterialDesc();
				md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
				shader->PushMaterialData(md);
				auto diffuse = mesh->material->GetDiffuseMap();
				shader->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr); // 알파클립용
			}
			RENDER_STATES->BindAllSamplersPS();

			mesh->vertexBuffer->PushData();
			mesh->indexBuffer->PushData();
			ctx.buffer->PushData();

			shader->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
		}
		return;
	}

	// ── Preview/Thumbnail/Forward lit 경로 (HLSL) ──
	if (ctx.buffer == nullptr)
		return;

	auto lit = RESOURCES->Get<HlslShader>(L"AnimPreview_HLSL");
	if (!lit) return;

	lit->Bind();
	lit->PushGlobalData(ctx.view, ctx.proj);
	lit->SetVSSRV(5, _srv.Get()); // TransformMap (t5)

	// 트윈(b6) — 단일 인스턴스(프리뷰 등)는 직접 push.
	// 다중 인스턴스는 InstancingManager 가 전체 트윈 배열을 이미 push 했으므로 덮어쓰지 않는다.
	if (ctx.buffer->GetCount() <= 1)
	{
		auto tween = make_shared<InstancedTweenDesc>();
		tween->tweens[0] = _tweenDesc;
		lit->PushTweenData(*tween);
	}

	const auto& meshes = _model->GetMeshes();
	for (auto& mesh : meshes)
	{
		if (mesh->material)
		{
			MaterialDesc& md = mesh->material->GetMaterialDesc();
			md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
			lit->PushMaterialData(md);
			auto diffuse = mesh->material->GetDiffuseMap();
			lit->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr);
		}
		RENDER_STATES->BindAllSamplersPS();

		mesh->vertexBuffer->PushData();
		mesh->indexBuffer->PushData();
		ctx.buffer->PushData();

		lit->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
	}
}

void ModelAnimator::SetModel(shared_ptr<Model> model)
{
	_model = model;
}

void ModelAnimator::UpdateTweenData()
{
	TweenDesc& desc = _tweenDesc;

	desc.curr.sumTime += DT;
	// ���� �ִϸ��̼�
	{
		shared_ptr<ModelAnimation> currentAnim = _model->GetAnimationByIndex(desc.curr.animIndex);
		if (currentAnim)
		{
			float timePerFrame = 1 / (currentAnim->frameRate * desc.curr.speed);
			if (desc.curr.sumTime >= timePerFrame)
			{
				desc.curr.sumTime = 0;
				desc.curr.currFrame = (desc.curr.currFrame + 1) % currentAnim->frameCount;
				desc.curr.nextFrame = (desc.curr.currFrame + 1) % currentAnim->frameCount;
			}

			desc.curr.ratio = (desc.curr.sumTime / timePerFrame);
		}
	}

	// ���� �ִϸ��̼��� ���� �Ǿ� �ִٸ�
	if (desc.next.animIndex >= 0)
	{
		desc.tweenSumTime += DT;
		desc.tweenRatio = desc.tweenSumTime / desc.tweenDuration;

		if (desc.tweenRatio >= 1.f)
		{
			// �ִϸ��̼� ���� ��ü
			desc.curr = desc.next;
			desc.ClearNextAnim();
		}
		else
		{
			// ������
			shared_ptr<ModelAnimation> nextAnim = _model->GetAnimationByIndex(desc.next.animIndex);
			desc.next.sumTime += DT;

			float timePerFrame = 1.f / (nextAnim->frameRate * desc.next.speed);

			if (desc.next.ratio >= 1.f)
			{
				desc.next.sumTime = 0;

				desc.next.currFrame = (desc.next.currFrame + 1) % nextAnim->frameCount;
				desc.next.nextFrame = (desc.next.currFrame + 1) % nextAnim->frameCount;
			}

			desc.next.ratio = desc.next.sumTime / timePerFrame;
		}
	}
}


InstanceID ModelAnimator::GetInstanceID()
{
	return make_pair((uint64)_model.get(), (uint64)0);
}

bool ModelAnimator::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
{
	return false;
}

void ModelAnimator::CreateTexture()
{
	if (_model->GetAnimationCount() == 0)
		return;

	_animTransforms.resize(_model->GetAnimationCount());
	for (uint32 i = 0; i < _model->GetAnimationCount(); i++)
		CreateAnimationTransform(i);

	// Creature Texture
	{
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
		desc.Width = MAX_MODEL_TRANSFORMS * 4;
		desc.Height = MAX_MODEL_KEYFRAMES;
		desc.ArraySize = _model->GetAnimationCount();
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 16����Ʈ
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;

		const uint32 dataSize = MAX_MODEL_TRANSFORMS * sizeof(Matrix);
		const uint32 pageSize = dataSize * MAX_MODEL_KEYFRAMES;
		void* mallocPtr = ::malloc(pageSize * _model->GetAnimationCount());

		// �ִϸ��̼� �����͸� �غ��Ѵ�.
		for (uint32 c = 0; c < _model->GetAnimationCount(); c++)
		{
			uint32 startOffset = c * pageSize;

			BYTE* pageStartPtr = reinterpret_cast<BYTE*>(mallocPtr) + startOffset;

			for (uint32 f = 0; f < MAX_MODEL_KEYFRAMES; f++)
			{
				void* ptr = pageStartPtr + dataSize * f;
				::memcpy(ptr, _animTransforms[c].transforms[f].data(), dataSize);
			}
		}

		// ���ҽ� �ʱ�ȭ
		vector<D3D11_SUBRESOURCE_DATA> subResources(_model->GetAnimationCount());

		for (uint32 c = 0; c < _model->GetAnimationCount(); c++)
		{
			void* ptr = (BYTE*)mallocPtr + c * pageSize;
			subResources[c].pSysMem = ptr;
			subResources[c].SysMemPitch = dataSize;
			subResources[c].SysMemSlicePitch = pageSize;
		}

		HRESULT hr = DEVICE->CreateTexture2D(&desc, subResources.data(), _texture.GetAddressOf());
		CHECK(hr);

		::free(mallocPtr);
	}

	// Create SRV
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipLevels = 1;
		desc.Texture2DArray.ArraySize = _model->GetAnimationCount();

		HRESULT hr = DEVICE->CreateShaderResourceView(_texture.Get(), &desc, _srv.GetAddressOf());
		CHECK(hr);
	}
}

void ModelAnimator::CreateAnimationTransform(uint32 index)
{
	vector<Matrix> tempAnimBoneTransforms(MAX_MODEL_TRANSFORMS, Matrix::Identity);

	shared_ptr<ModelAnimation> animation = _model->GetAnimationByIndex(index);

	for (uint32 f = 0; f < animation->frameCount; f++)
	{
		for (uint32 b = 0; b < _model->GetBoneCount(); b++)
		{
			shared_ptr<ModelBone> bone = _model->GetBoneByIndex(b);

			Matrix matAnimation;

			shared_ptr<ModelKeyframe> frame = animation->GetKeyframe(bone->name);
			if (frame != nullptr)
			{
				ModelKeyframeData& data = frame->transforms[f];

				Matrix S, R, T;
				S = Matrix::CreateScale(data.scale.x, data.scale.y, data.scale.z);
				R = Matrix::CreateFromQuaternion(data.rotation);
				T = Matrix::CreateTranslation(data.translation.x, data.translation.y, data.translation.z);

				matAnimation = S * R * T;
			}
			else
			{
				matAnimation = Matrix::Identity;
			}

			// [ !!!!!!! ]
			Matrix toRootMatrix = bone->transform;
			Matrix invGlobal = toRootMatrix.Invert();

			int32 parentIndex = bone->parentIndex;

			Matrix matParent = Matrix::Identity;
			if (parentIndex >= 0)
				matParent = tempAnimBoneTransforms[parentIndex];

			tempAnimBoneTransforms[b] = matAnimation * matParent;

			// �����
			_animTransforms[index].transforms[f][b] = invGlobal * tempAnimBoneTransforms[b];
		}
	}
}
