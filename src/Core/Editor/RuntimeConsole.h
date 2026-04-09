#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct ImGuiInputTextCallbackData;
class InputManager;

enum class EConsoleMessageType {
    Command,
    Info,
    Warning,
    Error
};

using ConsoleAutocompleteHandler = std::function<std::vector<std::string>(const std::vector<std::string>& args, size_t activeArgIndex)>;

struct ConsoleCommandDefinition {
    std::string Name;
    std::string Usage;
    std::string Description;
    ConsoleAutocompleteHandler AutocompleteHandler;
    std::function<void(const std::vector<std::string>& args)> Handler;
};

class RuntimeConsole {
public:
    RuntimeConsole() = default;
    RuntimeConsole(const RuntimeConsole&) = delete;
    RuntimeConsole& operator=(const RuntimeConsole&) = delete;
    RuntimeConsole(RuntimeConsole&&) = delete;
    RuntimeConsole& operator=(RuntimeConsole&&) = delete;

    void Initialize(InputManager* inputManager);
    void TickInput();
    void Draw();
    void RegisterCommand(const ConsoleCommandDefinition& def);
    void AddMessage(EConsoleMessageType type, std::string text);
    [[nodiscard]] bool IsOpen() const { return bIsOpen; }

private:
    struct ConsoleMessage {
        EConsoleMessageType Type = EConsoleMessageType::Info;
        std::string Text;
    };

    static constexpr size_t MAX_INPUT_LENGTH = 256;
    static constexpr size_t MAX_MESSAGE_COUNT = 200;

    InputManager* Input = nullptr;
    std::array<char, MAX_INPUT_LENGTH> InputBuffer{};
    std::vector<ConsoleMessage> Messages;
    std::vector<std::string> History;
    int HistoryIndex = -1;
    bool bIsOpen = false;
    bool bFocusInput = false;
    bool bScrollToBottom = false;
    bool bBuiltinsRegistered = false;

    std::vector<ConsoleCommandDefinition> Commands;
    std::unordered_map<std::string, size_t> CommandLookup;

    void RegisterBuiltins();
    void ClearMessages();
    void ExecuteCurrentInput();
    void ExecuteCommand(const std::string& commandLine);
    [[nodiscard]] std::vector<std::string> TokenizeCommand(const std::string& commandLine) const;
    [[nodiscard]] static std::string NormalizeCommandName(const std::string& name);
    static int InputTextCallback(ImGuiInputTextCallbackData* data);
    int OnInputTextCompletion(ImGuiInputTextCallbackData* data);
    int OnInputTextHistory(ImGuiInputTextCallbackData* data);
};
