#pragma once
#include "Common.h"
#include "Define.h"

// DX11 Engine/ResourceManager 이식(1차) — 키(wstring)별 리소스 캐시. RESOURCES 매크로.
// (Mesh/Material/Texture 등 공유 — 동일 키 = 한 인스턴스)
class ResourceManager
{
	DECLARE_SINGLE(ResourceManager);

public:
	template<typename T>
	void Add(const wstring& key, shared_ptr<T> res) { _cache[key] = res; }

	template<typename T>
	shared_ptr<T> Get(const wstring& key)
	{
		auto it = _cache.find(key);
		return it != _cache.end() ? std::static_pointer_cast<T>(it->second) : nullptr;
	}

	bool Has(const wstring& key) const { return _cache.find(key) != _cache.end(); }
	void Clear() { _cache.clear(); }

private:
	unordered_map<wstring, shared_ptr<void>> _cache;
};
