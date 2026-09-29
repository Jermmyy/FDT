#include <imgui_internal.h>
#include "keynav.h"

#include <Windows.h>
#include <cstdio>

// Debug variables
static ImVec2 g_DebugRayStart = { 0,0 };
static ImVec2 g_DebugRayEnd = { 0,0 };
static ImGuiDir g_DebugLastDir = ImGuiDir_None;
static float g_DebugTimer = 0.0f;

// Returns all leaf nodes under "node" that have a visible window
static void CollectLeafNodes(ImGuiDockNode* node, ImVector<ImGuiDockNode*>& out)
{
	if (!node) return;
	if (node->IsLeafNode())
	{
		if (node->VisibleWindow != nullptr)
			out.push_back(node);
		return;
	}
	CollectLeafNodes(node->ChildNodes[0], out);
	CollectLeafNodes(node->ChildNodes[1], out);
}

// Find the root of the dockspace that owns "window"
static ImGuiDockNode* GetRootDockNode(ImGuiWindow* window)
{
	ImGuiDockNode* node = window->DockNode;
	if (!node) return nullptr;
	while (node->ParentNode)
		node = node->ParentNode;
	return node;
}

// Returns the best candidate leaf node in direction "dir" from "current"
// dir: ImGuiDir_Left, ImGuiDir_Right, ImGuiDir_Up, ImGuiDir_Down

ImGuiDockNode* FindNeighborDockNode(ImGuiDockNode* current, ImGuiDir dir)
{
	if (!current) return nullptr;

	ImGuiDockNode* root = current;
	while (root->ParentNode) root = root->ParentNode;

	ImVector<ImGuiDockNode*> leaves;
	CollectLeafNodes(root, leaves);

	ImVec2 cur_center = ImVec2(current->Pos.x + current->Size.x * 0.5f, current->Pos.y + current->Size.y * 0.5f);

	ImGuiDockNode* best = nullptr;
	float best_score = FLT_MAX;

	for (ImGuiDockNode* candidate : leaves)
	{
		if (candidate == current) continue;

		ImVec2 cand_center = ImVec2(candidate->Pos.x + candidate->Size.x * 0.5f, candidate->Pos.y + candidate->Size.y * 0.5f);
		float dx = cand_center.x - cur_center.x;
		float dy = cand_center.y - cur_center.y;

		bool in_dir = false;
		switch (dir)
		{
		case ImGuiDir_Left:  in_dir = dx < -1.0f; break;
		case ImGuiDir_Right: in_dir = dx > 1.0f;  break;
		case ImGuiDir_Up:    in_dir = dy < -1.0f; break;
		case ImGuiDir_Down:  in_dir = dy > 1.0f;  break;
		default: break;
		}
		if (!in_dir) continue;

		if (dir == ImGuiDir_Up || dir == ImGuiDir_Down) {
			bool overlap = (current->Pos.x < (candidate->Pos.x + candidate->Size.x)) &&
				((current->Pos.x + current->Size.x) > candidate->Pos.x);
			if (!overlap) continue;
		}
		else {
			bool overlap = (current->Pos.y < (candidate->Pos.y + candidate->Size.y)) &&
				((current->Pos.y + current->Size.y) > candidate->Pos.y);
			if (!overlap) continue;
		}

		float primary = (dir == ImGuiDir_Left || dir == ImGuiDir_Right) ? ImFabs(dx) : ImFabs(dy);
		float secondary = (dir == ImGuiDir_Left || dir == ImGuiDir_Right) ? ImFabs(dy) : ImFabs(dx);
		float score = primary + secondary * 20.0f;

		if (score < best_score)
		{
			best_score = score;
			best = candidate;

			g_DebugRayStart = cur_center;
			g_DebugRayEnd = cand_center;
			g_DebugLastDir = dir;
			g_DebugTimer = 2.0f;
		}
	}

	return best;
}

void DrawKeyNavDebug()
{
	if (g_DebugTimer <= 0.0f) return;

	ImGuiIO& io = ImGui::GetIO();
	g_DebugTimer -= io.DeltaTime;

	ImDrawList* draw_list = ImGui::GetForegroundDrawList();

	// Draw the "Search Center"
	draw_list->AddCircleFilled(g_DebugRayStart, 6.0f, IM_COL32(255, 255, 0, 255));

	// Draw the "Found Neighbor"
	draw_list->AddCircleFilled(g_DebugRayEnd, 6.0f, IM_COL32(0, 255, 0, 255));

	// Draw a thick line connecting them
	draw_list->AddLine(g_DebugRayStart, g_DebugRayEnd, IM_COL32(0, 255, 0, 200), 3.0f);

	// Add text showing the direction
	const char* dir_names[] = { "None", "Left", "Right", "Up", "Down" };
	char buf[64];
	sprintf_s(buf, "Nav: %s", dir_names[g_DebugLastDir + 1]);
	draw_list->AddText(ImVec2(g_DebugRayStart.x + 10, g_DebugRayStart.y + 10), IM_COL32(255, 255, 255, 255), buf);
}

void HandleDockNavKeys()
{
	ImGuiContext& g = *GImGui;
	ImGuiIO& io = ImGui::GetIO();

	ImGuiDir dir = ImGuiDir_None;
	if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) dir = ImGuiDir_Left;
	if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) dir = ImGuiDir_Right;
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) dir = ImGuiDir_Up;
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) dir = ImGuiDir_Down;

	if (dir == ImGuiDir_None) return;

	if (!io.KeyCtrl || io.WantTextInput) return;

	ImGuiWindow* nav_window = g.NavWindow;
	if (!nav_window) return;

	while (nav_window->DockNode == nullptr && nav_window->DockId == 0 && nav_window->ParentWindow != nullptr)
		nav_window = nav_window->ParentWindow;

	ImGuiDockNode* current_node = nav_window->DockNode;
	if (!current_node && nav_window->DockId != 0)
		current_node = ImGui::DockBuilderGetNode(nav_window->DockId);

	if (!current_node) {
		char buf[256];
		sprintf_s(buf, "[KeyNav] FAIL: '%s' has no DockNode/ID.\n", nav_window->Name);
		OutputDebugStringA(buf);
		return;
	}

	ImGuiDockNode* target = FindNeighborDockNode(current_node, dir);
	if (target && target->VisibleWindow)
	{
		OutputDebugStringA("[KeyNav] Success! Changing focus.\n");
		ImGui::FocusWindow(target->VisibleWindow);
		if (target->TabBar)
			target->TabBar->NextSelectedTabId = target->VisibleWindow->TabId;
	}
}