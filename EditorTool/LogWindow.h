#include "EditorWindow.h"

enum LoggerType : uint8
{
	Info,
	Warn,
	Error,
};

class LogWindow : public EditorWindow
{
public:
	LogWindow();
	~LogWindow();

	virtual void Init() override;
	virtual void Update() override;

	void Clear();
	void AddLog(const char* fmt, ...) IM_FMTARGS(2) ;
	void Draw(const char* title, bool* p_open = NULL);

	void ShowLog();

private:

	ImGuiTextBuffer     _buf;
	ImGuiTextFilter     _filter;
	ImVector<int>       _lineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
	bool                _autoScroll;  // Keep scrolling if already at the bottom.

	//LogType _type = None;


	bool _logOpen = false;


};