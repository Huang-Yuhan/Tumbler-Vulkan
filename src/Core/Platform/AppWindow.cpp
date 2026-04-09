//
// Created by Icecream_Sarkaz on 2026/1/15.
//

#include "AppWindow.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "Core/Utils/Log.h"
#include "Core/Utils/VulkanUtils.h"

namespace {
bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    return std::any_of(extensions.begin(), extensions.end(), [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

void GlfwErrorCallback(int code, const char* description)
{
    LOG_ERROR("GLFW error {}: {}", code, description ? description : "Unknown");
}

const char* GetGlfwPlatformName(int platform)
{
    switch (platform) {
        case GLFW_PLATFORM_WIN32: return "Win32";
        case GLFW_PLATFORM_COCOA: return "Cocoa";
        case GLFW_PLATFORM_WAYLAND: return "Wayland";
        case GLFW_PLATFORM_X11: return "X11";
        case GLFW_PLATFORM_NULL: return "Null";
        case GLFW_ANY_PLATFORM: return "Any";
        default: return "Unknown";
    }
}

bool HasRequiredVulkanInstanceExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    return glfwExtensions != nullptr && glfwExtensionCount > 0;
}

void LogMissingVulkanSurfaceExtensionContext()
{
    const char* description = nullptr;
    const int errorCode = glfwGetError(&description);
    const int platform = glfwGetPlatform();

    LOG_ERROR("GLFW platform is {}. Vulkan surface extension query failed. GLFW error {}: {}",
              GetGlfwPlatformName(platform),
              errorCode,
              description ? description : "Unknown");

    uint32_t extensionCount = 0;
    const VkResult enumerateResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (enumerateResult != VK_SUCCESS) {
        LOG_ERROR("vkEnumerateInstanceExtensionProperties failed while collecting diagnostics: {}",
                  VkUtils::VkResultToString(enumerateResult));
        return;
    }

    if (extensionCount == 0) {
        LOG_WARN("No Vulkan instance extensions were reported by the loader.");
        return;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to fetch Vulkan instance extension names for diagnostics.");
        return;
    }

    LOG_INFO("Available Vulkan instance extensions on this system:");
    for (const VkExtensionProperties& extension : extensions) {
        LOG_INFO("  {}", extension.extensionName);
    }

    if (platform == GLFW_PLATFORM_X11) {
        constexpr const char* xcbSurfaceExtension = "VK_KHR_xcb_surface";
        constexpr const char* xlibSurfaceExtension = "VK_KHR_xlib_surface";
        const bool hasXcbSurface = HasExtension(extensions, xcbSurfaceExtension);
        const bool hasXlibSurface = HasExtension(extensions, xlibSurfaceExtension);
        if (!hasXcbSurface && !hasXlibSurface) {
            LOG_CRITICAL("No X11 Vulkan WSI extensions were exposed. Expected at least one of {} or {}.",
                         xcbSurfaceExtension,
                         xlibSurfaceExtension);
            LOG_CRITICAL("This is usually a system Vulkan/driver issue, not an engine code issue.");
        }
    } else if (platform == GLFW_PLATFORM_WAYLAND) {
        constexpr const char* waylandSurfaceExtension = "VK_KHR_wayland_surface";
        if (!HasExtension(extensions, waylandSurfaceExtension)) {
            LOG_CRITICAL("No Wayland Vulkan WSI extension was exposed. Expected {}.",
                         waylandSurfaceExtension);
            LOG_CRITICAL("This is usually a system Vulkan/driver issue, not an engine code issue.");
        }
    }
}

bool TryInitializeGlfwForVulkan(int platformHint)
{
    glfwInitHint(GLFW_PLATFORM, platformHint);
    if (!glfwInit()) {
        return false;
    }

    if (!glfwVulkanSupported()) {
        const char* description = nullptr;
        const int errorCode = glfwGetError(&description);
        LOG_ERROR("GLFW reports Vulkan is unsupported on platform {}. Error {}: {}",
                  GetGlfwPlatformName(glfwGetPlatform()),
                  errorCode,
                  description ? description : "Unknown");
        glfwTerminate();
        return false;
    }

    if (HasRequiredVulkanInstanceExtensions()) {
        LOG_INFO("GLFW initialized on platform {} with Vulkan surface extensions available.",
                 GetGlfwPlatformName(glfwGetPlatform()));
        return true;
    }

    LogMissingVulkanSurfaceExtensionContext();
    glfwTerminate();
    return false;
}

bool IsWaylandSession()
{
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if (sessionType != nullptr && std::strcmp(sessionType, "wayland") == 0) {
        return true;
    }

    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    return waylandDisplay != nullptr && waylandDisplay[0] != '\0';
}

bool EnvEquals(const char* name, const char* expected)
{
    const char* value = std::getenv(name);
    return value != nullptr && std::strcmp(value, expected) == 0;
}

bool EnvContains(const char* name, const char* needle)
{
    const char* value = std::getenv(name);
    return value != nullptr && std::strstr(value, needle) != nullptr;
}

bool SetEnvIfDifferent(const char* name, const char* value)
{
    const char* current = std::getenv(name);
    if (current != nullptr && std::strcmp(current, value) == 0) {
        return false;
    }

#if defined(_WIN32)
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, 1) == 0;
#endif
}

bool UnsetEnvIfPresent(const char* name)
{
    if (std::getenv(name) == nullptr) {
        return false;
    }

#if defined(_WIN32)
    return _putenv_s(name, "") == 0;
#else
    return unsetenv(name) == 0;
#endif
}

bool RestoreEnvFromBackup(const char* targetName, const char* backupName)
{
    const char* backup = std::getenv(backupName);
    if (backup == nullptr || backup[0] == '\0') {
        return false;
    }

    return SetEnvIfDifferent(targetName, backup);
}

bool IsSnapCodeGtkEnvironment()
{
    return EnvEquals("SNAP_NAME", "code")
        || EnvContains("GTK_EXE_PREFIX", "/snap/code/")
        || EnvContains("GTK_PATH", "/snap/code/")
        || EnvContains("GSETTINGS_SCHEMA_DIR", "/snap/code/")
        || EnvContains("XDG_DATA_DIRS", "/snap/code/")
        || EnvContains("XDG_DATA_HOME", "/snap/code/")
        || EnvContains("GIO_MODULE_DIR", "/snap/code/");
}

void SanitizeWaylandGtkEnvironment()
{
#if defined(__linux__)
    if (!IsWaylandSession() || !IsSnapCodeGtkEnvironment()) {
        return;
    }

    bool changed = false;

    changed |= SetEnvIfDifferent("GDK_BACKEND", "wayland");
    changed |= RestoreEnvFromBackup("XDG_DATA_DIRS", "XDG_DATA_DIRS_VSCODE_SNAP_ORIG");
    changed |= RestoreEnvFromBackup("XDG_CONFIG_DIRS", "XDG_CONFIG_DIRS_VSCODE_SNAP_ORIG");

    constexpr std::array<const char*, 30> envVarsToUnset = {
        "SNAP",
        "SNAP_NAME",
        "SNAP_REVISION",
        "SNAP_ARCH",
        "SNAP_COOKIE",
        "SNAP_COMMON",
        "SNAP_CONTEXT",
        "SNAP_DATA",
        "SNAP_EUID",
        "SNAP_EXE",
        "SNAP_INSTANCE_NAME",
        "SNAP_LAUNCHER_ARCH_TRIPLET",
        "SNAP_LIBRARY_PATH",
        "SNAP_REAL_HOME",
        "SNAP_UID",
        "SNAP_USER_COMMON",
        "SNAP_USER_DATA",
        "SNAP_VERSION",
        "GTK_EXE_PREFIX",
        "GTK_PATH",
        "GDK_PIXBUF_MODULEDIR",
        "GDK_PIXBUF_MODULE_FILE",
        "GTK_IM_MODULE_FILE",
        "GTK_MODULES",
        "GSETTINGS_SCHEMA_DIR",
        "GIO_MODULE_DIR",
        "GIO_LAUNCHED_DESKTOP_FILE",
        "GIO_LAUNCHED_DESKTOP_FILE_PID",
        "CHROME_DESKTOP",
        "XDG_DATA_HOME"
    };

    for (const char* envVar : envVarsToUnset) {
        changed |= UnsetEnvIfPresent(envVar);
    }

    constexpr std::array<const char*, 9> vscodeVarsToUnset = {
        "VSCODE_CLI",
        "VSCODE_CODE_CACHE_PATH",
        "VSCODE_CRASH_REPORTER_PROCESS_TYPE",
        "VSCODE_CWD",
        "VSCODE_ESM_ENTRYPOINT",
        "VSCODE_HANDLES_UNCAUGHT_ERRORS",
        "VSCODE_IPC_HOOK",
        "VSCODE_NLS_CONFIG",
        "VSCODE_PID"
    };

    for (const char* envVar : vscodeVarsToUnset) {
        changed |= UnsetEnvIfPresent(envVar);
    }

    changed |= UnsetEnvIfPresent("ELECTRON_NO_ATTACH_CONSOLE");
    changed |= UnsetEnvIfPresent("ELECTRON_RUN_AS_NODE");

    if (changed) {
        LOG_WARN("Detected Snap Code GTK environment on Wayland. Sanitizing GTK/GIO environment for libdecor window decorations.");
    }
#endif
}
}

AppWindow::AppWindow(const AppWindowConfig& config)
    : WindowConfig(config)
{
    Init();
}

AppWindow::~AppWindow()
{
    Shutdown();
}

void AppWindow::Init()
{
    SanitizeWaylandGtkEnvironment();
    glfwSetErrorCallback(GlfwErrorCallback);

    const bool waylandSession = IsWaylandSession();
    if (waylandSession) {
        LOG_INFO("Wayland session detected. Preferring GLFW Wayland platform first.");
    }

    bool initialized = false;

#if defined(__linux__)
    if (waylandSession) {
        initialized = TryInitializeGlfwForVulkan(GLFW_PLATFORM_WAYLAND);
        if (!initialized) {
            LOG_WARN("Wayland GLFW initialization failed. Falling back to automatic platform selection.");
        }
    }
#endif

    if (!initialized) {
        initialized = TryInitializeGlfwForVulkan(GLFW_ANY_PLATFORM);
    }

    if (!initialized) {
#if defined(__linux__)
        LOG_WARN("Retrying GLFW initialization with X11 platform hint for Vulkan compatibility.");
        initialized = TryInitializeGlfwForVulkan(GLFW_PLATFORM_X11);
        if (!initialized) {
            throw std::runtime_error("Failed to initialize GLFW with Vulkan surface extensions");
        }
#else
        throw std::runtime_error("Failed to initialize GLFW with Vulkan surface extensions");
#endif
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, WindowConfig.Resizable ? GLFW_TRUE : GLFW_FALSE);

    Handle = glfwCreateWindow(static_cast<int>(WindowConfig.Width),
                              static_cast<int>(WindowConfig.Height),
                              WindowConfig.Title.c_str(),
                              nullptr,
                              nullptr);
    if (!Handle) {
        glfwTerminate();
        LOG_ERROR("Failed to create GLFW window");
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(Handle, this);
    
    glfwSetFramebufferSizeCallback(Handle, FramebufferResizeCallback);

    LOG_INFO("Created window: {} ({}x{})", WindowConfig.Title, WindowConfig.Width, WindowConfig.Height);

}

void AppWindow::Shutdown()
{
    if (Handle) {
        glfwDestroyWindow(Handle);
        Handle = nullptr;
    }
    glfwTerminate();
    LOG_INFO("Destroyed GLFW window");
}

void AppWindow::PollEvents()
{
    glfwPollEvents();
}

bool AppWindow::ShouldClose() const
{
    return glfwWindowShouldClose(Handle);
}

void AppWindow::GetFramebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(Handle, &width, &height);
}

VkSurfaceKHR AppWindow::CreateSurface(VkInstance instance)
{
    VkSurfaceKHR surface;

    VK_CHECK(glfwCreateWindowSurface(instance, Handle, nullptr, &surface));
    return surface;
}

void AppWindow::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto appWindow = reinterpret_cast<AppWindow*>(glfwGetWindowUserPointer(window));
    if (appWindow) {
        appWindow->bFramebufferResized = true;
        LOG_INFO("Framebuffer resized to {}x{}", width, height);
    }
}
