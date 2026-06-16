#pragma once
#include "Component.h"

// DX11 Engine/MonoBehaviour 이식 — 스크립트 컴포넌트 베이스. GameObject._scripts 에 보관.
class MonoBehaviour : public Component
{
public:
	MonoBehaviour() : Component(ComponentType::Script) {}
};
