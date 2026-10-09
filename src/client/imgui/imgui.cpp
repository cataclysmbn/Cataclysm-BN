#if defined(CATA_IMGUI)

#    include "cata_utility.h"
#    include "client_backend.h"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "game_observation.h"
#    include "json.h"
#    include "options.h"
#    include "output.h"
#    include "path_info.h"
#    include "runtime_handlers.h"
#    include "translations.h"

#    if defined(DebugLog)
#        undef DebugLog
#    endif

#    include <SDL3/SDL.h>
#    include <algorithm>
#    include <array>
#    include <chrono>
#    include <cstdint>
#    include <imgui.h>
#    include <imgui_impl_sdl3.h>
#    include <imgui_impl_sdlrenderer3.h>
#    include <optional>
#    include <sstream>
#    include <stdexcept>
#    include <string>
#    include <string_view>
#    include <utility>
#    include <vector>

namespace game_client {
namespace {

struct queued_input {
    std::uint64_t input_id;
    input_event event;
};

struct imgui_state {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    ImGuiContext* context = nullptr;
    std::optional<std::uint64_t> waiting_input_id;
    std::optional<queued_input> pending_input;
    std::optional<std::uint64_t> observation_input_id;
    game_observation::snapshot observation;
    std::array<char, 512> text{};
    std::string input_error;
    std::string interaction_schema;
    interaction_page_state interaction_page;
    ImVector<ImWchar> glyph_ranges;
    bool platform_backend_initialized = false;
    bool renderer_backend_initialized = false;
    bool memory_initialized = false;
    bool initialized = false;
};

auto state() -> imgui_state& {
    static auto result = imgui_state{};
    return result;
}

auto release_resources(imgui_state& client) -> void {
    if (client.memory_initialized) { memory::shutdown(); }
    if (client.context != nullptr) { ImGui::SetCurrentContext(client.context); }
    if (client.renderer_backend_initialized) { ImGui_ImplSDLRenderer3_Shutdown(); }
    if (client.platform_backend_initialized) { ImGui_ImplSDL3_Shutdown(); }
    if (client.context != nullptr) { ImGui::DestroyContext(client.context); }
    if (client.renderer != nullptr) { SDL_DestroyRenderer(client.renderer); }
    if (client.window != nullptr) { SDL_DestroyWindow(client.window); }
    client = imgui_state{};
}

auto accepting_input() -> bool {
    return state().waiting_input_id && *state().waiting_input_id == current_input_id();
}

auto queue_command(input_command command) -> void {
    auto& client = state();
    if (!accepting_input() || client.pending_input) { return; }
    command.input_id = *client.waiting_input_id;
    auto resolved = resolve_input_command(command, memory::screen_size());
    if (resolved) {
        client.pending_input =
            queued_input{.input_id = *client.waiting_input_id, .event = std::move(*resolved)};
        client.input_error.clear();
    } else {
        client.input_error = resolved.error();
    }
}

auto key_command(std::string key, std::vector<std::string> modifiers = {}) -> input_command {
    auto result = input_command{};
    result.key = std::move(key);
    result.modifiers = std::move(modifiers);
    return result;
}

auto text_command(std::string text) -> input_command {
    auto result = input_command{};
    result.text = std::move(text);
    return result;
}

auto action_command(std::string action) -> input_command {
    auto result = input_command{};
    result.action = std::move(action);
    return result;
}

struct interaction_request {
    interaction_operation operation;
    std::string target_id;
    std::string value;
    std::optional<bool> submit;
    std::optional<std::uint64_t> count;
    std::optional<interaction_position> position;
};

auto interaction_command_for(const interaction_snapshot& snapshot, interaction_request request)
    -> input_command {
    auto result = input_command{};
    result.interaction = interaction_command{
        .input_id = snapshot.input_id,
        .operation = request.operation,
        .target_id = std::move(request.target_id),
        .value = std::move(request.value),
        .submit = request.submit,
        .count = request.count,
        .position = request.position,
    };
    return result;
}

auto special_key_command(const SDL_KeyboardEvent& event) -> std::optional<input_command> {
    auto name = std::string{};
    switch (event.key) {
        case SDLK_ESCAPE:
            name = "ESC";
            break;
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER:
            name = "RETURN";
            break;
        case SDLK_TAB:
            name = event.mod & SDL_KMOD_SHIFT ? "BACKTAB" : "TAB";
            break;
        case SDLK_BACKSPACE:
        case SDLK_KP_BACKSPACE:
            name = "BACKSPACE";
            break;
        case SDLK_DELETE:
            name = "DELETE";
            break;
        case SDLK_UP:
            name = "UP";
            break;
        case SDLK_DOWN:
            name = "DOWN";
            break;
        case SDLK_LEFT:
            name = "LEFT";
            break;
        case SDLK_RIGHT:
            name = "RIGHT";
            break;
        case SDLK_PAGEUP:
            name = "PPAGE";
            break;
        case SDLK_PAGEDOWN:
            name = "NPAGE";
            break;
        case SDLK_HOME:
            name = "HOME";
            break;
        case SDLK_END:
            name = "END";
            break;
        case SDLK_F1:
        case SDLK_F2:
        case SDLK_F3:
        case SDLK_F4:
        case SDLK_F5:
        case SDLK_F6:
        case SDLK_F7:
        case SDLK_F8:
        case SDLK_F9:
        case SDLK_F10:
        case SDLK_F11:
        case SDLK_F12:
        case SDLK_F13:
        case SDLK_F14:
        case SDLK_F15:
            name = "F" + std::to_string(event.key - SDLK_F1 + 1);
            break;
        default:
            break;
    }
    if (!name.empty()) { return key_command(std::move(name)); }

    if (event.mod & SDL_KMOD_CTRL && event.key >= SDLK_A && event.key <= SDLK_Z) {
        const auto letter = static_cast<char>('A' + event.key - SDLK_A);
        return key_command(std::string(1, letter), {"CTRL"});
    }
    return std::nullopt;
}

auto process_events() -> void {
    auto event = SDL_Event{};
    while (SDL_PollEvent(&event)) {
        const auto imgui_wants_text = ImGui::GetIO().WantTextInput;
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT
            || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
                && event.window.windowID == SDL_GetWindowID(state().window))) {
            exit_handler(0);
        }
        if (!accepting_input() || state().pending_input || imgui_wants_text) { continue; }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (const auto command = special_key_command(event.key)) { queue_command(*command); }
        } else if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text[0] != '\0') {
            queue_command(text_command(event.text.text));
        }
    }
}

auto current_observation() -> const game_observation::snapshot& {
    auto& client = state();
    if (!client.observation_input_id || *client.observation_input_id != current_input_id()) {
        client.observation = game_observation::capture();
        client.observation_input_id = current_input_id();
    }
    return client.observation;
}

auto imgui_label(const char* visible, const std::string_view id) -> std::string {
    return std::string(visible) + "###" + std::string(id);
}

auto render_screen(const screen_snapshot& screen) -> void {
    const auto heading = imgui_label(_("Composed screen"), "composed_screen_heading");
    ImGui::SeparatorText(heading.c_str());
    if (ImGui::BeginChild("###composed_screen", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::TextUnformatted(screen.text.c_str());
    }
    ImGui::EndChild();
}

auto render_item_array(JsonArray items, const char* empty_text) -> void {
    if (items.empty()) {
        ImGui::TextDisabled("%s", empty_text);
        return;
    }
    auto index = 0;
    for (const auto value : items) {
        auto item = value.get_object();
        item.allow_omitted_members();
        const auto name = item.get_string("name", item.get_string("type_id", "?"));
        const auto count = item.get_int("count", 1);
        const auto invlet = item.get_string("invlet", "");
        const auto label =
            invlet.empty()
                ? (count > 1 ? name + " ×" + std::to_string(count) : name)
                : invlet + " - " + name + (count > 1 ? " ×" + std::to_string(count) : "");
        ImGui::PushID(index++);
        auto contents = item.get_array("contents");
        if (contents.empty()) {
            ImGui::BulletText("%s", label.c_str());
        } else {
            if (ImGui::TreeNode("###item", "%s", label.c_str())) {
                render_item_array(contents, _("Empty"));
                ImGui::TreePop();
            }
        }
        ImGui::PopID();
    }
}

auto render_state_and_inventory() -> void {
    const auto& observation = current_observation();
    try {
        auto input = std::istringstream{observation.json};
        auto json = JsonIn{input};
        auto root = json.get_object();
        root.allow_omitted_members();
        auto session = root.get_object("session");
        session.allow_omitted_members();
        ImGui::Text("%s: %s", _("Time"), session.get_string("time", "-").c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s %d)", _("turn"), session.get_int("turn", 0));
        if (!root.get_bool("game_ready", false)) {
            ImGui::TextWrapped("%s", _("Player state is available after entering a game."));
            return;
        }

        auto avatar = root.get_object("avatar");
        avatar.allow_omitted_members();
        auto position = avatar.get_object("absolute_position");
        position.allow_omitted_members();
        auto hp = avatar.get_object("hp");
        hp.allow_omitted_members();
        auto needs = avatar.get_object("needs");
        needs.allow_omitted_members();

        const auto player_heading = imgui_label(_("Player"), "player_state_heading");
        ImGui::SeparatorText(player_heading.c_str());
        ImGui::Text("%s: %s", _("Name"), avatar.get_string("name", "-").c_str());
        ImGui::Text("%s: %d / %d (%d%%)", _("Health"), hp.get_int("current", 0),
                    hp.get_int("max", 0), hp.get_int("percentage", 0));
        ImGui::Text("%s: %d, %d, %d", _("Position"), position.get_int("x", 0),
                    position.get_int("y", 0), position.get_int("z", 0));
        ImGui::Text("%s: %d / %d", _("Stamina"), needs.get_int("stamina", 0),
                    needs.get_int("stamina_max", 0));
        ImGui::Text(
            "%s: %d    %s: %d    %s: %d", _("Stored calories"), needs.get_int("stored_kcal", 0),
            _("Thirst"), needs.get_int("thirst", 0), _("Fatigue"), needs.get_int("fatigue", 0));

        if (ImGui::BeginTabBar("###inventory_tabs")) {
            const auto inventory_tab = imgui_label(_("Inventory"), "inventory_tab");
            if (ImGui::BeginTabItem(inventory_tab.c_str())) {
                render_item_array(avatar.get_array("inventory"), _("Inventory is empty."));
                ImGui::EndTabItem();
            }
            const auto worn_tab = imgui_label(_("Worn"), "worn_tab");
            if (ImGui::BeginTabItem(worn_tab.c_str())) {
                render_item_array(avatar.get_array("worn"), _("Nothing worn."));
                ImGui::EndTabItem();
            }
            const auto wielded_tab = imgui_label(_("Wielded"), "wielded_tab");
            if (ImGui::BeginTabItem(wielded_tab.c_str())) {
                render_item_array(avatar.get_array("wielded"), _("Nothing wielded."));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    } catch (const std::exception& error) {
        ImGui::TextWrapped("%s: %s", _("State unavailable"), error.what());
    }
}

auto storage_kind_label(const std::string& storage_kind) -> std::string {
    if (storage_kind == "inventory") { return _("inventory"); }
    if (storage_kind == "worn") { return _("worn"); }
    if (storage_kind == "container") { return _("container"); }
    if (storage_kind == "ground") { return _("ground"); }
    if (storage_kind == "cargo") { return _("cargo"); }
    if (storage_kind == "mixed") { return _("mixed"); }
    return storage_kind;
}

auto render_interaction(const bool disabled) -> void {
    auto& client = state();
    const auto interaction = paginated_interaction(client.interaction_page);
    const auto heading = imgui_label(_("Interaction"), "interaction_heading");
    ImGui::SeparatorText(heading.c_str());
    if (!interaction.structured) {
        ImGui::TextDisabled("%s", _("This view exposes generic actions only."));
        return;
    }
    if (!interaction.title.empty()) { ImGui::TextWrapped("%s", interaction.title.c_str()); }
    if (!interaction.message.empty()) { ImGui::TextWrapped("%s", interaction.message.c_str()); }
    for (const auto& pane : interaction.panes) {
        const auto role = [&]() -> std::string {
            if (pane.role == "source") { return _("Source"); }
            if (pane.role == "destination") { return _("Destination"); }
            if (pane.role == "npc") { return _("NPC"); }
            if (pane.role == "player") { return _("Player"); }
            return pane.role;
        }();
        ImGui::
            TextWrapped("%s — %s: %s", pane.label.c_str(), role.c_str(), pane.area_label.c_str());
        if (!pane.area_description.empty()) {
            ImGui::TextDisabled("%s", pane.area_description.c_str());
        }
        const auto storage = storage_kind_label(pane.storage_kind);
        ImGui::TextDisabled(
            "%s: %s  %s: %s", _("Storage"), storage.c_str(), _("Filter"),
            pane.filter.empty() ? _("none") : pane.filter.c_str());
    }
    if (interaction.choice_total > 0) {
        ImGui::Text("%s: %zu  %s: %zu", _("Offset"), interaction.choice_offset, _("Total"),
                    interaction.choice_total);
        ImGui::BeginDisabled(disabled || interaction.choice_offset == 0);
        if (ImGui::SmallButton(_("Previous"))) {
            client.interaction_page.offset =
                interaction.choice_offset > client.interaction_page.limit
                    ? interaction.choice_offset - client.interaction_page.limit
                    : 0;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        const auto page_end = interaction.choice_offset + interaction.choices.size();
        ImGui::BeginDisabled(disabled || page_end >= interaction.choice_total);
        if (ImGui::SmallButton(_("Next"))) {
            client.interaction_page.offset =
                interaction.choice_offset + client.interaction_page.limit;
        }
        ImGui::EndDisabled();
    }
    ImGui::BeginDisabled(disabled);
    for (const auto& choice : interaction.choices) {
        ImGui::PushID(choice.id.c_str());
        ImGui::BeginDisabled(!choice.selectable);
        if (interaction.kind == interaction_kind::inventory) {
            ImGui::TextUnformatted(choice.selected ? "[x]" : "[ ]");
            ImGui::SameLine();
        }
        if (choice.pane_id) {
            const auto pane =
                std::ranges::find(interaction.panes, *choice.pane_id, &interaction_pane::id);
            if (pane != interaction.panes.end()) {
                ImGui::Text("[%s]", pane->label.c_str());
                ImGui::SameLine();
            }
        }
        if (!choice.storage_kind.empty()) {
            const auto storage = storage_kind_label(choice.storage_kind);
            const auto area = std::ranges::find(choice.columns, "Area", &interaction_column::label);
            if (area == choice.columns.end()) {
                ImGui::Text("[%s]", storage.c_str());
            } else {
                ImGui::Text("[%s — %s]", storage.c_str(), area->value.c_str());
            }
            ImGui::SameLine();
        }
        if (ImGui::Selectable(choice.label.c_str(), choice.highlighted)) {
            queue_command(interaction_command_for(
                interaction, {.operation = interaction_operation::choose, .target_id = choice.id}));
        }
        if (!choice.description.empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", choice.description.c_str());
        }
        if (interaction.allow_set_count && choice.selected_count && choice.available_count) {
            ImGui::SameLine();
            ImGui::Text("%llu/%llu", static_cast<unsigned long long>(*choice.selected_count),
                        static_cast<unsigned long long>(*choice.available_count));
            const auto minimum_count = choice.minimum_count.value_or(0);
            if (*choice.selected_count > minimum_count) {
                ImGui::SameLine();
                if (ImGui::SmallButton(minimum_count == 0 ? _("Clear") : "-")) {
                    queue_command(interaction_command_for(
                        interaction,
                        {.operation = interaction_operation::set_count,
                         .target_id = choice.id,
                         .count = minimum_count == 0 ? 0 : *choice.selected_count - 1}));
                }
            }
            if (*choice.selected_count < *choice.available_count) {
                ImGui::SameLine();
                if (ImGui::SmallButton("+")) {
                    queue_command(interaction_command_for(
                        interaction,
                        {.operation = interaction_operation::set_count,
                         .target_id = choice.id,
                         .count = *choice.selected_count + 1}));
                }
            }
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if (interaction.target) {
        ImGui::Text(
            "%s: (%d, %d, %d)  %s: %d", _("Cursor"), interaction.target->cursor.x,
            interaction.target->cursor.y, interaction.target->cursor.z, _("Range"),
            interaction.target->range);
        for (const auto& candidate : interaction.target->candidates) {
            ImGui::PushID(candidate.id.c_str());
            if (ImGui::Selectable(
                    candidate.label.c_str(), candidate.position == interaction.target->cursor)) {
                queue_command(interaction_command_for(
                    interaction,
                    {.operation = interaction_operation::set_target,
                     .target_id = candidate.id,
                     .position = candidate.position}));
            }
            if (!candidate.description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", candidate.description.c_str());
            }
            ImGui::PopID();
        }
    }
    if (interaction.field) {
        if (client.interaction_schema != interaction.schema_id) {
            client.text.fill('\0');
            const auto count = std::min(client.text.size() - 1, interaction.field->value.size());
            std::ranges::copy_n(interaction.field->value.begin(), count, client.text.begin());
            client.interaction_schema = interaction.schema_id;
        }
        ImGui::InputText(interaction.field->label.c_str(), client.text.data(), client.text.size());
        if (ImGui::Button(_("Fill"))) {
            queue_command(interaction_command_for(
                interaction,
                {.operation = interaction_operation::fill,
                 .target_id = interaction.field->id,
                 .value = client.text.data(),
                 .submit = false}));
        }
        ImGui::SameLine();
        if (ImGui::Button(_("Submit"))) {
            queue_command(interaction_command_for(
                interaction,
                {.operation = interaction_operation::fill,
                 .target_id = interaction.field->id,
                 .value = client.text.data(),
                 .submit = true}));
        }
    }
    if (interaction.allow_cancel && ImGui::Button(_("Cancel"))) {
        queue_command(
            interaction_command_for(interaction, {.operation = interaction_operation::cancel}));
    }
    ImGui::EndDisabled();
}

auto render_controls() -> void {
    const auto active = active_input_context();
    ImGui::Text("%s: %s #%llu", _("Input"), std::string(active.category).c_str(),
                static_cast<unsigned long long>(current_input_id()));
    const auto disabled = !accepting_input() || state().pending_input.has_value();
    ImGui::BeginDisabled(disabled);
    const auto text_label = imgui_label(_("Text"), "command_text");
    const auto submit_text = ImGui::InputText(
        text_label.c_str(), state().text.data(), state().text.size(),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const auto send_label = imgui_label(_("Send"), "send_command");
    if (submit_text || ImGui::Button(send_label.c_str())) {
        if (state().text.front() != '\0') {
            queue_command(text_command(state().text.data()));
            state().text.fill('\0');
        }
    }
    ImGui::EndDisabled();
    if (!accepting_input()) { ImGui::TextDisabled("%s", _("Waiting for the next input request.")); }
    if (!state().input_error.empty()) {
        ImGui::TextColored(ImVec4(1.0F, 0.35F, 0.35F, 1.0F), "%s", state().input_error.c_str());
    }

    render_interaction(disabled);

    const auto actions_heading = imgui_label(_("Actions"), "actions_heading");
    ImGui::SeparatorText(actions_heading.c_str());
    if (ImGui::BeginChild("###actions", ImVec2(0.0F, 260.0F), ImGuiChildFlags_Borders)) {
        ImGui::BeginDisabled(disabled);
        for (const auto& action : available_input_actions()) {
            ImGui::PushID(action.id.c_str());
            const auto label = action.name.empty() ? action.id : action.name;
            if (ImGui::Selectable(label.c_str())) { queue_command(action_command(action.id)); }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();

    const auto state_heading = imgui_label(_("State and inventory"), "state_inventory_heading");
    ImGui::SeparatorText(state_heading.c_str());
    if (ImGui::BeginChild("###state_inventory", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
        render_state_and_inventory();
    }
    ImGui::EndChild();
}

auto render_frame() -> void {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(display);
    constexpr auto flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings;
    const auto window_label =
        imgui_label(_("Cataclysm: Bright Nights ImGui Client"), "main_window");
    ImGui::Begin(window_label.c_str(), nullptr, flags);
    if (ImGui::BeginTable(
            "###client_layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        const auto game_column = imgui_label(_("Game"), "game_column");
        const auto controls_column = imgui_label(_("Controls"), "controls_column");
        ImGui::TableSetupColumn(game_column.c_str(), ImGuiTableColumnFlags_WidthStretch, 0.68F);
        ImGui::TableSetupColumn(controls_column.c_str(), ImGuiTableColumnFlags_WidthStretch, 0.32F);
        ImGui::TableNextColumn();
        render_screen(memory::snapshot());
        ImGui::TableNextColumn();
        render_controls();
        ImGui::EndTable();
    }
    ImGui::End();

    ImGui::Render();
    SDL_SetRenderDrawColor(state().renderer, 18, 20, 24, 255);
    SDL_RenderClear(state().renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), state().renderer);
    SDL_RenderPresent(state().renderer);
    if (!ImGui::GetIO().WantTextInput && !SDL_TextInputActive(state().window)) {
        SDL_StartTextInput(state().window);
    }
}

class input_wait_scope {
public:
    input_wait_scope() {
        auto& client = state();
        client.pending_input.reset();
        client.waiting_input_id = current_input_id();
    }
    ~input_wait_scope() {
        auto& client = state();
        client.pending_input.reset();
        client.waiting_input_id.reset();
    }
    input_wait_scope(const input_wait_scope&) = delete;
    auto operator=(const input_wait_scope&) -> input_wait_scope& = delete; // *NOPAD*
};

class imgui_backend final: public backend {
public:
    auto initialize() -> void override {
        auto& client = state();
        if (client.initialized) { return; }
        auto rollback = on_out_of_scope([&client]() { release_resources(client); });
        client.window =
            SDL_CreateWindow("Cataclysm: Bright Nights - ImGui", 1440, 900, SDL_WINDOW_RESIZABLE);
        if (client.window == nullptr) {
            throw std::runtime_error(
                std::string("Could not create ImGui window: ") + SDL_GetError());
        }
        client.renderer = SDL_CreateRenderer(client.window, nullptr);
        if (client.renderer == nullptr) {
            throw std::runtime_error(
                std::string("Could not create ImGui renderer: ") + SDL_GetError());
        }
        SDL_SetRenderVSync(client.renderer, 1);
        SDL_StartTextInput(client.window);
        IMGUI_CHECKVERSION();
        client.context = ImGui::CreateContext();
        if (client.context == nullptr) {
            throw std::runtime_error("Could not create the Dear ImGui context");
        }
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        auto glyph_builder = ImFontGlyphRangesBuilder{};
        glyph_builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
        glyph_builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
        glyph_builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        constexpr auto interface_ranges = std::array<
            ImWchar, 9>{0x2000, 0x206F, 0x2190, 0x21FF, 0x2500, 0x259F, 0x2600, 0x26FF, 0};
        glyph_builder.AddRanges(interface_ranges.data());
        glyph_builder.BuildRanges(&client.glyph_ranges);
        const auto font = PATH_INFO::fontdir() + "unifont.ttf";
        if (io.Fonts->AddFontFromFileTTF(font.c_str(), 16.0F, nullptr, client.glyph_ranges.Data)
            == nullptr) {
            throw std::runtime_error("Could not load the ImGui Unicode font");
        }
        ImGui::StyleColorsDark();
        const auto platform_result =
            ImGui_ImplSDL3_InitForSDLRenderer(client.window, client.renderer);
        client.platform_backend_initialized = ImGui::GetIO().BackendPlatformUserData != nullptr;
        if (!platform_result) {
            throw std::runtime_error("Could not initialize the Dear ImGui SDL3 platform backend");
        }
        const auto renderer_result = ImGui_ImplSDLRenderer3_Init(client.renderer);
        client.renderer_backend_initialized = ImGui::GetIO().BackendRendererUserData != nullptr;
        if (!renderer_result) {
            throw std::runtime_error("Could not initialize the Dear ImGui SDL3 renderer backend");
        }
        const auto width = std::max(FULL_SCREEN_WIDTH, get_option<int>("TERMINAL_X"));
        const auto height = std::max(FULL_SCREEN_HEIGHT, get_option<int>("TERMINAL_Y"));
        client.memory_initialized = true;
        memory::initialize(width, height);
        render_frame();
        client.initialized = true;
        rollback.cancel();
    }

    auto shutdown() -> void override { release_resources(state()); }

    auto native_window_handle() const -> void* override { return state().window; }
    auto present() -> void override {
        process_events();
        render_frame();
    }
    auto draw_window(const catacurses::window& window) -> void override {
        memory::draw_window(window);
    }
    auto clear_window(const catacurses::window& window) -> void override {
        memory::clear_window(window);
    }
    auto set_cursor(const int visibility) -> void override { memory::set_cursor(visibility); }
    auto set_timeout(const int /*timeout_ms*/) -> void override {}
    auto read_input(const int timeout_ms) -> input_event override {
        const auto wait = input_wait_scope{};
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(0, timeout_ms));
        do {
            process_events();
            render_frame();
            if (state().pending_input) {
                if (state().pending_input->input_id != current_input_id()) {
                    state().pending_input.reset();
                    state().input_error = "Input boundary changed; observe again before acting";
                } else {
                    auto result = std::move(state().pending_input->event);
                    state().pending_input.reset();
                    return result;
                }
            }
            if (timeout_ms == 0) { return {}; }
            if (timeout_ms > 0 && std::chrono::steady_clock::now() >= deadline) {
                auto result = input_event{};
                result.type = input_event_t::timeout;
                return result;
            }
            SDL_Delay(10);
        } while (true);
    }
    auto pump_events() -> void override { process_events(); }
    auto resize(const point cell_size) -> void override {
        memory::resize(cell_size.x, cell_size.y);
    }
    auto projected_size() const -> point override { return memory::screen_size(); }
    auto capabilities() const -> client_capabilities override {
        return {.tiles = false, .mouse = false, .gamepad = false, .requires_display = true};
    }
};

} // namespace

auto make_imgui_backend() -> backend_ptr { return std::make_unique<imgui_backend>(); }

} // namespace game_client

#endif // CATA_IMGUI
