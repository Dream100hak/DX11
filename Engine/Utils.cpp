#include "pch.h"
#include "Utils.h"
#include <filesystem>

wstring Utils::ToMaterialKey(const wstring& path)
{
	wstring p = path;

	// 확장자 없이 들어오는 호출(.mmat 의 matName 등)도 같은 키로 수렴
	if (p.size() < 4 || _wcsicmp(p.substr(p.size() - 4).c_str(), L".mat") != 0)
		p += L".mat";

	// 구분자(\, /)와 상대 경로(..) 차이 흡수 — 파일이 없어도 동작 (weakly)
	std::error_code ec;
	std::filesystem::path canon = std::filesystem::weakly_canonical(std::filesystem::path(p), ec);
	wstring key = ec ? p : canon.wstring();

	// 윈도우 파일시스템은 대소문자 비구분 — 키도 동일하게
	std::transform(key.begin(), key.end(), key.begin(), ::towlower);
	return key;
}


bool Utils::StartsWith(string str, string comp)
{
	wstring::size_type index = str.find(comp);
	if (index != wstring::npos && index == 0)
		return true;

	return false;
}

bool Utils::StartsWith(wstring str, wstring comp)
{
	wstring::size_type index = str.find(comp);
	if (index != wstring::npos && index == 0)
		return true;

	return false;
}

void Utils::Replace(OUT string& str, string comp, string rep)
{
	string temp = str;

	size_t start_pos = 0;
	while ((start_pos = temp.find(comp, start_pos)) != wstring::npos)
	{
		temp.replace(start_pos, comp.length(), rep);
		start_pos += rep.length();
	}

	str = temp;
}

void Utils::Replace(OUT wstring& str, wstring comp, wstring rep)
{
	wstring temp = str;

	size_t start_pos = 0;
	while ((start_pos = temp.find(comp, start_pos)) != wstring::npos)
	{
		temp.replace(start_pos, comp.length(), rep);
		start_pos += rep.length();
	}

	str = temp;
}

// UTF-8 <-> UTF-16 蹂??(?덉쟾 naive narrowing ? ?쒓???源⑥죱??
std::wstring Utils::ToWString(string value)
{
	if (value.empty())
		return L"";

	int len = ::MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
	if (len <= 0)
		return wstring(value.begin(), value.end()); // 鍮꾩젙???낅젰 ?대갚

	wstring result(len, L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), len);
	return result;
}

std::string Utils::ToString(wstring value)
{
	if (value.empty())
		return "";

	int len = ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	if (len <= 0)
		return string(value.begin(), value.end()); // 鍮꾩젙???낅젰 ?대갚

	string result(len, '\0');
	::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), len, nullptr, nullptr);
	return result;
}
std::string Utils::ToString(Vec2 value)
{
	string tmp = "";
	tmp += to_string(value.x);
	tmp += " : ";
	tmp += to_string(value.y);
	return tmp;
}

std::string Utils::ToString(Vec3 value)
{
	string tmp = ToString(Vec2(value.x , value.y))  + " : ";
	tmp += to_string(value.z);
	return tmp; 
}

std::string Utils::ToString(Vec4 value)
{
	string tmp = ToString(Vec3(value.x, value.y, value.z)) + " : ";
	tmp += to_string(value.w);
	return tmp;
}

std::string Utils::ToString(float value)
{
	string tmp = to_string(value);
	return tmp;
}

std::string Utils::ToString(int32 value)
{
	string tmp = to_string(value);
	return tmp;
}

std::wstring Utils::GetResourcesName(wstring value, wstring exten)
{
	wstring resourceName = value;
	size_t idx = value.find(exten);
	if (idx != std::wstring::npos) 
		resourceName.erase(idx, exten.length());
	
	return resourceName;
}

std::string Utils::GetResourcesName(string value, string exten)
{
	string resourceName = value;
	size_t idx = value.find(exten);
	if (idx != std::string::npos)
		resourceName.erase(idx, exten.length());

	return resourceName;
}

string Utils::ConvertWCharToChar(const wchar_t* wstr)
{
	int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
	if (bufferSize == 0)
	{
		// 변환 실패
		return "";
	}

	std::string result(bufferSize, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], bufferSize, nullptr, nullptr);

	return result;
}
