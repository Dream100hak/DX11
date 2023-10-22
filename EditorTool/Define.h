#pragma once

#define TOOL			GET_SINGLE(EditorToolManager)		
#define SHORTCUT		GET_SINGLE(ShortcutManager)		

#define SELECTED_H		TOOL->GetSelectedIdH()
#define SELECTED_P		TOOL->GetSelectedIdP()