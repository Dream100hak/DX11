#include "pch.h"
#include "LogWindow.h"
#include "Utils.h"
#include <chrono>

LogWindow::LogWindow()
{
	_autoScroll = true;
	Clear();
}
LogWindow::~LogWindow()
{

}

void LogWindow::Init()
{


}

void LogWindow::Update()
{
	ShowLog();
}

void LogWindow::Clear()
{
	_messages.clear();
}

void LogWindow::AddLog(string msg, LogFilter filter)
{

	LogMessage logMsg = {};

	auto nowTime = chrono::system_clock::now();
	auto miltime = chrono::duration_cast<chrono::milliseconds>(nowTime.time_since_epoch()).count();

	if (_messages.find(msg) == _messages.end())
	{
		logMsg.type = filter;
		logMsg.msg = msg;
	}
	else
	{
		logMsg = _messages[msg];
	}

	logMsg.time = "[" + Utils::ConvertTimeToHHMMSS(miltime) + "]" + " : ";
	logMsg.count++;

	//_messages.push_back(logMsg);
	_messages[msg] = logMsg;

}

void LogWindow::Draw(const char* title, bool* p_open /*= NULL*/)
{
	ImGui::SetNextWindowPos(ImVec2(0, 551));
	ImGui::SetNextWindowSize(ImVec2(800, 500));

	if (!ImGui::Begin(title, p_open))
	{
		ImGui::End();
		return;
	}

	// Options menu
	if (ImGui::BeginPopup("Options"))
	{
		ImGui::Checkbox("Auto-scroll", &_autoScroll);
		ImGui::EndPopup();
	}

	// Main window
	if (ImGui::Button("Options"))
		ImGui::OpenPopup("Options");
	ImGui::SameLine();
	bool clear = ImGui::Button("Clear");
	ImGui::SameLine();

	_filter.Draw("##Filter", -200.0f);

	int32 filter = 0;

	float rightSideX = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;

	ImGui::SameLine(rightSideX);
	if (ImGui::ColorButton("ErrorBtn", ImVec4(1.f, 0.f, 0.f, 1.f)))
			filter |= (int32)LogFilter::Error;
		
	rightSideX -= ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 3;
	ImGui::SameLine(rightSideX);
	if(ImGui::ColorButton("WarningBtn", ImVec4(0.85f, 0.92f, 0.f, 1.f)))
		filter |= (int32)LogFilter::Warn;

	rightSideX -= ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 3;
	ImGui::SameLine(rightSideX);
	if(ImGui::ColorButton("InfoBtn", ImVec4(0.4f, 0.9f, 0.f, 1.f)))
		filter |= (int32)LogFilter::Info;


	ImGui::Separator();

	if (ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		if (clear)
			Clear();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		vector<LogMessage> messages; 

		for (auto logMsg : _messages)
		{
			messages.push_back(logMsg.second);
		}
		
		sort(messages.begin(), messages.end());

		for (auto logMsg : messages)
		{
			if ((logMsg.type & _msgFilter) == false)
				continue;

			ImVec4 color(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			if (logMsg.type & LogFilter::Info)
				color = ImVec4(0.4f, 0.9f, 0.f, 1.f);
			else if (logMsg.type & LogFilter::Warn)
				color = ImVec4(0.85f, 0.92f, 0.f, 1.f);
			else if (logMsg.type & LogFilter::Error)
				color = ImVec4(1.f, 0.f, 0.f, 1.f);

			string colorStr = "[" + GUI->EnumToString(logMsg.type) + "]";


			ImGui::TextColored(color, colorStr.c_str());
			ImGui::SameLine();
			ImGui::Text(logMsg.time.c_str());
			ImGui::SameLine();
			ImGui::Text(logMsg.msg.c_str());
			ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize("오른쪽에 글자").x - ImGui::GetStyle().ItemSpacing.x);

			std::string countStr = to_string(logMsg.count);
			int countLength = static_cast<int>(countStr.length());

			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
			ImVec2 buttonSize((countLength * 30.0f) * 0.5f, 15.f);
			float buttonXPos = (ImGui::GetWindowWidth() - buttonSize.x) - 20.f; 
			float buttonYPos = ImGui::GetCursorPos().y + 5.0f; 
			ImGui::SetCursorPos(ImVec2(buttonXPos, buttonYPos));
			ImGui::Button("##logCount", buttonSize);
			
			ImGui::PopStyleVar();
			ImVec2 buttonPos = ImGui::GetItemRectMin();
			ImVec2 textPosition = ImVec2(buttonPos.x + (buttonSize.x - ImGui::CalcTextSize(countStr.c_str()).x) * 0.5f, buttonPos.y + (buttonSize.y - ImGui::CalcTextSize(countStr.c_str()).y) * 0.5f);
			ImGui::GetWindowDrawList()->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), countStr.c_str());
		}
		
		ImGui::PopStyleVar();

		if (_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
	ImGui::End();
}



void LogWindow::ShowLog()
{
	Draw("Log", &_logOpen);

}
