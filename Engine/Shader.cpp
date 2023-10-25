#include "pch.h"
#include "Shader.h"
#include "Utils.h"


Shader::Shader(wstring name) : Super(ResourceType::Shader) , _fileName(name)
{
	_resourcesName = Utils::GetResourcesName(name , L".fx");

	wstring finalPath = _path + _fileName;
	Load(finalPath);
}

Shader::~Shader()
{

}

void Shader::Load(const wstring& path)
{
	auto shader = RESOURCES->Get<Shader>(_resourcesName);

	if (shader == nullptr)
	{
		ComPtr<ID3DBlob> error;
		INT flag = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;

		HRESULT hr = ::D3DCompileFromFile(path.c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, NULL, "fx_5_0", flag, NULL, _blob.GetAddressOf(), error.GetAddressOf());
		if (FAILED(hr))
		{
			if (error != NULL)
			{
				string str = (const char*)error->GetBufferPointer();
				MessageBoxA(NULL, str.c_str(), "Shader Error", MB_OK);
			}
			assert(false);
		}

		hr = ::D3DX11CreateEffectFromMemory(_blob->GetBufferPointer(), _blob->GetBufferSize(), 0, DEVICE.Get(), _effect.GetAddressOf());
		CHECK(hr);
	}
}
