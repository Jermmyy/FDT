#include "RatePanel.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include <algorithm>
#include <vector>
#include <unordered_map>

void renderRatePanel(const RateClient &client)
{
    const auto &data = client.data();
    if (data.empty())
        return;

    static std::unordered_map<std::string, bool> showChart;
    if (showChart.find(client.name()) == showChart.end())
        showChart[client.name()] = true;
    bool &chart = showChart[client.name()];

    std::vector<double> x(data.size()), y(data.size());
    for (size_t i = 0; i < data.size(); ++i)
    {
        x[i] = (double)i;
        y[i] = data[data.size() - 1 - i].value;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    // ImGuiWindowClass window_class;
    // window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    // ImGui::SetNextWindowClass(&window_class);

     ImGui::Begin(client.name().c_str(), nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(chart ? "Show Raw Data" : "Show Chart"))
        chart = !chart;

    if (chart)
    {
        double y_min = *std::min_element(y.begin(), y.end());
        double y_max = *std::max_element(y.begin(), y.end());
        double margin = (y_max - y_min) * 0.05;

        ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0.00f, 0.00f, 0.00f, 1.00f));
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.00f));

        ImPlot::SetNextAxisLimits(ImAxis_X1, 0, (double)data.size() - 1, ImGuiCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, y_min - margin, y_max + margin, ImGuiCond_Always);

        std::string plotTitle = client.name() + " over Time";
        if (ImPlot::BeginPlot(plotTitle.c_str(), ImVec2(-1, -1)))
        {
            ImPlot::SetupAxes("Day", "Rate (%)");

            switch (client.chartType())
            {
            case ChartType::Line:
                ImPlot::PlotLine(client.name().c_str(), x.data(), y.data(), (int)data.size());
                break;
            case ChartType::Bar:
                ImPlot::PlotBars(client.name().c_str(), x.data(), y.data(), (int)data.size(), 0.67);
                break;
            case ChartType::Step:
                ImPlot::PlotStairs(client.name().c_str(), x.data(), y.data(), (int)data.size());
                break;
            }

            if (ImPlot::IsPlotHovered())
            {
                ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                int idx = (int)std::round(mouse.x);
                if (idx >= 0 && idx < (int)data.size())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Date: %s", data[data.size() - 1 - idx].date.c_str());
                    ImGui::Text("Value: %.4f", data[data.size() - 1 - idx].value);
                    ImGui::EndTooltip();
                }
            }

            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor(2);
    }
    else
    {
        std::string tableId = client.name() + " Table";
        ImGui::BeginTable(tableId.c_str(), 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
        ImGui::TableSetupColumn("Date");
        ImGui::TableSetupColumn("Rate (%)");
        ImGui::TableHeadersRow();

        for (const auto &pt : data)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(pt.date.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", pt.value);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}