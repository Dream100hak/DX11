#pragma once
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include "Component.h"

class Transform;
class Camera;
class MeshRenderer;
class ModelRenderer;
class ModelAnimator;
class Light;
class BaseCollider;
class Terrain;
class Button;
class Billboard;
class SnowBillboard;

class ImGuiManager
{
	DECLARE_SINGLE(ImGuiManager);

public:
	void Init();
	void Update();
	void Render();

	int32 CreateEmptyGameObject();
	void RemoveGameObject(int32 id);

	template<class E>
	std::string EnumToString(E e)
	{
		string r = "(unnamed)";

		boost::mp11::mp_for_each<boost::describe::describe_enumerators<E>>([&](auto D)
			{
				if (e == D.value)
					r = D.name;
			});

		return r;
	}

};

