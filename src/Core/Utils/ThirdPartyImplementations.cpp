// ThirdPartyImplementations.cpp — 第三方库实现入口
//
// VMA、stb、tinyobjloader 等 header-only 库需要在一个翻译单元中定义实现

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
