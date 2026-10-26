#include "editor/recent_documents.h"

#include "path_info.h"

namespace editor {

auto recent_documents_file() -> std::string {
    return PATH_INFO::config_dir() + "editor_recent.json";
}

auto recent_documents() -> std::vector<recent_document> { return {}; }

auto remember_document(const recent_document& /*opened*/) -> void {}

auto loaded_mods() -> std::vector<mod_id> { return {}; }

} // namespace editor
