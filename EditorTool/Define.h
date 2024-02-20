#pragma once

#define TOOL			GET_SINGLE(EditorToolManager)		
#define SHORTCUT		GET_SINGLE(ShortcutManager)		

#define ADDLOG(msg, filter)			TOOL->GetLog()->AddLog(msg,filter)


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
	MetaType metaType = NONE;
};


#define SELECTED_H		TOOL->GetSelectedIdH()
#define SELECTED_P		TOOL->GetSelectedIdP()