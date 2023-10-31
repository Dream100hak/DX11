#pragma once

#define TOOL			GET_SINGLE(EditorToolManager)		
#define SHORTCUT		GET_SINGLE(ShortcutManager)		

#define ADDLOG(msg, filter)			TOOL->GetLog()->AddLog(msg,filter)

#define SELECTED_H		TOOL->GetSelectedIdH()
#define SELECTED_P		TOOL->GetSelectedIdP()