#pragma once
#include <boost/type_index.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>

class Utils
{
public:
	static bool StartsWith(string str, string comp);
	static bool StartsWith(wstring str, wstring comp);

	static void Replace(OUT string& str, string comp, string rep);
	static void Replace(OUT wstring& str, wstring comp, wstring rep);

	static wstring ToWString(string value);


	static string ToString(wstring value);
	static string ToString(Vec2 value);
	static string ToString(Vec3 value); 
	static string ToString(Vec4 value); 
	static string ToString(float value);

	static wstring GetResourcesName(wstring value , wstring exten);  // 확장자 뺀 이름만 
	static string  GetResourcesName(string value, string exten);  // 확장자 뺀 이름만 

	static string ConvertWCharToChar(const wchar_t* wideString);

	template<typename T> 
	static string GetPtrName(shared_ptr<T> t)
	{
		if(t == nullptr)
			return "";

		string name = boost::typeindex::type_id_runtime(*t.get()).pretty_name();
		name = name.substr(name.find(' ') + 1);
		return name;
	}
	template<typename T>
	static string GetClassNameEX()
	{
		string name = std::type_index(typeid(T)).name();
		name = name.substr(name.find(' ') + 1);
		return name;
	}

	static string ConvertTimeToHHMMSS(int64_t time)
	{
		time_t timestamp = static_cast<time_t>(time / 1000);

		struct tm timeInfo;
		localtime_s(&timeInfo, &timestamp);

		std::stringstream ss;
		ss << std::put_time(&timeInfo, "%T"); 

		return ss.str();
	}

};

