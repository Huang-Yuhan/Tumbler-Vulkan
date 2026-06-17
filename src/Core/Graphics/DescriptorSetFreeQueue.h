#pragma once

#include <vulkan/vulkan.h>

#include <algorithm>
#include <vector>

class DescriptorSetFreeQueue {
public:
    bool Enqueue(VkDescriptorSet descriptorSet) {
        if (descriptorSet == VK_NULL_HANDLE) {
            return false;
        }

        if (std::ranges::find(PendingDescriptorSets, descriptorSet) != PendingDescriptorSets.end()) {
            return false;
        }

        PendingDescriptorSets.push_back(descriptorSet);
        return true;
    }

    void Clear() { PendingDescriptorSets.clear(); }

    [[nodiscard]] bool Empty() const { return PendingDescriptorSets.empty(); }

    [[nodiscard]] size_t Size() const { return PendingDescriptorSets.size(); }

    [[nodiscard]] const std::vector<VkDescriptorSet>& GetPendingDescriptorSets() const { return PendingDescriptorSets; }

private:
    std::vector<VkDescriptorSet> PendingDescriptorSets;
};
