#pragma once

class Shader : public ResourceBase
{
	using Super = ResourceBase;

public:
	Shader(wstring file);
	virtual ~Shader();

	virtual void Load(const wstring& path) override;

	ComPtr<ID3DBlob> GetBlob() {return _blob;}
	ComPtr<ID3DX11Effect> GetEffect() {return _effect;}

	const wstring& GetFilename() {return _fileName; }
	const wstring& GetPath() {return _path; }

private:

	ComPtr<ID3DBlob> _blob;
	ComPtr<ID3DX11Effect> _effect;

	wstring _fileName;
	wstring _path = L"..\\Shaders\\";

	wstring _resourcesName;

};

