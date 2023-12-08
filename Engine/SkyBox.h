#pragma once

enum class SkyType : uint8
{
	SkyBox,
	CubeMap, 
};

class SkyBox : public Component
{
	using Super = Component;

public:
	SkyBox();
	virtual ~SkyBox();

	void Init(SkyType type);

private:
	
	shared_ptr<Material> _material = nullptr;
	shared_ptr<Texture> _texture = nullptr;

	SkyType _type = SkyType::SkyBox;	
	
};

