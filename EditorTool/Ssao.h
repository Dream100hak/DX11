#pragma once
struct SsaoBuffer
{
	Matrix ViewToTexSpace;
	Vec4 OffsetVectors[14];
	Vec4 FrustumCorners[4];

	float OcclusionRadius = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05f;
};

struct SsaoBlurBuffer
{
	float TexelWidth;
	float TexelHeight;
	float HorzBlur = 0.f; // 1 = 媛濡?釉붾윭, 0 = ?몃줈 釉붾윭 (FX uniform bool ?뚰겕??遺꾧린 ?泥?
	float BlurPad = 0.f;
};

struct VertexSsao
{
	Vec3 pos;
	Vec3 normal;
	Vec2 uv;
};

class Ssao : public Texture
{
public:
	Ssao(int32 width, int32 height, float fovy, float farZ);
	~Ssao();

	void Draw();

public:
	ComPtr<ID3D11ShaderResourceView> GetAmbientPtr() { return _ambientSRV0; }
	ComPtr<ID3D11ShaderResourceView> GetNormalDepthPtr() { return _normalDepthSRV; }

private:

	void CreateBuffer();
	void CreateTwoAmbientTexture();
	void CreateRandomVectorTexture();
	void CreateSamplers();
	void SetShader();

	void OnSize(int32 width, int32 height, float fovy, float farZ);
	void SetFrustumFarCorners(float fovy, float farZ);
	void SetOffsetVectors();
	void SetNormalDepthRenderTarget(ComPtr<ID3D11DepthStencilView> dsv);

	void ComputeSsao(Matrix& P);
	void BlurAmbientMap(int32 blurCount);
	void BlurAmbientMap(ComPtr<ID3D11ShaderResourceView> inputSRV, ComPtr<ID3D11RenderTargetView> outputRTV, bool horzBlur);

private:

	uint32 _width;
	uint32 _height;
	Viewport _vp;

	ComPtr<ID3D11ShaderResourceView> _randomVectorSRV;
	ComPtr<ID3D11RenderTargetView> _normalDepthRTV;
	ComPtr<ID3D11ShaderResourceView> _normalDepthSRV;

	// Need two for ping-ponging during blur.
	ComPtr<ID3D11RenderTargetView> _ambientRTV0;
	ComPtr<ID3D11ShaderResourceView> _ambientSRV0;
	ComPtr<ID3D11RenderTargetView> _ambientRTV1;
	ComPtr<ID3D11ShaderResourceView> _ambientSRV1;

	Vec4 _frustumFarCorner[4];
	Vec4 _offsets[14];

	//SSAO (HLSL)
	shared_ptr<HlslShader> _ssaoShader = nullptr;

	SsaoBuffer _ssaoDesc;
	shared_ptr<ConstantBuffer<SsaoBuffer>> _ssaoBuffer;

	//SSAO BLUR (HLSL)
	shared_ptr<HlslShader> _ssaoBlurShader = nullptr;

	SsaoBlurBuffer _ssaoBlurDesc;
	shared_ptr<ConstantBuffer<SsaoBlurBuffer>> _ssaoBlurBuffer;

	// ?섑뵆??(FX ?곗씠?????뺤쓽瑜?C++ 濡??댁쟾)
	ComPtr<ID3D11SamplerState> _samBorder; // SSAO s0: LINEAR_MIP_POINT, BORDER(0,0,0,1e5)
	ComPtr<ID3D11SamplerState> _samWrap;   // SSAO s1: LINEAR_MIP_POINT, WRAP
	ComPtr<ID3D11SamplerState> _samClamp;  // Blur s0: LINEAR_MIP_POINT, CLAMP

	//Buffer
	shared_ptr<Geometry<VertexSsao>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;
};

