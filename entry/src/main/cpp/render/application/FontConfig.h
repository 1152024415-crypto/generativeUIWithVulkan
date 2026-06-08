#ifndef APPLICATION_FONT_CONFIG_H
#define APPLICATION_FONT_CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>

namespace application {

struct FontEntryConfig {
    std::string id;
    std::string path;                   // "fonts/HarmonyOS_Sans_SC.ttf"
    int faceIndex = -1;                 // -1=auto, 0+=specified TTC face
    std::vector<std::string> roles;     // "primary", "chinese_fallback", "bold"

    struct FallbackStep {
        std::string source;             // font id
        int faceIndex = 0;
    };
    std::vector<FallbackStep> fallbackChain;
};

class FontConfig {
public:
    bool parseFromJson(const std::string& jsonStr);
    bool isValid() const { return m_valid; }
    const std::string& getDefaultFont() const;
    const std::vector<FontEntryConfig>& getFonts() const;
    const std::unordered_map<std::string, std::string>& getAliases() const;
    const std::vector<std::string>& getLoadOrder() const;
    const FontEntryConfig* findByRole(const std::string& role) const;
    const FontEntryConfig* findById(const std::string& id) const;

private:
    bool m_valid = false;
    std::string m_defaultFont;
    std::vector<FontEntryConfig> m_fonts;
    std::unordered_map<std::string, std::string> m_aliases;
    std::vector<std::string> m_loadOrder;
};

} // namespace application

#endif // APPLICATION_FONT_CONFIG_H
