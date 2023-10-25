#pragma once


class SkyBox : public Component
{
	using Super = Component;

public:
	SkyBox();
	virtual ~SkyBox();

	void Init();

private:
	
	shared_ptr<Material> _material = nullptr;
	shared_ptr<Texture> _texture = nullptr;
	

};

