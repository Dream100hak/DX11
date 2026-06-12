#include "pch.h"
#include "Ssao.h"
#include "MathUtils.h"
#include "Camera.h"
#include "GeometryHelper.h"
#include "ModelRenderer.h"
#include "Terrain.h"
#include "Material.h"

Ssao::Ssao(int32 width, int32 height, float fovy, float farZ)
{
	OnSize(width, height, fovy, farZ);

	CreateBuffer();
	CreateTwoAmbientTexture();
	CreateRandomVectorTexture();

	SetOffsetVectors();
	SetShader(); 
}
Ssao::~Ssao()
{
}

void Ssao::CreateBuffer()
{
	_geometry = make_shared<Geometry<VertexSsao>>();
	vector<VertexSsao> vtx;
	vtx.resize(4);

	vtx[0].pos = Vec3(-1.0f, -1.0f, 0.0f);
	vtx[1].pos = Vec3(-1.0f, +1.0f, 0.0f);
	vtx[2].pos = Vec3(+1.0f, +1.0f, 0.0f);
	vtx[3].pos = Vec3(+1.0f, -1.0f, 0.0f);

	// Store far plane frustum corner indices in Normal.x slot.
	vtx[0].normal = Vec3(0.0f, 0.0f, 0.0f);
	vtx[1].normal = Vec3(1.0f, 0.0f, 0.0f);
	vtx[2].normal = Vec3(2.0f, 0.0f, 0.0f);
	vtx[3].normal = Vec3(3.0f, 0.0f, 0.0f);

	vtx[0].uv = Vec2(0.0f, 1.0f);
	vtx[1].uv = Vec2(0.0f, 0.0f);
	vtx[2].uv = Vec2(1.0f, 0.0f);
	vtx[3].uv = Vec2(1.0f, 1.0f);

	_geometry->SetVertices(vtx);

	vector<uint32> idx = { 0, 1, 2, 0, 2, 3 };

	_geometry->SetIndices(idx);

	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());
}

void Ssao::OnSize(int32 width, int32 height, float fovy, float farZ)
{
	_width = width;
	_height = height;

	_vp.Set(width / 2  , height / 2 , 0.f , 0.f , 0.f, 1.0f);
	SetFrustumFarCorners(fovy, farZ);
}

// 렌더 타겟 크기 변경 시 SSAO 버퍼 재생성 (같으면 프러스텀 코너만 갱신)
void Ssao::Resize(int32 width, int32 height, float fovy, float farZ)
{
	if (width == _width && height == _height)
	{
		SetFrustumFarCorners(fovy, farZ);
		return;
	}

	OnSize(width, height, fovy, farZ);

	// GetAddressOf 덮어쓰기 누수 방지 — 해제 후 재생성
	_normalDepthSRV.Reset();
	_normalDepthRTV.Reset();
	_ambientSRV0.Reset();
	_ambientRTV0.Reset();
	_ambientSRV1.Reset();
	_ambientRTV1.Reset();
	CreateTwoAmbientTexture();
}

void Ssao::SetShader()
{
	// SSAO / Blur ??HLSL (FX 00. Ssao.fx / 00. SsaoBlur.fx ?泥?
	_ssaoShader     = RESOURCES->Get<HlslShader>(L"Ssao_HLSL");
	_ssaoBlurShader = RESOURCES->Get<HlslShader>(L"SsaoBlur_HLSL");

	_ssaoBuffer = make_shared<ConstantBuffer<SsaoBuffer>>();
	_ssaoBuffer->Create();

	_ssaoBlurBuffer = make_shared<ConstantBuffer<SsaoBlurBuffer>>();
	_ssaoBlurBuffer->Create();

	CreateSamplers();
}

void Ssao::CreateSamplers()
{
	// FX ?곗씠???덉뿉 ?뺤쓽???덈뜕 ?섑뵆?щ뱾??C++ ?먯꽌 ?앹꽦
	D3D11_SAMPLER_DESC desc{};
	desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MaxLOD = D3D11_FLOAT32_MAX;

	// BORDER(0,0,0,1e5): 留?諛??섑뵆? 留ㅼ슦 癒?源딆씠濡?泥섎━??false occlusion 諛⑹?
	desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	desc.BorderColor[0] = 0.f; desc.BorderColor[1] = 0.f;
	desc.BorderColor[2] = 0.f; desc.BorderColor[3] = 1e5f;
	CHECK(DEVICE->CreateSamplerState(&desc, _samBorder.GetAddressOf()));

	desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	CHECK(DEVICE->CreateSamplerState(&desc, _samWrap.GetAddressOf()));

	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	CHECK(DEVICE->CreateSamplerState(&desc, _samClamp.GetAddressOf()));
}

void Ssao::SetNormalDepthRenderTarget(ComPtr<ID3D11DepthStencilView> dsv)
{
	DCT->RSSetState(0);

	DCT->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 자기 텍스처 기준 원점 뷰포트 — 스크린 offset 이 포함된 GRAPHICS 뷰포트를 쓰면
	// normal-depth 가 어긋나게 기록되어 AO 가 엉뚱한 위치에 찍힘 (GBuffer 와 동일 규약)
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(_width);
	vp.Height = static_cast<float>(_height);
	vp.MaxDepth = 1.f;
	DCT->RSSetViewports(1, &vp);
	ID3D11RenderTargetView* renderTargets[1] = { _normalDepthRTV.Get() };
	DCT->OMSetRenderTargets(1, renderTargets, dsv.Get());

	// Clear view space normal to (0,0,-1) and clear depth to be very far away.  
	float clearColor[] = { 0.0f, 0.0f, -1.0f, 1e5f };
	DCT->ClearRenderTargetView(_normalDepthRTV.Get(), clearColor);
}

void Ssao::SetFrustumFarCorners(float fovy, float farZ)
{
	float aspect = (float)_width / (float)_height;

	float halfHeight = farZ * tanf(0.5f * fovy);
	float halfWidth = aspect * halfHeight;

	_frustumFarCorner[0] = Vec4(-halfWidth, -halfHeight, farZ, 0.0f);
	_frustumFarCorner[1] = Vec4(-halfWidth, +halfHeight, farZ, 0.0f);
	_frustumFarCorner[2] = Vec4(+halfWidth, +halfHeight, farZ, 0.0f);
	_frustumFarCorner[3] = Vec4(+halfWidth, -halfHeight, farZ, 0.0f);
}

void Ssao::SetOffsetVectors()
{
	// and the 6 center points along each cube face.  We always alternate the points on 
	// opposites sides of the cubes.  This way we still get the vectors spread out even
	// if we choose to use less than 14 samples.

	// 8 cube corners
	_offsets[0] = Vec4(+1.0f, +1.0f, +1.0f, 0.0f);
	_offsets[1] = Vec4(-1.0f, -1.0f, -1.0f, 0.0f);
				  
	_offsets[2] = Vec4(-1.0f, +1.0f, +1.0f, 0.0f);
	_offsets[3] = Vec4(+1.0f, -1.0f, -1.0f, 0.0f);
				  
	_offsets[4] = Vec4(+1.0f, +1.0f, -1.0f, 0.0f);
	_offsets[5] = Vec4(-1.0f, -1.0f, +1.0f, 0.0f);
				  
	_offsets[6] = Vec4(-1.0f, +1.0f, -1.0f, 0.0f);
	_offsets[7] = Vec4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 6 centers of cube faces
	_offsets[8] = Vec4(-1.0f, 0.0f, 0.0f, 0.0f);
	_offsets[9] = Vec4(+1.0f, 0.0f, 0.0f, 0.0f);

	_offsets[10] = Vec4(0.0f, -1.0f, 0.0f, 0.0f);
	_offsets[11] = Vec4(0.0f, +1.0f, 0.0f, 0.0f);

	_offsets[12] = Vec4(0.0f, 0.0f, -1.0f, 0.0f);
	_offsets[13] = Vec4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int32 i = 0; i < 14; ++i)
	{
		// Create random lengths in [0.25, 1.0].
		float s = MathUtils::Random(0.25f, 1.0f);

		XMVECTOR v = s * ::XMVector4Normalize(::XMLoadFloat4(&_offsets[i]));
		::XMStoreFloat4(&_offsets[i], v);
	}
}

void Ssao::CreateTwoAmbientTexture()
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = _width;
	texDesc.Height = _height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	HRESULT hr;

	ComPtr<ID3D11Texture2D> normalDepthTex;
	hr = DEVICE->CreateTexture2D(&texDesc, 0, normalDepthTex.GetAddressOf());
	CHECK(hr);

	hr = DEVICE->CreateShaderResourceView(normalDepthTex.Get(), 0, _normalDepthSRV.GetAddressOf());
	CHECK(hr);
	hr = DEVICE->CreateRenderTargetView(normalDepthTex.Get(), 0, _normalDepthRTV.GetAddressOf());
	CHECK(hr);

	// Render ambient map at half resolution.
	texDesc.Width = _width / 2;
	texDesc.Height = _height / 2;
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;

	ComPtr<ID3D11Texture2D> ambientTex0;
	hr = DEVICE->CreateTexture2D(&texDesc, 0, ambientTex0.GetAddressOf());
	CHECK(hr);
	hr = DEVICE->CreateShaderResourceView(ambientTex0.Get(), 0, _ambientSRV0.GetAddressOf());
	CHECK(hr);
	hr = DEVICE->CreateRenderTargetView(ambientTex0.Get(), 0, _ambientRTV0.GetAddressOf());
	CHECK(hr);

	ComPtr<ID3D11Texture2D> ambientTex1;
	hr = DEVICE->CreateTexture2D(&texDesc, 0, ambientTex1.GetAddressOf());
	CHECK(hr);
	hr = DEVICE->CreateShaderResourceView(ambientTex1.Get(), 0, _ambientSRV1.GetAddressOf());
	CHECK(hr);
	hr = DEVICE->CreateRenderTargetView(ambientTex1.Get(), 0, _ambientRTV1.GetAddressOf());
	CHECK(hr);
}

void Ssao::CreateRandomVectorTexture()
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = 256;
	texDesc.Height = 256;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = { 0 };
	initData.SysMemPitch = 256 * sizeof(Color);

	vector<Color> color(256 * 256);

	for (int32 i = 0; i < 256; ++i)
	{
		for (int32 j = 0; j < 256; ++j)
		{
			Vec3 v(MathUtils::Random(), MathUtils::Random(), MathUtils::Random());
			color[i * 256 + j] = Color(v.x, v.y, v.z, 0.0f);
		}
	}

	initData.pSysMem = color.data();
	HRESULT hr;

	ComPtr<ID3D11Texture2D> tex;
	hr = DEVICE->CreateTexture2D(&texDesc, &initData, tex.GetAddressOf());
	CHECK(hr);

	hr = DEVICE->CreateShaderResourceView(tex.Get(), 0, _randomVectorSRV.GetAddressOf());
	CHECK(hr);
}

void Ssao::ComputeSsao(Matrix& P)
{
	if(_ssaoShader == nullptr)
		return;

	Color black = Color(0.f,0.f,0.f,1.f);

	ID3D11RenderTargetView* renderTargets[1] = { _ambientRTV0.Get() };
	DCT->OMSetRenderTargets(1, renderTargets, 0);
	DCT->ClearRenderTargetView(_ambientRTV0.Get(), (float*)(&black));
	_vp.RSSetViewport();

	Matrix T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	Matrix PT = P * T;

	_ssaoDesc = SsaoBuffer{};
	_ssaoDesc.ViewToTexSpace = PT;

	for (int i = 0; i < 14; ++i) 
		_ssaoDesc.OffsetVectors[i] = _offsets[i];

	for (int i = 0; i < 4; ++i) 
		_ssaoDesc.FrustumCorners[i] = _frustumFarCorner[i];
	
	_ssaoDesc.OcclusionRadius = 0.5f;
	_ssaoDesc.OcclusionFadeStart = 0.2f;
	_ssaoDesc.OcclusionFadeEnd = 2.0f;
	_ssaoDesc.SurfaceEpsilon = 0.05f;

	_ssaoBuffer->CopyData(_ssaoDesc);

	auto shader = _ssaoShader;
	shader->Bind();

	auto buf = _ssaoBuffer->GetComPtr().Get();
	shader->SetVSConstantBuffer(8, buf); // FrustumCorners (VS)
	shader->SetPSConstantBuffer(8, buf);

	shader->SetPSSRV(0, _normalDepthSRV.Get());
	shader->SetPSSRV(1, _randomVectorSRV.Get());
	shader->SetPSSampler(0, _samBorder.Get());
	shader->SetPSSampler(1, _samWrap.Get());

	DCT->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_vertexBuffer->PushData();
	_indexBuffer->PushData();
	shader->DrawIndexed(6, 0, 0);

	shader->SetPSSRV(0, nullptr); // normal-depth ???ㅼ쓬 ?⑥뒪?먯꽌 RT 濡??곗씪 ???덉뼱 ?댁젣
}


void Ssao::BlurAmbientMap(int32 blurCount)
{
	for (int32 i = 0; i < blurCount; ++i)
	{
		// Ping-pong the two ambient map textures as we apply
		// horizontal and vertical blur passes.
		BlurAmbientMap(_ambientSRV0, _ambientRTV1, true);
		BlurAmbientMap(_ambientSRV1, _ambientRTV0, false);
	}
}

void Ssao::BlurAmbientMap(ComPtr<ID3D11ShaderResourceView> inputSRV, ComPtr<ID3D11RenderTargetView> outputRTV, bool horzBlur)
{
	if(_ssaoBlurShader == nullptr)
		return;

	Color black = Color(0.f, 0.f, 0.f, 1.f);

	ID3D11RenderTargetView* renderTargets[1] = { outputRTV.Get() };
	DCT->OMSetRenderTargets(1, renderTargets, 0);
	DCT->ClearRenderTargetView(outputRTV.Get(), (float*)(&black));
	_vp.RSSetViewport();

	_ssaoBlurDesc = SsaoBlurBuffer {} ;
	_ssaoBlurDesc.TexelWidth =  1.f / _vp.GetWidth();
	_ssaoBlurDesc.TexelHeight = 1.f / _vp.GetHeight();
	_ssaoBlurDesc.HorzBlur = horzBlur ? 1.f : 0.f;

	_ssaoBlurBuffer->CopyData(_ssaoBlurDesc);

	auto shader = _ssaoBlurShader;
	shader->Bind();

	shader->SetPSConstantBuffer(8, _ssaoBlurBuffer->GetComPtr().Get());
	shader->SetPSSRV(0, _normalDepthSRV.Get());
	shader->SetPSSRV(1, inputSRV.Get());
	shader->SetPSSampler(0, _samClamp.Get());

	DCT->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	shader->DrawIndexed(6, 0, 0);

	// ?묓릟: ?대쾲 ?낅젰 SRV 媛 ?ㅼ쓬 ?⑥뒪??RT 媛 ?섎?濡?諛섎뱶???댁젣
	shader->SetPSSRV(1, nullptr);
}

void Ssao::Draw(shared_ptr<Camera> renderCam)
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	auto camera = (renderCam != nullptr) ? renderCam : scene->GetMainCamera()->GetCamera();

	// 라이트 없는 씬 (New Scene 직후) — SSAO 패스 스킵 (null 역참조 크래시 방지)
	auto lightObj = scene->GetLight();
	if (lightObj == nullptr || lightObj->GetLight() == nullptr)
		return;
	auto light = lightObj->GetLight();

	vector<shared_ptr<GameObject>> vecForward;

	shared_ptr<Terrain> terrain = nullptr;

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetTerrain() != nullptr)
			terrain = gameObject->GetTerrain();

		if (camera->IsCulled(gameObject->GetLayerIndex()))
			continue;

		if (gameObject->GetRenderer() == nullptr)
			continue;

		if(gameObject->GetSkyBox())
			continue;

		vecForward.push_back(gameObject);
	}

	Matrix V = camera->GetViewMatrix();
	Matrix P = camera->GetProjectionMatrix();

	// 렌더 타겟 크기에 SSAO 버퍼 동기화 (도킹 씬 창 리사이즈/Game 뷰 대응) + 프러스텀 코너 갱신
	int32 targetW = (renderCam != nullptr) ? (int32)camera->GetWidth() : (int32)GRAPHICS->GetViewport().GetWidth();
	int32 targetH = (renderCam != nullptr) ? (int32)camera->GetHeight() : (int32)GRAPHICS->GetViewport().GetHeight();
	if (targetW > 0 && targetH > 0)
		Resize(targetW, targetH, camera->GetFov(), camera->GetFar());

	//Draw To Normal Depth
	SetNormalDepthRenderTarget(GRAPHICS->GetDsv());

	// RenderContext ?ㅼ젙 諛??몄텧 (HLSL SsaoNormalDepth*_HLSL: view-space normal+depth)
	RenderContext ctx;
	ctx.tech = 0;
	ctx.view = V;
	ctx.proj = P;
	ctx.light = light;
	ctx.hlslOverride = nullptr;
	ctx.buffer = nullptr;
	ctx.lightArray = nullptr;
	ctx.ssaoPass = true;

	INSTANCING->Render(ctx, vecForward);

	// ?곕젅?몄? view-space normal+depth 瑜?湲곕줉 (PS ?녿뒗 depth-only ?⑥뒪???媛??댁냼)
	if (terrain)
		terrain->TerrainRendererNormalDepth(V , P);

	/////////////////////////////////////////////////////////
	ComputeSsao(P);
	BlurAmbientMap(4);

	// Resize 가 _ambientSRV0 을 재생성하면 머티리얼이 들고 있던 SRV 는 죽은 옛 텍스처가 됨 —
	// 디퍼드 라이팅/PassViewer 가 첫 프레임 AO 를 영원히 샘플(카메라 안 따라옴) → 매 드로우 후 최신 SRV 로 갱신
	if (auto mat = RESOURCES->Get<Material>(L"DefaultMaterial"))
		mat->SetSsaoMap(_ambientSRV0);
}
