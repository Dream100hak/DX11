#pragma once
#include "IExecute.h"

class SceneDemo : public IExecute
{
public:
	void Init() override;
	void Update() override;
	void Render() override;

private:
	shared_ptr<Shader> _shader;

private:
};

