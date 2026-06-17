#include "FActor.h"

namespace Tumbler {

FActor::FActor() = default;

FActor::~FActor() = default;

FActor* FActor::CreateActor(const std::string& name) {
    const auto NewActor = new FActor();
    NewActor->Name = name;
    NewActor->Transform.SetOwner(NewActor);
    return NewActor;
}

} // namespace Tumbler
