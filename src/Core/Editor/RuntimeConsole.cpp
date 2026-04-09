#include "RuntimeConsole.h"

#include "Core/GameSystem/InputManager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <exception>

namespace {
struct ConsoleTokenRange {
    size_t Start = 0;
    size_t End = 0;
    bool Quoted = false;
    std::string Value;
};

std::string TrimCopy(const std::string& text)
{
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(start, end - start);
}

ImVec4 GetConsoleMessageColor(EConsoleMessageType type)
{
    switch (type) {
        case EConsoleMessageType::Command: return ImVec4(0.45f, 0.85f, 1.0f, 1.0f);
        case EConsoleMessageType::Warning: return ImVec4(1.0f, 0.85f, 0.35f, 1.0f);
        case EConsoleMessageType::Error: return ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
        case EConsoleMessageType::Info:
        default: return ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    }
}

bool StartsWithInsensitive(const std::string& value, const std::string& prefix)
{
    if (prefix.size() > value.size()) {
        return false;
    }

    for (size_t index = 0; index < prefix.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index]))
            != std::tolower(static_cast<unsigned char>(prefix[index]))) {
            return false;
        }
    }

    return true;
}

std::string BuildLongestCommonPrefix(const std::vector<std::string>& values)
{
    if (values.empty()) {
        return {};
    }

    std::string prefix = values.front();
    for (size_t valueIndex = 1; valueIndex < values.size() && !prefix.empty(); ++valueIndex) {
        size_t matchLength = 0;
        while (matchLength < prefix.size()
            && matchLength < values[valueIndex].size()
            && std::tolower(static_cast<unsigned char>(prefix[matchLength]))
                == std::tolower(static_cast<unsigned char>(values[valueIndex][matchLength]))) {
            ++matchLength;
        }

        prefix.resize(matchLength);
    }

    return prefix;
}

std::vector<ConsoleTokenRange> TokenizeWithRanges(const std::string& text)
{
    std::vector<ConsoleTokenRange> tokens;
    size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }

        if (index >= text.size()) {
            break;
        }

        ConsoleTokenRange token{};
        if (text[index] == '"') {
            token.Quoted = true;
            ++index;
            token.Start = index;
            while (index < text.size() && text[index] != '"') {
                ++index;
            }
            token.End = index;
            token.Value = text.substr(token.Start, token.End - token.Start);
            if (index < text.size() && text[index] == '"') {
                ++index;
            }
        } else {
            token.Start = index;
            while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) == 0) {
                ++index;
            }
            token.End = index;
            token.Value = text.substr(token.Start, token.End - token.Start);
        }

        tokens.push_back(std::move(token));
    }

    return tokens;
}

bool EndsWithWhitespaceOutsideQuotes(const std::string& text)
{
    bool inQuotes = false;
    for (char ch : text) {
        if (ch == '"') {
            inQuotes = !inQuotes;
        }
    }

    return !inQuotes
        && !text.empty()
        && std::isspace(static_cast<unsigned char>(text.back())) != 0;
}

std::string QuoteIfNeeded(const std::string& text)
{
    if (text.find_first_of(" \t") == std::string::npos) {
        return text;
    }

    return "\"" + text + "\"";
}
}

void RuntimeConsole::Initialize(InputManager* inputManager)
{
    Input = inputManager;
    if (Input != nullptr) {
        Input->SetGameplayInputBlocked(false);
    }

    if (!bBuiltinsRegistered) {
        RegisterBuiltins();
        bBuiltinsRegistered = true;
    }
}

void RuntimeConsole::TickInput()
{
    if (Input == nullptr) {
        return;
    }

    if (Input->WasKeyJustPressed(EKeyCode::GraveAccent)) {
        bIsOpen = !bIsOpen;
        bFocusInput = bIsOpen;
        HistoryIndex = -1;

        if (bIsOpen) {
            AddMessage(EConsoleMessageType::Info, "Runtime console opened. Type 'help' for available commands.");
        }
    }

    Input->SetGameplayInputBlocked(bIsOpen);
}

void RuntimeConsole::Draw()
{
    if (!bIsOpen) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float consoleHeight = viewport->Size.y * 0.3f;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - consoleHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, consoleHeight));
    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("Runtime Console", nullptr, windowFlags)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Press ~ to toggle");
    ImGui::Separator();

    const float footerHeight = ImGui::GetFrameHeightWithSpacing();
    const ImVec2 logRegionSize(0.0f, -footerHeight * 2.0f);
    if (ImGui::BeginChild("ConsoleLog", logRegionSize, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const ConsoleMessage& entry : Messages) {
            ImGui::PushStyleColor(ImGuiCol_Text, GetConsoleMessageColor(entry.Type));
            ImGui::TextWrapped("%s", entry.Text.c_str());
            ImGui::PopStyleColor();
        }

        if (bScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            bScrollToBottom = false;
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Run")) {
        ExecuteCurrentInput();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearMessages();
        bFocusInput = true;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (bFocusInput) {
        ImGui::SetKeyboardFocusHere();
        bFocusInput = false;
    }

    const ImGuiInputTextFlags inputFlags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory |
        ImGuiInputTextFlags_CallbackCompletion;

    if (ImGui::InputText("##ConsoleInput", InputBuffer.data(), InputBuffer.size(), inputFlags, &RuntimeConsole::InputTextCallback, this)) {
        ExecuteCurrentInput();
    }

    ImGui::End();
}

void RuntimeConsole::RegisterCommand(const ConsoleCommandDefinition& def)
{
    if (def.Name.empty() || !def.Handler) {
        return;
    }

    ConsoleCommandDefinition normalizedDef = def;
    if (normalizedDef.Usage.empty()) {
        normalizedDef.Usage = normalizedDef.Name;
    }

    const std::string normalizedName = NormalizeCommandName(normalizedDef.Name);
    const auto it = CommandLookup.find(normalizedName);
    if (it != CommandLookup.end()) {
        Commands[it->second] = std::move(normalizedDef);
        return;
    }

    CommandLookup[normalizedName] = Commands.size();
    Commands.push_back(std::move(normalizedDef));
}

void RuntimeConsole::AddMessage(EConsoleMessageType type, std::string text)
{
    Messages.push_back({type, std::move(text)});
    if (Messages.size() > MAX_MESSAGE_COUNT) {
        Messages.erase(Messages.begin());
    }
    bScrollToBottom = true;
}

void RuntimeConsole::RegisterBuiltins()
{
    RegisterCommand({
        .Name = "help",
        .Usage = "help",
        .Description = "List all registered console commands.",
        .Handler = [this](const std::vector<std::string>& args) {
            if (!args.empty()) {
                AddMessage(EConsoleMessageType::Error, "Usage: help");
                return;
            }

            AddMessage(EConsoleMessageType::Info, "Available commands:");
            for (const ConsoleCommandDefinition& command : Commands) {
                std::string line = "  " + command.Usage;
                if (!command.Description.empty()) {
                    line += " - " + command.Description;
                }
                AddMessage(EConsoleMessageType::Info, line);
            }
        }
    });

    RegisterCommand({
        .Name = "clear",
        .Usage = "clear",
        .Description = "Clear the runtime console log.",
        .Handler = [this](const std::vector<std::string>& args) {
            if (!args.empty()) {
                AddMessage(EConsoleMessageType::Error, "Usage: clear");
                return;
            }

            ClearMessages();
        }
    });

    RegisterCommand({
        .Name = "history",
        .Usage = "history",
        .Description = "Print previously executed commands.",
        .Handler = [this](const std::vector<std::string>& args) {
            if (!args.empty()) {
                AddMessage(EConsoleMessageType::Error, "Usage: history");
                return;
            }

            if (History.empty()) {
                AddMessage(EConsoleMessageType::Info, "Command history is empty.");
                return;
            }

            for (size_t index = 0; index < History.size(); ++index) {
                AddMessage(EConsoleMessageType::Info, std::to_string(index) + ": " + History[index]);
            }
        }
    });
}

void RuntimeConsole::ClearMessages()
{
    Messages.clear();
    bScrollToBottom = true;
}

void RuntimeConsole::ExecuteCurrentInput()
{
    const std::string commandLine = TrimCopy(InputBuffer.data());
    std::fill(InputBuffer.begin(), InputBuffer.end(), '\0');
    HistoryIndex = -1;
    bFocusInput = true;

    if (commandLine.empty()) {
        return;
    }

    ExecuteCommand(commandLine);

    History.push_back(commandLine);
}

void RuntimeConsole::ExecuteCommand(const std::string& commandLine)
{
    AddMessage(EConsoleMessageType::Command, "> " + commandLine);

    std::vector<std::string> tokens = TokenizeCommand(commandLine);
    if (tokens.empty()) {
        return;
    }

    const std::string normalizedName = NormalizeCommandName(tokens.front());
    tokens.erase(tokens.begin());

    const auto it = CommandLookup.find(normalizedName);
    if (it == CommandLookup.end()) {
        AddMessage(EConsoleMessageType::Error, "Unknown command: " + normalizedName);
        AddMessage(EConsoleMessageType::Info, "Type 'help' to list available commands.");
        return;
    }

    try {
        Commands[it->second].Handler(tokens);
    } catch (const std::exception& ex) {
        AddMessage(EConsoleMessageType::Error, std::string("Command failed: ") + ex.what());
    } catch (...) {
        AddMessage(EConsoleMessageType::Error, "Command failed due to an unknown error.");
    }
}

std::vector<std::string> RuntimeConsole::TokenizeCommand(const std::string& commandLine) const
{
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (char ch : commandLine) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string RuntimeConsole::NormalizeCommandName(const std::string& name)
{
    std::string normalized = name;
    std::ranges::transform(normalized, normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized;
}

int RuntimeConsole::InputTextCallback(ImGuiInputTextCallbackData* data)
{
    RuntimeConsole* console = static_cast<RuntimeConsole*>(data->UserData);
    if (console == nullptr) {
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        return console->OnInputTextHistory(data);
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        return console->OnInputTextCompletion(data);
    }

    return 0;
}

int RuntimeConsole::OnInputTextCompletion(ImGuiInputTextCallbackData* data)
{
    if (data == nullptr) {
        return 0;
    }

    const size_t cursorPos = static_cast<size_t>(data->CursorPos);
    const std::string fullInput(data->Buf, static_cast<size_t>(data->BufTextLen));
    const std::string prefixInput = fullInput.substr(0, cursorPos);
    const std::vector<ConsoleTokenRange> allTokens = TokenizeWithRanges(fullInput);
    const std::vector<ConsoleTokenRange> prefixTokens = TokenizeWithRanges(prefixInput);
    const bool completingNewToken = EndsWithWhitespaceOutsideQuotes(prefixInput);

    if (prefixTokens.empty()) {
        AddMessage(EConsoleMessageType::Info, "Commands: type 'help' or press Tab after a prefix.");
        return 0;
    }

    bool completingCommand = false;
    std::vector<std::string> argsBeforeCursor;
    size_t activeArgIndex = 0;
    std::string currentPrefix;
    size_t replaceStart = cursorPos;
    size_t replaceEnd = cursorPos;
    bool activeTokenQuoted = false;
    std::vector<std::string> matches;

    if (prefixTokens.size() == 1 && !completingNewToken) {
        completingCommand = true;
        currentPrefix = prefixTokens.front().Value;
        replaceStart = allTokens.front().Start;
        replaceEnd = allTokens.front().End;

        for (const ConsoleCommandDefinition& command : Commands) {
            if (StartsWithInsensitive(command.Name, currentPrefix)) {
                matches.push_back(command.Name);
            }
        }
    } else {
        const std::string normalizedCommand = NormalizeCommandName(prefixTokens.front().Value);
        const auto it = CommandLookup.find(normalizedCommand);
        if (it == CommandLookup.end()) {
            return 0;
        }

        const ConsoleCommandDefinition& command = Commands[it->second];
        if (!command.AutocompleteHandler) {
            return 0;
        }

        for (size_t tokenIndex = 1; tokenIndex < prefixTokens.size(); ++tokenIndex) {
            argsBeforeCursor.push_back(prefixTokens[tokenIndex].Value);
        }

        if (completingNewToken) {
            activeArgIndex = argsBeforeCursor.size();
            currentPrefix.clear();
            replaceStart = cursorPos;
            replaceEnd = cursorPos;
        } else {
            if (argsBeforeCursor.empty()) {
                return 0;
            }

            activeArgIndex = argsBeforeCursor.size() - 1;
            currentPrefix = argsBeforeCursor.back();
            replaceStart = allTokens[prefixTokens.size() - 1].Start;
            replaceEnd = allTokens[prefixTokens.size() - 1].End;
            activeTokenQuoted = allTokens[prefixTokens.size() - 1].Quoted;
        }

        matches = command.AutocompleteHandler(argsBeforeCursor, activeArgIndex);
        std::erase_if(matches, [this, &currentPrefix](const std::string& value) {
            return value.empty() || !StartsWithInsensitive(value, currentPrefix);
        });
    }

    if (matches.empty()) {
        return 0;
    }

    std::ranges::sort(matches, [this](const std::string& lhs, const std::string& rhs) {
        return NormalizeCommandName(lhs) < NormalizeCommandName(rhs);
    });
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

    const std::string commonPrefix = BuildLongestCommonPrefix(matches);
    const bool hasSingleMatch = matches.size() == 1;
    std::string replacement = hasSingleMatch ? matches.front() : commonPrefix;

    if (replacement.empty()) {
        if (matches.size() > 1) {
            std::string line = completingCommand ? "Command matches:" : "Argument matches:";
            for (const std::string& name : matches) {
                line += " ";
                line += name;
            }
            AddMessage(EConsoleMessageType::Info, line);
        }
        return 0;
    }

    if (!completingCommand && !activeTokenQuoted) {
        replacement = QuoteIfNeeded(replacement);
    }

    const std::string existingToken = fullInput.substr(replaceStart, replaceEnd - replaceStart);
    if (hasSingleMatch && replaceEnd == cursorPos) {
        replacement += ' ';
    }

    if (replacement != existingToken) {
        data->DeleteChars(static_cast<int>(replaceStart), static_cast<int>(replaceEnd - replaceStart));
        data->InsertChars(static_cast<int>(replaceStart), replacement.c_str());
    }

    if (matches.size() > 1 && commonPrefix.size() <= currentPrefix.size()) {
        std::string line = completingCommand ? "Command matches:" : "Argument matches:";
        for (const std::string& name : matches) {
            line += " ";
            line += name;
        }
        AddMessage(EConsoleMessageType::Info, line);
    }

    return 0;
}

int RuntimeConsole::OnInputTextHistory(ImGuiInputTextCallbackData* data)
{
    if (History.empty()) {
        return 0;
    }

    if (data->EventKey == ImGuiKey_UpArrow) {
        if (HistoryIndex == -1) {
            HistoryIndex = static_cast<int>(History.size()) - 1;
        } else if (HistoryIndex > 0) {
            --HistoryIndex;
        }
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        if (HistoryIndex != -1) {
            ++HistoryIndex;
            if (HistoryIndex >= static_cast<int>(History.size())) {
                HistoryIndex = -1;
            }
        }
    }

    const std::string historyEntry = HistoryIndex >= 0 ? History[HistoryIndex] : std::string();
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, historyEntry.c_str());
    return 0;
}
