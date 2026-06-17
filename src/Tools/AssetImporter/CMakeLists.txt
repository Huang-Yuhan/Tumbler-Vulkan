cmake_minimum_required(VERSION 3.28)

# TumblerImporter — 资产导入 CLI 工具
# 独立可执行文件，不依赖 Vulkan / GLFW / AppWindow / TumblerCore

find_package(nlohmann_json CONFIG REQUIRED)

set(IMPORTER_SOURCES
    main.cpp
    AssetImporter.cpp
    MeshImporter.cpp
    TextureImporter.cpp
    SceneSerializer.cpp
    "${CMAKE_SOURCE_DIR}/src/Core/Utils/StbImplementations.cpp"
)

add_executable(TumblerImporter ${IMPORTER_SOURCES})

target_include_directories(TumblerImporter PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    ${Stb_INCLUDE_DIR}
)

target_link_libraries(TumblerImporter PRIVATE
    tinyobjloader::tinyobjloader
    nlohmann_json::nlohmann_json
)
