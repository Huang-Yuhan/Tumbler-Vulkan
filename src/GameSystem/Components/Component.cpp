#include "Component.h"
// 注意：通常建议把 Component.h 放在第一行，以检查头文件自包含性

// 如果在这个 cpp 里你需要访问 FActor 的成员（比如 Name），才需要包含这个
// 如果只是指针赋值，其实连这一行都不需要

Component::Component() = default;
Component::~Component() = default;

// 3. 在这里实现 SetOwner
void Component::SetOwner(FActor* InOwner)
{
    Owner = InOwner;
}

// 4. 在这里实现 GetOwner
FActor* Component::GetOwner()
{
    return Owner;
}

// 5. 实现 Update (虽然是空的)
void Component::Update(float DeltaTime)
{
    // 留空，等待子类重写
}