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

// 애니메이션 상태 업데이트
// 애니메이션 상태 업데이트
// 애니메이션 상태 업데이트
void ModelAnimator::Update()
{
	if (_model == nullptr) return;
	UpdateTweenData();
}

void ModelAnimator::Draw(const RenderContext& ctx)
{
	if (_model == nullptr) return;
	if (_texture == nullptr) CreateTexture();

	// ?? Deferred G-Buffer 寃쎈줈 (HLSL, ?좊땲硫붿씠???ㅽ궎?? ??
	if (ctx.deferredPass)
	{
		auto gbuf = RESOURCES->Get<HlslShader>(L"GBufferAnim_HLSL");
		if (!gbuf || ctx.buffer == nullptr) return;

		gbuf->Bind();
		gbuf->PushGlobalData(ctx.view, ctx.proj);
		gbuf->SetVSSRV(5, _srv.Get()); // TransformMap (t5) 바인딩
		// 트윈(b6)은 InstancingManager에서 push

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

	// ?? Shadow(depth-only) / SSAO(normal-depth) ?⑥뒪 (HLSL, ?좊땲硫붿씠???ㅽ궎?? ??
	if (ctx.shadowPass || ctx.ssaoPass)
	{
		auto shader = RESOURCES->Get<HlslShader>(ctx.shadowPass ? L"ShadowAnim_HLSL" : L"SsaoNormalDepthAnim_HLSL");
		if (!shader || ctx.buffer == nullptr) return;

		shader->Bind();
		shader->PushGlobalData(ctx.view, ctx.proj);
		shader->SetVSSRV(5, _srv.Get()); // TransformMap (t5) 바인딩
		// 트윈(b6)은 InstancingManager에서 push

		const auto& meshes = _model->GetMeshes();
		for (auto& mesh : meshes)
		{
			if (mesh->material)
			{
				MaterialDesc& md = mesh->material->GetMaterialDesc();
				md.useTexture = mesh->material->GetDiffuseMap() ? 1 : 0;
				shader->PushMaterialData(md);
				auto diffuse = mesh->material->GetDiffuseMap();
				shader->SetPSSRV(0, diffuse ? diffuse->GetComPtr().Get() : nullptr); // ?뚰뙆?대┰??
			}
			RENDER_STATES->BindAllSamplersPS();

			mesh->vertexBuffer->PushData();
			mesh->indexBuffer->PushData();
			ctx.buffer->PushData();

			shader->DrawIndexedInstanced(mesh->indexBuffer->GetCount(), ctx.buffer->GetCount());
		}
		return;
	}

	// ?? Preview/Thumbnail/Forward lit 寃쎈줈 (HLSL) ??
	if (ctx.buffer == nullptr)
		return;

	auto lit = RESOURCES->Get<HlslShader>(L"AnimPreview_HLSL");
	if (!lit) return;

	lit->Bind();
	lit->PushGlobalData(ctx.view, ctx.proj);
	lit->SetVSSRV(5, _srv.Get()); // TransformMap (t5) 바인딩

	// 트윈(b6)은 InstancingManager에서 push
	// 배열 버퍼는 InstancingManager가 전체 배열을 push 하므로 제외
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
	// 애니메이션 상태 업데이트
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

	// 애니메이션 상태 업데이트
	if (desc.next.animIndex >= 0)
	{
		desc.tweenSumTime += DT;
		desc.tweenRatio = desc.tweenSumTime / desc.tweenDuration;

		if (desc.tweenRatio >= 1.f)
		{
			// 애니메이션 상태 업데이트
			desc.curr = desc.next;
			desc.ClearNextAnim();
		}
		else
		{
			// 애니메이션 상태 업데이트
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

// 현재 애니메이션 포즈 기준 삼각형 픽킹 — GPU 스키닝과 동일한 본 행렬(_animTransforms)을
// CPU 로 적용해 화면에 보이는 포즈 그대로 판정. 애니 데이터가 없으면 바인드포즈 폴백.
bool ModelAnimator::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
{
	if (_model == nullptr)
		return false;

	auto cam = SCENE->GetCurrentScene()->GetMainCamera()->GetCamera();
	Matrix V = cam->GetViewMatrix();
	Matrix P = cam->GetProjectionMatrix();
	Matrix W = GetTransform()->GetWorldMatrix();

	Viewport& vp = GRAPHICS->GetViewport();

	Vec3 n = vp.Unproject(Vec3(screenX, screenY, 0), Matrix::Identity, V, P);
	Vec3 f = vp.Unproject(Vec3(screenX, screenY, 1), Matrix::Identity, V, P);
	Vec3 start = n;
	Vec3 direction = f - n;
	direction.Normalize();
	Ray ray = Ray(start, direction);
	vector<shared_ptr<ModelMesh>>& meshes = _model->GetMeshes();
	vector<shared_ptr<ModelBone>>& bones = _model->GetBones();

	// 바운딩 박스 선판정 — 애니 포즈가 바인드 박스를 벗어날 수 있어 여유를 두고 키운다
	TransformBoundingBox();
	BoundingBox looseBox = _boundingBox;
	looseBox.Extents.x = looseBox.Extents.x * 1.6f + 0.5f;
	looseBox.Extents.y = looseBox.Extents.y * 1.6f + 0.5f;
	looseBox.Extents.z = looseBox.Extents.z * 1.6f + 0.5f;

	float dist = 0.f;
	if (looseBox.Intersects(ray.position, ray.direction, dist) == false)
		return false;

	// 현재 포즈 본 팔레트 — TransformMap(t5)에 올라간 것과 같은 데이터를 프레임 보간해 재구성
	// (_animTransforms 는 첫 Draw 의 CreateTexture 에서 채워짐 — 그 전엔 바인드포즈 폴백)
	const KeyframeDesc& kf = _tweenDesc.curr;
	vector<Matrix> palette;
	if (kf.animIndex >= 0 && kf.animIndex < static_cast<int32>(_animTransforms.size()))
	{
		const uint32 boneCount = min(_model->GetBoneCount(), static_cast<uint32>(MAX_MODEL_TRANSFORMS));
		const uint32 curr = min(kf.currFrame, static_cast<uint32>(MAX_MODEL_KEYFRAMES - 1));
		const uint32 next = min(kf.nextFrame, static_cast<uint32>(MAX_MODEL_KEYFRAMES - 1));

		palette.resize(boneCount);
		for (uint32 b = 0; b < boneCount; b++)
		{
			palette[b] = Matrix::Lerp(
				_animTransforms[kf.animIndex].transforms[curr][b],
				_animTransforms[kf.animIndex].transforms[next][b],
				kf.ratio);
		}
	}

	for (auto mesh : meshes)
	{
		const auto& vertices = mesh->GetGeometry()->GetVertices();
		const auto& indices = mesh->GetGeometry()->GetIndices();

		// 정점 전체를 현재 포즈의 월드 좌표로 변환 (공유 정점 중복 스키닝 방지)
		Matrix bindWorld = bones[mesh->boneIndex]->transform * W;
		vector<Vec3> posed(vertices.size());

		for (size_t v = 0; v < vertices.size(); v++)
		{
			const auto& vtx = vertices[v];

			if (palette.empty() == false)
			{
				const float idx[4] = { vtx.blendIndices.x, vtx.blendIndices.y, vtx.blendIndices.z, vtx.blendIndices.w };
				const float wgt[4] = { vtx.blendWeights.x, vtx.blendWeights.y, vtx.blendWeights.z, vtx.blendWeights.w };

				Vec3 acc = Vec3::Zero;
				float weightSum = 0.f;

				for (int32 k = 0; k < 4; k++)
				{
					if (wgt[k] <= 0.f)
						continue;

					uint32 b = static_cast<uint32>(idx[k]);
					if (b >= palette.size())
						continue;

					Vec3 skinned = XMVector3TransformCoord(vtx.position, palette[b]);
					acc += skinned * wgt[k];
					weightSum += wgt[k];
				}

				if (weightSum > 0.0001f)
				{
					posed[v] = XMVector3TransformCoord(acc * (1.f / weightSum), W);
					continue;
				}
			}

			// 애니 데이터 없음 / 비스킨 정점 — 기존 바인드포즈 경로
			posed[v] = XMVector3TransformCoord(vtx.position, bindWorld);
		}

		for (uint32 i = 0; i < indices.size() / 3; ++i)
		{
			const Vec3& v0 = posed[indices[i * 3 + 0]];
			const Vec3& v1 = posed[indices[i * 3 + 1]];
			const Vec3& v2 = posed[indices[i * 3 + 2]];

			if (ray.Intersects(v0, v1, v2, OUT distance))
			{
				pickPos = ray.position + ray.direction * distance;
				return true;
			}
		}
	}

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
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 애니메이션 상태 업데이트
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;

		const uint32 dataSize = MAX_MODEL_TRANSFORMS * sizeof(Matrix);
		const uint32 pageSize = dataSize * MAX_MODEL_KEYFRAMES;
		void* mallocPtr = ::malloc(pageSize * _model->GetAnimationCount());

		// 애니메이션 상태 업데이트
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

		// 서브리소스 초기화
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

			// 애니메이션 상태 업데이트
			_animTransforms[index].transforms[f][b] = invGlobal * tempAnimBoneTransforms[b];
		}
	}
}
