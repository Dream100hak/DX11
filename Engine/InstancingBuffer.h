#pragma once

class VertexBuffer;

struct InstancingData
{
	Matrix world;
	uint32 isPicked; // 4����Ʈ
	float padding[3]; // �е� �߰� (Matrix�� 64����Ʈ�̹Ƿ� �� 64����Ʈ�� ���߱� ���� �߰�)
};

#define MAX_MESH_INSTANCE 500

class InstancingBuffer
{
public:
	InstancingBuffer();
	~InstancingBuffer();

private:
	void CreateBuffer(uint32 maxCount = MAX_MESH_INSTANCE);

public:
	void ClearData();
	void AddData(InstancingData& data);

	void PushData();

public:
	uint32						GetCount() { return static_cast<uint32>(_data.size()); }
	shared_ptr<VertexBuffer>	GetBuffer() { return _instanceBuffer; }

	void	SetID(uint64 instanceId) { _instanceId = instanceId; }
	uint64	GetID() { return _instanceId; }

private:
	uint64						_instanceId = 0;
	shared_ptr<VertexBuffer>	_instanceBuffer;
	uint32						_maxCount = 0;
	vector<InstancingData>		_data;


};

