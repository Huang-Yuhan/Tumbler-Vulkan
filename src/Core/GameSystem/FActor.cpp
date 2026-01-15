#include "FActor.h" // 包含对应的头文件

// 构造函数实现 (默认实现)
FActor::FActor() = default;

// 析构函数实现 (默认实现)
// 放在这里的好处是：当 vector 清理 unique_ptr 时，
// 不需要所有 include FActor.h 的人都知道 Component 的具体析构逻辑
FActor::~FActor() = default;

// 静态工厂方法实现
FActor* FActor::CreateActor(const std::string& name) {
    // 这里的 new FActor() 可以调用私有构造函数，因为 CreateActor 是成员函数
    const auto NewActor = new FActor();
    NewActor->Name = name;

    NewActor->Transform.SetOwner(NewActor);

    return NewActor;
}
