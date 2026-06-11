#include "pch.h"
#include "Utils.h"


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

// UTF-8 <-> UTF-16 변환 (예전 naive narrowing 은 한글이 깨졌음)
std::wstring Utils::ToWString(string value)
{
	if (value.empty())
		return L"";

	int len = ::MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
	if (len <= 0)
		return wstring(value.begin(), value.end()); // 비정상 입력 폴백

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
		return string(value.begin(), value.end()); // 비정상 입력 폴백

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
		// ��ȯ ����
		return "";
	}

	std::string result(bufferSize, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], bufferSize, nullptr, nullptr);

	return result;
}
