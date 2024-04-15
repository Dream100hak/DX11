#pragma once
class TextureRenderer 
{
public:

	void Update(Matrix W);
	void SetShader(shared_ptr<Shader> shader); 
	void SetDiffuseMap(ComPtr<ID3D11ShaderResourceView>  diffuseMap) { _diffuseMap = diffuseMap;  /*_desc.useDiffuseMap = _diffuseMap ? 1 : 0;*/ }

	void CreateBuffer();

public:
	//shared_ptr<Texture> GetDiffuseMap() { return _diffuseMap; }
	ComPtr<ID3D11ShaderResourceView>  GetDiffuseMap() { return _diffuseMap; }
	shared_ptr<Shader> GetShader() { return _shader; }


private:
	shared_ptr<Shader> _shader  = nullptr;
	//shared_ptr<Texture> _diffuseMap;
	ComPtr<ID3D11ShaderResourceView>  _diffuseMap;

	ComPtr<ID3DX11EffectShaderResourceVariable> _diffuseEffectBuffer;
	ComPtr<ID3DX11EffectMatrixVariable> _wvpEffectBuffer;

	shared_ptr<Geometry<VertexTextureNormalData>> _geometry;
	shared_ptr<VertexBuffer> _vertexBuffer;
	shared_ptr<IndexBuffer> _indexBuffer;
};

