#pragma once
struct SsaoBuffer
{
	Matrix gViewToTexSpace;
	XMFLOAT4 gOffsetVectors[14];
	XMFLOAT4 gFrustumCorners[4];

	float gOcclusionRadius = 0.5f;
	float gOcclusionFadeStart = 0.2f;
	float gOcclusionFadeEnd = 2.0f;
	float gSurfaceEpsilon = 0.05f;
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

	void CreateBuffer();
	void OnSize(int32 width, int32 height, float fovy, float farZ);

	void SetShader(shared_ptr<Shader> shader); 

	void SetNormalDepthRenderTarget(ComPtr<ID3D11DepthStencilView> dsv);
	//void ComputeSsao(shared_ptr<Shader> shader, Matrix P);
	void ComputeSsao();
	void BlurAmbientMap(int32 blurCount);

	void SetFrustumFarCorners(float fovy, float farZ);
	void SetOffsetVectors();

	void CreateTextureViews();
	void CreateRandomVectorTexture();

	void Draw();

public:
	ComPtr<ID3D11ShaderResourceView> GetAmbientPtr() { return _ambientSRV0; }
	ComPtr<ID3D11ShaderResourceView> GetNormalDepthPtr() { return _normalDepthSRV; }

private:
	void BlurAmbientMap(ComPtr<ID3D11ShaderResourceView> inputSRV, ComPtr<ID3D11RenderTargetView> outputRTV, bool horzBlur);

private:

	uint32 _width;
	uint32 _height;

	ComPtr<ID3D11ShaderResourceView> _randomVectorSRV;
	ComPtr<ID3D11RenderTargetView> _normalDepthRTV;
	ComPtr<ID3D11ShaderResourceView> _normalDepthSRV;

	// Need two for ping-ponging during blur.
	ComPtr<ID3D11RenderTargetView> _ambientRTV0;
	ComPtr<ID3D11ShaderResourceView> _ambientSRV0;
	ComPtr<ID3D11RenderTargetView> _ambientRTV1;
	ComPtr<ID3D11ShaderResourceView> _ambientSRV1;

	XMFLOAT4 _frustumFarCorner[4];
	XMFLOAT4 _offsets[14];

	Viewport _vp;

	shared_ptr<Shader> _shader = nullptr; 

	//SSAO
	SsaoBuffer _ssaoDesc;
	shared_ptr<ConstantBuffer<SsaoBuffer>> _ssaoBuffer;
	ComPtr<ID3DX11EffectConstantBuffer> _ssaoEffectBuffer;

	ComPtr<ID3DX11EffectMatrixVariable> _viewToTexEffectBuffer;
	ComPtr<ID3DX11EffectVectorVariable> _offsetEffectBuffer;
	ComPtr<ID3DX11EffectVectorVariable> _frustumConersEffectBuffer;

	//ComPtr<ID3DX11EffectScalarVariable> _occlusionRadius;
	//ComPtr<ID3DX11EffectScalarVariable> _occlusionFadeStart;
	//ComPtr<ID3DX11EffectScalarVariable> _occlusionFadeEnd;
	//ComPtr<ID3DX11EffectScalarVariable> _surfaceEpsilon;

	ComPtr<ID3DX11EffectShaderResourceVariable> _normalDepthEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _randomEffectBuffer;

	shared_ptr<Geometry<VertexSsao>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;

	//SSAO BLUR
	ComPtr<ID3DX11EffectScalarVariable> _texelWidthEffectBuffer;
	ComPtr<ID3DX11EffectScalarVariable> _texelHeightEffectBuffer;

	ComPtr<ID3DX11EffectShaderResourceVariable> _normalDepthBlurEffectBuffer;
	ComPtr<ID3DX11EffectShaderResourceVariable> _inputBlurEffectBuffer;

};

