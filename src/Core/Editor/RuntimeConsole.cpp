#include "RuntimeConsole.h"

#include "Core/GameSystem/InputManager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <exception>

namespace {
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
        ImGuiInputTextFlags_CallbackHistory;

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
