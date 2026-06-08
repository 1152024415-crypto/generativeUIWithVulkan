#ifndef GAME_DSL_RENDER_H
#define GAME_DSL_RENDER_H

#include "dsl/CustomV2DslRender.h"
#include <string>
#include <vector>

namespace application::dsl {

class GameDslRender : public CustomV2DslRender {
public:
    std::string GetFormatName() const override { return "GameDsl"; }
    bool CanParse(const nlohmann::json& root) const override;
    std::vector<DslRenderCommand> Parse(
        const nlohmann::json& root,
        const ParseContext& ctx = {}
    ) const override;

    struct GameConfig {
        nlohmann::json targetConfig;
        nlohmann::json crosshairConfig;
        float duration = 60.0f;
    };
    static GameConfig ExtractGameConfig(const nlohmann::json& root);
};

} // namespace application::dsl
#endif // GAME_DSL_RENDER_H
