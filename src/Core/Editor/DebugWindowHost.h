#pragma once

#include <functional>
#include <string>
#include <vector>

class DebugWindowHost {
public:
    using SectionDrawFn = std::function<void()>;

    struct Section {
        std::string Name;
        SectionDrawFn Draw;
        int Priority = 0;           // Lower = drawn first
        bool DefaultOpen = true;    // CollapsingHeader initial state
    };

    void RegisterSection(Section section);
    void Draw();                    // Call once per frame inside ImGui frame

private:
    std::vector<Section> mSections;
    bool mSorted = true;
};
