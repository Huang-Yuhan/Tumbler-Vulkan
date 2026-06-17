#include "CStaticMesh.h"

namespace Tumbler {

void CStaticMesh::AddMaterialOverride(const std::string& sourcePath) {
    FMaterialRef ref;
    ref.SourcePath = sourcePath;
    MaterialOverrides.push_back(std::move(ref));
}

} // namespace Tumbler
