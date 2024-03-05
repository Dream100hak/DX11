#pragma once

#define TOOL			GET_SINGLE(EditorToolManager)		
#define SHORTCUT		GET_SINGLE(ShortcutManager)		

#define ADDLOG(msg, filter)			TOOL->GetLog()->AddLog(msg,filter)

#define SELECTED_H			TOOL->GetSelectedIdH()
#define SELECTED_P			TOOL->GetSelectedIdP()
#define SELECTED_ITEM		TOOL->GetSelectedItem()
#define SELECTED_FOLDER		TOOL->GetSelectedFolder()
#define CASHE_FILE_LIST		TOOL->GetCashesFileList()

enum MetaType
{
	NONE = 0,
	FOLDER,
	META,
	TEXT,
	SOUND,
	IMAGE,
	MESH,
	XML,
	Unknown,
};

struct MetaData
{
	wstring fileName;
	wstring fileFullPath;
	Vec2    size;
	MetaType metaType = NONE;
};

static inline ImVec2 operator*(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
static inline ImVec2 operator/(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x / rhs, lhs.y / rhs); }
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
