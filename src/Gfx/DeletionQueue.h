#pragma once

#include <functional>
#include <vector>
#include <vulkan/vulkan.h>

namespace Tumbler {

// Timeline-semaphore-based deferred GPU resource deletion.
// Resources enqueued here are destroyed only after the GPU has
// signaled that the corresponding submission has completed.
class DeletionQueue {
public:
    void Init(VkDevice device);
    void Shutdown();  // vkDeviceWaitIdle + drain all

    // Enqueue a destruction callback. Returns the retire value for
    // the current "inflight" set (call AdvanceSubmitCounter() next
    // to associate it with a submission).
    uint64_t Enqueue(std::function<void()> deleter);

    // Non-blocking: destroy resources whose retire value has been completed.
    void Flush();

    // Call before vkQueueSubmit to get the next signal value for
    // the timeline semaphore.
    uint64_t AdvanceSubmitCounter();

    VkSemaphore GetTimelineSemaphore() const { return m_Timeline; }

private:
    VkDevice     m_Device    = VK_NULL_HANDLE;
    VkSemaphore  m_Timeline  = VK_NULL_HANDLE;
    uint64_t     m_NextValue = 1;
    uint64_t     m_CompletedValue = 0;

    struct Entry {
        uint64_t retireValue;
        std::function<void()> deleter;
    };
    std::vector<Entry> m_Pending;
};

} // namespace Tumbler
