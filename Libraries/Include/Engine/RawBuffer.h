#pragma once

class RawBuffer
{
public:
	RawBuffer(void* inputData, uint32 inputByte, uint32 outputByte);
	~RawBuffer();

public:
	void CreateBuffer();
	void CopyToInput(void* data);
	void CopyFromOutput(void* data);

public:
	ComPtr<ID3D11ShaderResourceView> GetSRV() { return _srv; }
	ComPtr<ID3D11UnorderedAccessView> GetUAV() { return _uav; }

public:
	void CreateInput();
	void CreateSRV();
	void CreateOutput();
	void CreateUAV();
	void CreateResult();

private:
	ComPtr<ID3D11Buffer> _input;
	ComPtr<ID3D11ShaderResourceView> _srv;
	ComPtr<ID3D11Buffer> _output;
	ComPtr<ID3D11UnorderedAccessView> _uav;
	ComPtr<ID3D11Buffer> _result;

private:

	void* _inputData;
	uint32 _inputByte;
	uint32 _outputByte;

};

