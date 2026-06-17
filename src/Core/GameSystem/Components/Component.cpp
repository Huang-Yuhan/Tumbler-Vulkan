#include "Component.h"

namespace Tumbler {

Component::Component() = default;
Component::~Component() = default;

void Component::SetOwner(FActor* InOwner) {
    Owner = InOwner;
}

FActor* Component::GetOwner() {
    return Owner;
}

void Component::Update(float DeltaTime) {
    // 留空，等待子类重写
}

} // namespace Tumbler