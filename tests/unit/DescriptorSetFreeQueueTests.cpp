#include <gtest/gtest.h>

#include "Core/Graphics/DescriptorSetFreeQueue.h"

#include <cstdint>

namespace {
VkDescriptorSet MakeFakeDescriptorSet(uint64_t value)
{
#if defined(VK_USE_64_BIT_PTR_DEFINES)
    return reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(value));
#else
    return static_cast<VkDescriptorSet>(value);
#endif
}
}

TEST(DescriptorSetFreeQueueTests, IgnoresNullDescriptorSet) {
    DescriptorSetFreeQueue queue;

    EXPECT_FALSE(queue.Enqueue(VK_NULL_HANDLE));
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(DescriptorSetFreeQueueTests, DeduplicatesQueuedDescriptorSets) {
    DescriptorSetFreeQueue queue;
    const VkDescriptorSet descriptorSet = MakeFakeDescriptorSet(1);

    EXPECT_TRUE(queue.Enqueue(descriptorSet));
    EXPECT_FALSE(queue.Enqueue(descriptorSet));
    EXPECT_EQ(queue.Size(), 1u);
}

TEST(DescriptorSetFreeQueueTests, PreservesUniqueInsertionOrderAndClears) {
    DescriptorSetFreeQueue queue;
    const VkDescriptorSet first = MakeFakeDescriptorSet(1);
    const VkDescriptorSet second = MakeFakeDescriptorSet(2);
    const VkDescriptorSet third = MakeFakeDescriptorSet(3);

    EXPECT_TRUE(queue.Enqueue(first));
    EXPECT_TRUE(queue.Enqueue(second));
    EXPECT_TRUE(queue.Enqueue(third));

    const auto& pending = queue.GetPendingDescriptorSets();
    ASSERT_EQ(pending.size(), 3u);
    EXPECT_EQ(pending[0], first);
    EXPECT_EQ(pending[1], second);
    EXPECT_EQ(pending[2], third);

    queue.Clear();
    EXPECT_TRUE(queue.Empty());
}
