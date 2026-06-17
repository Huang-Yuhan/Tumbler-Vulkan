#include "FActor.h"

namespace Tumbler {

FActor::FActor() = default;

FActor::~FActor() = default;

std::unique_ptr<FActor> FActor::CreateActor(const std::string& name) {
    auto NewActor = std::make_unique<FActor>();
    NewActor->Name = name;
    NewActor->Transform.SetOwner(NewActor.get());
    return NewActor;
}

} // namespace Tumbler
