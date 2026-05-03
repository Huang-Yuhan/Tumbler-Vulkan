#include "DebugWindowHost.h"
#include <algorithm>
#include <imgui.h>

void DebugWindowHost::RegisterSection(Section section)
{
    mSections.push_back(std::move(section));
    mSorted = false;
}

void DebugWindowHost::Draw()
{
    if (mSections.empty()) {
        return;
    }

    if (!mSorted) {
        std::sort(mSections.begin(), mSections.end(),
            [](const Section& a, const Section& b) { return a.Priority < b.Priority; });
        mSorted = true;
    }

    if (!ImGui::Begin("Render Debug")) {
        ImGui::End();
        return;
    }

    constexpr ImGuiTreeNodeFlags sectionFlags = ImGuiTreeNodeFlags_DefaultOpen;

    for (const auto& section : mSections) {
        ImGuiTreeNodeFlags flags = sectionFlags;
        if (!section.DefaultOpen) {
            flags &= ~ImGuiTreeNodeFlags_DefaultOpen;
        }

        if (ImGui::CollapsingHeader(section.Name.c_str(), flags)) {
            if (section.Draw) {
                section.Draw();
            }
        }
    }

    ImGui::End();
}
