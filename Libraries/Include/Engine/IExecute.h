#pragma once
class IExecute
{
public:
	virtual void Init() abstract;
	virtual void Update() abstract;
	virtual void Render() abstract;

	virtual void OnMouseWheel(int32 scrollAmount) abstract;

};

