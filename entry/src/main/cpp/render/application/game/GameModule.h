#ifndef GAME_MODULE_H
#define GAME_MODULE_H

#include "GameTypes.h"
#include "../dsl/DslRenderCommand.h"
#include <string>
#include <memory>
#include <mutex>

namespace AgenUIEngine::Core { class RenderContext; }

namespace application::game {

class GameModule {
public:
    GameModule() = default;

    void InitFromStyle(const std::string& styleJson, float designW, float designH);
    void Update(float dt);
    int HandleTouch(float designX, float designY);
    void UpdateCrosshair(float designX, float designY);

    std::vector<dsl::DslRenderCommand> GenerateRenderCommands();
    void RenderFrame(AgenUIEngine::Core::RenderContext* ctx);

    bool IsGameOver() const { return m_phase == GamePhase::Over; }
    int GetScore() const { return m_score; }
    int GetCombo() const { return m_combo; }
    float GetTimeLeft() const { return m_timeLeft; }
    bool IsActive() const { return m_phase == GamePhase::Playing; }

private:
    void spawnTarget();
    void spawnParticles(float cx, float cy, int level, const glm::vec4& color);
    void spawnFragments(float cx, float cy);
    void spawnHitEffects(float cx, float cy, float radius, int level, const glm::vec4& color);

    mutable std::mutex m_mutex;
    GamePhase m_phase = GamePhase::Playing;
    float m_designW = 0, m_designH = 0;

    // Config
    GameStyle m_style;
    int m_targetCount = 3;
    std::string m_backgroundSrc;  // background image path for fragment texturing
    std::vector<glm::vec4> m_styleRingColors;
    glm::vec4 m_styleBullseyeColor{0.8f, 0.0f, 0.0f, 1.0f};
    int m_styleRings = 4;
    float m_styleBullseyeSize = 0.14f;

    // State
    std::vector<TargetInfo> m_targets;
    std::vector<Particle> m_particles;
    std::vector<HitRing> m_hitRings;
    std::vector<HitFlash> m_hitFlashes;
    std::vector<HitSpike> m_hitSpikes;
    std::vector<Fragment> m_fragments;
    std::vector<DamagePatch> m_damages;
    std::vector<FloatingText> m_floatingTexts;
    CrosshairConfig m_crosshair;
    float m_crosshairX = 0, m_crosshairY = 0;

    int m_score = 0;
    int m_combo = 0;
    int m_missCombo = 0;
    float m_timeLeft = 180.0f;
    float m_shakeTimer = 0;        // screen shake countdown
    float m_shakeIntensity = 0;    // shake pixel offset
    int m_nextTargetId = 0;
};

} // namespace application::game

#endif // GAME_MODULE_H
