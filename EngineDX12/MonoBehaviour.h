#pragma once
#include "Component.h"
#include <functional>
#include <sstream>

// DX11 Engine/MonoBehaviour 이식 — 스크립트 컴포넌트 베이스. GameObject._scripts 에 보관.
// Play 모드에서 Scene::Update → GameObject::Update → 스크립트 Update 가 틱.
class MonoBehaviour : public Component
{
public:
	MonoBehaviour() : Component(ComponentType::Script) {}
	virtual const char* TypeName() const { return "Script"; } // 직렬화/레지스트리 키
	virtual void Serialize(std::ostream&) {}                   // 파라미터 저장
	virtual void Deserialize(std::istream&) {}                 // 파라미터 로드
};

// 스크립트 팩토리 레지스트리 (이름 → 생성). 시작 시 RegisterBuiltinScripts() 로 등록.
namespace ScriptRegistry
{
	using Factory = std::function<shared_ptr<MonoBehaviour>()>;
	inline std::map<std::string, Factory>& Map() { static std::map<std::string, Factory> m; return m; }
	inline void Register(const std::string& name, Factory f) { Map()[name] = f; }
	inline shared_ptr<MonoBehaviour> Create(const std::string& name)
	{
		auto it = Map().find(name);
		return it != Map().end() ? it->second() : nullptr;
	}
}
