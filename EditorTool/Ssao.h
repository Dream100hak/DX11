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
	Vec2 Dummy = Vec2::Zero;
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

	//SSAO
	shared_ptr<Shader> _ssaoShader = nullptr;

	SsaoBuffer _ssaoDesc;
	shared_ptr<ConstantBuffer<SsaoBuffer>> _ssaoBuffer;
	ComPtr<ID3DX11EffectConstantBuffer> _ssaoEffectBuffer;

	ComPtr<ID3DX11EffectShaderResourceVariable> _normalDepthEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _randomEffectBuffer;

	//SSAO BLUR
	shared_ptr<Shader> _ssaoBlurShader = nullptr;

	SsaoBlurBuffer _ssaoBlurDesc;
	shared_ptr<ConstantBuffer<SsaoBlurBuffer>> _ssaoBlurBuffer;
	ComPtr<ID3DX11EffectConstantBuffer> _ssaoBlurEffectBuffer;

	ComPtr<ID3DX11EffectShaderResourceVariable> _normalDepthBlurEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _inputBlurEffectBuffer;

	//Buffer
	shared_ptr<Geometry<VertexSsao>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;
};

