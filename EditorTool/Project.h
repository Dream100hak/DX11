#pragma once
#include "EditorWindow.h"

class Model;

enum MetaType
{
	None = 0,
	Folder,
	Meta, 
	Text,
	Sound,
	Image,
	Mesh,
	Xml,
	UnKnown,
};

struct MetaData
{
	wstring name;
	wstring path;
	MetaType type; 
	//TODO : 
};

class Project : public EditorWindow
{
public:

	Project();
	~Project();

	virtual void Init() override;
	virtual void Update() override;

	std::wstring GetExecutablePath() {
		WCHAR path[MAX_PATH];
		GetModuleFileName(NULL, path, MAX_PATH);
		std::wstring fullPath(path);
		std::wstring::size_type pos = fullPath.find_last_of(L"\\/");
		return fullPath.substr(0, pos);
	}

	std::wstring GetDirectoryAbove(const wstring& path) {
		std::wstring::size_type pos = path.find_last_of(L"\\/");
		return path.substr(0, pos);
	}

	MetaType GetMetaType(const wstring& name);

	float CalculateCameraDistance(float modelDiagonal, float fov)
	{
		// 시야각의 절반을 라디안으로 변환
		float halfFovRadians = (fov / 2.0f) * (XM_PI / 180.0f);

		// 모델의 대각선 길이를 적절한 크기로 조정
		float adjustedModelDiagonal = min(modelDiagonal, 10.0f); // 예: 최대 10.0f로 제한

		// 카메라 거리 계산
		float distance = (adjustedModelDiagonal / 2.0f) / tan(halfFovRadians);

		// 현실적인 범위 내에서 거리 제한
		return max(min(distance, 1000.0f), 10.0f); // 예: 최소 10.0f, 최대 1000.0f
	}

	void ShowProject();
	void ListFolderHierarchy(const wstring& directory);
	// 새로운 함수 추가: 선택된 폴더의 내용을 보여주는 창
	void ShowFolderContents();
	void DisplayItem(const std::wstring& path, const MetaData& meta, int columns);

private:

	void RefreshCasheFileList(const wstring& directory);


private:
	wstring _rootDirectory = L"";
	wstring _selectedDirectory = L"";  // 선택한 폴더의 경로
	wstring _selectedFolder = L"";  // 사용자가 선택한 폴더
	wstring _selectedItem = L"";  // 사용자가 선택한 폴더
	map<wstring , MetaData> _cashesFileList; 

private:
	shared_ptr<GameObject> _camera;
	float _modelDiagonal = 2.07744789f;

};