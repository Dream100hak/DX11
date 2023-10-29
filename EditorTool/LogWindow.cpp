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
	logMsg.type = filter;

	auto nowTime = chrono::system_clock::now();
	auto miltime = chrono::duration_cast<chrono::milliseconds>(nowTime.time_since_epoch()).count();
	
	logMsg.msg = "[" + Utils::ConvertTimeToHHMMSS(miltime) + "]";
	logMsg.msg += " : " + msg;

	_messages.push_back(logMsg);
}

void LogWindow::Draw(const char* title, bool* p_open /*= NULL*/)
{
	ImGui::SetNextWindowPos(ImVec2(800, 551));
	ImGui::SetNextWindowSize(ImVec2(373 * 2, 500));

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
	if(ImGui::ColorButton("ErrorBtn", ImVec4(1.f, 0.f, 0.f, 1.f)))
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

	if (ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		if (clear)
			Clear();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	
		for (auto logMsg : _messages)
		{
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
			ImGui::Text(logMsg.msg.c_str());
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
