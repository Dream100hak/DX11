#include "pch.h"
#include "LogWindow.h"

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
	_buf.clear();
	_lineOffsets.clear();
	_lineOffsets.push_back(0);
}

void LogWindow::AddLog(const char* fmt, ...) IM_FMTARGS(2)
{
	int oldSize = _buf.size();
	va_list args;
	va_start(args, fmt);
	_buf.appendfv(fmt, args);
	va_end(args);
	for (int newSize = _buf.size(); oldSize < newSize; oldSize++)
		if (_buf[oldSize] == '\n')
			_lineOffsets.push_back(oldSize + 1);
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

	if (ImGui::SmallButton("[Debug] Add 5 entries"))
	{
		static int counter = 0;
		const char* categories[3] = { "info", "warn", "error" };
		const char* words[] = { "Bumfuzzled", "Cattywampus", "Snickersnee", "Abibliophobia", "Absquatulate", "Nincompoop", "Pauciloquent" };
		for (int n = 0; n < 5; n++)
		{
			const char* category = categories[counter % IM_ARRAYSIZE(categories)];
			const char* word = words[counter % IM_ARRAYSIZE(words)];
			AddLog("[%05d] [%s] Hello, current time is %.1f, here's a word: '%s'\n",
				ImGui::GetFrameCount(), category, ImGui::GetTime(), word);
			counter++;
		}
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

	// 오른쪽에 배치할 UI 요소의 가로 위치 계산
	float rightSideX = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;

	// Info 버튼을 오른쪽 끝에 배치
	ImGui::SameLine(rightSideX);
	ImGui::ColorButton("ErrorBtn", ImVec4(1.f, 0.f, 0.f, 1.f));

	// Warning 버튼을 오른쪽에 배치
	rightSideX -= ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 3;
	ImGui::SameLine(rightSideX);
	ImGui::ColorButton("WarningBtn", ImVec4(0.4f, 0.9f, 0.f, 1.f));

	// Error 버튼을 오른쪽에 배치
	rightSideX -= ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 3;
	ImGui::SameLine(rightSideX);
	ImGui::ColorButton("InfoBtn", ImVec4(0.85f, 0.92f, 0.f, 1.f));


	ImGui::Separator();

	if (ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		if (clear)
			Clear();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		const char* buf = _buf.begin();
		const char* buf_end = _buf.end();
		if (_filter.IsActive())
		{
			// In this example we don't use the clipper when Filter is enabled.
			// This is because we don't have random access to the result of our filter.
			// A real application processing logs with ten of thousands of entries may want to store the result of
			// search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
			for (int line_no = 0; line_no < _lineOffsets.Size; line_no++)
			{
				const char* line_start = buf + _lineOffsets[line_no];
				const char* line_end = (line_no + 1 < _lineOffsets.Size) ? (buf + _lineOffsets[line_no + 1] - 1) : buf_end;
				if (_filter.PassFilter(line_start, line_end))
					ImGui::TextUnformatted(line_start, line_end);
			}
		}
		else
		{
			// The simplest and easy way to display the entire buffer:
			//   ImGui::TextUnformatted(buf_begin, buf_end);
			// And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
			// to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
			// within the visible area.
			// If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
			// on your side is recommended. Using ImGuiListClipper requires
			// - A) random access into your data
			// - B) items all being the  same height,
			// both of which we can handle since we have an array pointing to the beginning of each line of text.
			// When using the filter (in the block of code above) we don't have random access into the data to display
			// anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
			// it possible (and would be recommended if you want to search through tens of thousands of entries).
			ImGuiListClipper clipper;
			clipper.Begin(_lineOffsets.Size);
			while (clipper.Step())
			{
				for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
				{
					const char* line_start = buf + _lineOffsets[line_no];
					const char* line_end = (line_no + 1 < _lineOffsets.Size) ? (buf + _lineOffsets[line_no + 1] - 1) : buf_end;
					ImGui::TextUnformatted(line_start, line_end);
				}
			}
			clipper.End();
		}
		ImGui::PopStyleVar();

		// Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
		// Using a scrollbar or mouse-wheel will take away from the bottom edge.
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
