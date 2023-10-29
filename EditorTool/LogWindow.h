#include "EditorWindow.h"
#include <boost/type_index.hpp>

enum LogFilter : uint8
{
	Info = 1,
	Warn = 2,
	Error = 4,
};

BOOST_DESCRIBE_ENUM(LogFilter, Info, Warn, Error);

struct LogMessage 
{
	LogFilter type;
	string msg;
	string time;
};

class LogWindow : public EditorWindow
{
public:
	LogWindow();
	~LogWindow();

	virtual void Init() override;
	virtual void Update() override;

	void Clear();
	//void AddLog(const char* fmt, ...) IM_FMTARGS(2) ;
	void AddLog(string msg, LogFilter filter);
	void Draw(const char* title, bool* p_open = NULL);

	void ShowLog();

private:

	ImGuiTextFilter     _filter;
	bool                _autoScroll;  

	vector<LogMessage> _messages;
	bool _logOpen = false;
	int32 _msgFilter = (int)(LogFilter::Info | LogFilter::Warn | LogFilter::Error);

};