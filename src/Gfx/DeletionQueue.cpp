#include "DeletionQueue.h"
#include "Core/Utils/Log.h"

namespace Tumbler {

void DeletionQueue::Init(VkDevice device) {
    m_Device = device;

    VkSemaphoreTypeCreateInfo typeInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };

    VkSemaphoreCreateInfo semInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &typeInfo,
    };

    vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_Timeline);
    LOG_INFO("DeletionQueue initialized");
}

void DeletionQueue::Shutdown() {
    vkDeviceWaitIdle(m_Device);
    for (auto& entry : m_Pending) entry.deleter();
    m_Pending.clear();

    if (m_Timeline) {
        vkDestroySemaphore(m_Device, m_Timeline, nullptr);
        m_Timeline = VK_NULL_HANDLE;
    }
    LOG_INFO("DeletionQueue shutdown");
}

uint64_t DeletionQueue::Enqueue(std::function<void()> deleter) {
    m_Pending.push_back({m_NextValue, std::move(deleter)});
    return m_NextValue;
}

void DeletionQueue::Flush() {
    // Non-blocking query of GPU progress
    uint64_t completed = 0;
    vkGetSemaphoreCounterValue(m_Device, m_Timeline, &completed);
    m_CompletedValue = completed;

    // Destroy everything whose retire value has been reached
    auto it = std::remove_if(m_Pending.begin(), m_Pending.end(),
        [&](const Entry& e) {
            if (m_CompletedValue >= e.retireValue) {
                e.deleter();
                return true;
            }
            return false;
        });
    m_Pending.erase(it, m_Pending.end());
}

uint64_t DeletionQueue::AdvanceSubmitCounter() {
    return m_NextValue++;
}

} // namespace Tumbler
