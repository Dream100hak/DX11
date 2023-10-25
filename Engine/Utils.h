#pragma once

class Utils
{
public:
	static bool StartsWith(string str, string comp);
	static bool StartsWith(wstring str, wstring comp);

	static void Replace(OUT string& str, string comp, string rep);
	static void Replace(OUT wstring& str, wstring comp, wstring rep);

	static wstring ToWString(string value);
	static string ToString(wstring value);

	static wstring GetResourcesName(wstring value , wstring exten);  // 확장자 뺀 이름만 
	static string  GetResourcesName(string value, string exten);  // 확장자 뺀 이름만 

	static string ConvertWCharToChar(const wchar_t* wideString);
};

