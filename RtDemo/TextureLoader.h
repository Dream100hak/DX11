#pragma once
#include "Common.h"
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

// WIC(Windows Imaging Component, OS 내장)로 PNG/JPG 를 RGBA8 픽셀로 디코드.
// CoInitializeEx 가 먼저 호출돼 있어야 함.
inline bool LoadImageRGBA(const std::wstring& path, std::vector<uint8_t>& outPixels, uint32& outW, uint32& outH)
{
	ComPtr<IWICImagingFactory> factory;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
		return false;

	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnDemand, &decoder)))
		return false;

	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame))) return false;

	ComPtr<IWICFormatConverter> conv;
	if (FAILED(factory->CreateFormatConverter(&conv))) return false;
	if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
		WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		return false;

	UINT w = 0, h = 0;
	frame->GetSize(&w, &h);
	outW = w; outH = h;
	outPixels.resize(size_t(w) * h * 4);
	if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)outPixels.size(), outPixels.data())))
		return false;
	return true;
}
