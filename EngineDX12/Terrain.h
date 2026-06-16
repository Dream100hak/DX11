#pragma once
#include "Component.h"

// DX11 Engine/Terrain 이식(스텁) — 하이트맵/페인트/Foliage 는 추후. Scene 이 ComponentType::Terrain 캐시.
class Terrain : public Component
{
public:
	Terrain() : Component(ComponentType::Terrain) {}
};
