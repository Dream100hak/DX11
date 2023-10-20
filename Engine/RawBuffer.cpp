#include "pch.h"
#include "RawBuffer.h"

RawBuffer::RawBuffer(void* inputData, uint32 inputByte, uint32 outputByte)
	: _inputData(inputData), _inputByte(inputByte), _outputByte(outputByte)
{
	CreateBuffer();
}

RawBuffer::~RawBuffer()
{

}

void RawBuffer::CreateBuffer()
{
	CreateInput();
	CreateSRV();
	CreateOutput();
	CreateUAV();
	CreateResult();
}

void RawBuffer::CopyToInput(void* data)
{
	D3D11_MAPPED_SUBRESOURCE subResource;
	DC->Map(_input.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
	{
		memcpy(subResource.pData, data, _inputByte);
	}
	DC->Unmap(_input.Get(), 0);
}

void RawBuffer::CopyFromOutput(void* data)
{
	// 출력 데이터 -> result에 복사
	DC->CopyResource(_result.Get(), _output.Get());

	D3D11_MAPPED_SUBRESOURCE subResource;
	DC->Map(_result.Get(), 0, D3D11_MAP_READ, 0, &subResource);
	{
		memcpy(data, subResource.pData, _outputByte);
	}
	DC->Unmap(_result.Get(), 0);
}

void RawBuffer::CreateInput()
{
	if (_inputByte == 0)
		return;

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ByteWidth = _inputByte;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS; // RAW_BUFFER
	desc.Usage = D3D11_USAGE_DYNAMIC; // CPU-WRITE, GPU-READ
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA subResource = { 0 };
	subResource.pSysMem = _inputData;

	if (_inputData != nullptr)
		CHECK(DEVICE->CreateBuffer(&desc, &subResource, _input.GetAddressOf()));
	else
		CHECK(DEVICE->CreateBuffer(&desc, nullptr, _input.GetAddressOf()));
}

void RawBuffer::CreateSRV()
{
	if (_inputByte == 0)
		return;

	D3D11_BUFFER_DESC desc;
	_input->GetDesc(&desc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS; // 쉐이더에서 알아서 하세요
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX; // SRV_FLAG_RAW
	srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
	srvDesc.BufferEx.NumElements = desc.ByteWidth / 4; // 전체 데이터 개수

	CHECK(DEVICE->CreateShaderResourceView(_input.Get(), &srvDesc, _srv.GetAddressOf()));
}

void RawBuffer::CreateOutput()
{
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ByteWidth = _outputByte;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	CHECK(DEVICE->CreateBuffer(&desc, NULL, _output.GetAddressOf()));
}

void RawBuffer::CreateUAV()
{
	D3D11_BUFFER_DESC desc;
	_output->GetDesc(&desc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	uavDesc.Buffer.NumElements = desc.ByteWidth / 4;

	CHECK(DEVICE->CreateUnorderedAccessView(_output.Get(), &uavDesc, _uav.GetAddressOf()));
}

void RawBuffer::CreateResult()
{
	D3D11_BUFFER_DESC desc;
	_output->GetDesc(&desc);

	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.BindFlags = D3D11_USAGE_DEFAULT; // UAV가 연결되려면, USAGE는 DEFAULT여야 함.
	desc.MiscFlags = 0;

	CHECK(DEVICE->CreateBuffer(&desc, nullptr, _result.GetAddressOf()));
}
