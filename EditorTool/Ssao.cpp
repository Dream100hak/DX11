#include "pch.h"
#include "Ssao.h"
#include "MathUtils.h"
#include "Camera.h"
#include "GeometryHelper.h"
#include "ModelRenderer.h"
#include "Terrain.h"

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

void Ssao::SetShader()
{
	// SSAO //

	auto ssaoShader = RESOURCES->Get<Shader>(L"Ssao");

	_ssaoShader = ssaoShader;

	_normalDepthEffectBuffer = ssaoShader->GetSRV("NormalDepthMap");
	_randomEffectBuffer = ssaoShader->GetSRV("RandomVecMap");

	if (_ssaoEffectBuffer == nullptr)
	{
		_ssaoBuffer = make_shared<ConstantBuffer<SsaoBuffer>>();
		_ssaoBuffer->Create();
		_ssaoEffectBuffer = ssaoShader->GetConstantBuffer("SsaoBuffer");
	}

	// SSAO BLUR //

	auto ssaoBlurShader = RESOURCES->Get<Shader>(L"SsaoBlur");

	_ssaoBlurShader = ssaoBlurShader;

	_normalDepthBlurEffectBuffer = ssaoBlurShader->GetSRV("NormalDepthMap");
	_inputBlurEffectBuffer = ssaoBlurShader->GetSRV("InputImage");

	if (_ssaoBlurEffectBuffer == nullptr)
	{
		_ssaoBlurBuffer = make_shared<ConstantBuffer<SsaoBlurBuffer>>();
		_ssaoBlurBuffer->Create();
		_ssaoBlurEffectBuffer = ssaoBlurShader->GetConstantBuffer("SsaoBlurBuffer");
	}
}

void Ssao::SetNormalDepthRenderTarget(ComPtr<ID3D11DepthStencilView> dsv)
{
	DCT->RSSetState(0);

	DCT->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	GRAPHICS->GetViewport().RSSetViewport();
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
	_ssaoEffectBuffer->SetConstantBuffer(_ssaoBuffer->GetComPtr().Get());

	_normalDepthEffectBuffer->SetResource(_normalDepthSRV.Get());
	_randomEffectBuffer->SetResource(_randomVectorSRV.Get());

	_vertexBuffer->PushData();
	_indexBuffer->PushData();
	_ssaoShader->DrawIndexed(0, 0, 6, 0, 0);
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

	_ssaoBlurBuffer->CopyData(_ssaoBlurDesc);
	_ssaoBlurEffectBuffer->SetConstantBuffer(_ssaoBlurBuffer->GetComPtr().Get());

	_normalDepthBlurEffectBuffer->SetResource(_normalDepthSRV.Get());
	_inputBlurEffectBuffer->SetResource(inputSRV.Get());

	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	_ssaoBlurShader->DrawIndexed(horzBlur, 0, 6, 0, 0);

}

void Ssao::Draw()
{
	shared_ptr<Scene> scene = CUR_SCENE;
	unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

	auto camera = scene->GetMainCamera()->GetCamera();
	auto light = SCENE->GetCurrentScene()->GetLight()->GetLight();

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
	
	auto shader = RESOURCES->Get<Shader>(L"SsaoNormalDepth");
	//Draw To Normal Depth
	
	SetNormalDepthRenderTarget(GRAPHICS->GetDsv());
	INSTANCING->Render(0, shader, V, P, light, vecForward);

	if (terrain)
		terrain->TerrainRendererNotPS(shader , V , P);

	/////////////////////////////////////////////////////////
	ComputeSsao(P);
	BlurAmbientMap(4);
	
}
