#pragma once
#include <ctime>
#include <iomanip>
#include <sstream>
#include <magic_enum.hpp>

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
	static string ToString(int32 value);
	static string ToString(float value);

	static wstring GetResourcesName(wstring value , wstring exten);  // 확장자 뺀 이름만 
	static string  GetResourcesName(string value, string exten);  // 확장자 뺀 이름만 

	static string ConvertWCharToChar(const wchar_t* wideString);

	template<typename T> 
	static string GetPtrName(shared_ptr<T> t)
	{
		if (t == nullptr)
			return "";

		// t가 가리키는 실제 객체의 클래스 이름을 가져옵니다.
		// .name()은 컴파일러마다 결과가 다를 수 있지만(ex: "class SceneWindow") 
		// 중복 방지용 Key로 쓰기에는 충분합니다.
		std::string name = typeid(*t).name();
		name = name.substr(name.find(' ') + 1);
		// 만약 "class ", "struct " 같은 접두사를 떼고 싶다면 추가 처리가 가능합니다.
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

