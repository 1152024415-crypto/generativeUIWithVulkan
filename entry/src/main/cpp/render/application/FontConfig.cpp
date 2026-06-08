#include "FontConfig.h"
#include "agenui_engine/thirdparty/json.hpp"
#include "logger_common.h"

namespace application {

bool FontConfig::parseFromJson(const std::string& jsonStr) {
    m_valid = false;
    m_fonts.clear();
    m_aliases.clear();
    m_loadOrder.clear();
    m_defaultFont.clear();

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(jsonStr, nullptr, false);
        if (!root.is_object()) {
            LOGE("FontConfig: JSON root is not an object");
            return false;
        }
    } catch (const nlohmann::json::exception& e) {
        LOGE("FontConfig: JSON parse error: %s", e.what());
        return false;
    }

    // version (optional, for future compatibility)
    if (root.contains("version")) {
        int version = root["version"].get<int>();
        LOGI("FontConfig: version %d", version);
    }

    // defaultFont
    if (!root.contains("defaultFont") || !root["defaultFont"].is_string()) {
        LOGE("FontConfig: missing or invalid 'defaultFont'");
        return false;
    }
    m_defaultFont = root["defaultFont"].get<std::string>();

    // fonts array
    if (!root.contains("fonts") || !root["fonts"].is_array()) {
        LOGE("FontConfig: missing or invalid 'fonts' array");
        return false;
    }
    for (const auto& item : root["fonts"]) {
        FontEntryConfig entry;
        if (!item.contains("id") || !item["id"].is_string()) continue;
        entry.id = item["id"].get<std::string>();

        if (!item.contains("path") || !item["path"].is_string()) continue;
        entry.path = item["path"].get<std::string>();

        entry.faceIndex = item.value("faceIndex", -1);

        if (item.contains("roles") && item["roles"].is_array()) {
            for (const auto& r : item["roles"]) {
                if (r.is_string()) entry.roles.push_back(r.get<std::string>());
            }
        }

        if (item.contains("fallbackChain") && item["fallbackChain"].is_array()) {
            for (const auto& fb : item["fallbackChain"]) {
                FontEntryConfig::FallbackStep step;
                step.source = fb.value("source", "");
                step.faceIndex = fb.value("faceIndex", 0);
                if (!step.source.empty()) {
                    entry.fallbackChain.push_back(step);
                }
            }
        }

        m_fonts.push_back(std::move(entry));
    }

    // aliases
    if (root.contains("aliases") && root["aliases"].is_object()) {
        for (auto it = root["aliases"].begin(); it != root["aliases"].end(); ++it) {
            if (it.value().is_string()) {
                m_aliases[it.key()] = it.value().get<std::string>();
            }
        }
    }

    // loadOrder
    if (root.contains("loadOrder") && root["loadOrder"].is_array()) {
        for (const auto& id : root["loadOrder"]) {
            if (id.is_string()) {
                m_loadOrder.push_back(id.get<std::string>());
            }
        }
    } else {
        // Default: load order follows fonts array order
        for (const auto& f : m_fonts) {
            m_loadOrder.push_back(f.id);
        }
    }

    m_valid = true;
    LOGI("FontConfig: parsed %zu fonts, %zu aliases, default='%s'",
         m_fonts.size(), m_aliases.size(), m_defaultFont.c_str());
    return true;
}

const std::string& FontConfig::getDefaultFont() const {
    return m_defaultFont;
}

const std::vector<FontEntryConfig>& FontConfig::getFonts() const {
    return m_fonts;
}

const std::unordered_map<std::string, std::string>& FontConfig::getAliases() const {
    return m_aliases;
}

const std::vector<std::string>& FontConfig::getLoadOrder() const {
    return m_loadOrder;
}

const FontEntryConfig* FontConfig::findByRole(const std::string& role) const {
    for (const auto& f : m_fonts) {
        for (const auto& r : f.roles) {
            if (r == role) return &f;
        }
    }
    return nullptr;
}

const FontEntryConfig* FontConfig::findById(const std::string& id) const {
    for (const auto& f : m_fonts) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

} // namespace application
