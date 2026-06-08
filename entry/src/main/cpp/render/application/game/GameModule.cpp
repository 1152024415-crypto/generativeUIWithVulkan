#include "GameModule.h"
#include "logger_common.h"
#include "agenui_engine/core/RenderContext.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sstream>

// For JSON parsing
#include "agenui_engine/thirdparty/json.hpp"

namespace application::game {

static float randf() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

static glm::vec4 hexToVec4(const std::string& hex) {
    glm::vec4 result(1.0f, 1.0f, 1.0f, 1.0f);
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.length() >= 6) {
        result.r = std::stoul(h.substr(0, 2), nullptr, 16) / 255.0f;
        result.g = std::stoul(h.substr(2, 2), nullptr, 16) / 255.0f;
        result.b = std::stoul(h.substr(4, 2), nullptr, 16) / 255.0f;
    }
    return result;
}

static bool isYellowish(const glm::vec4& color) {
    return color.r > 0.65f && color.g > 0.45f && color.b < 0.25f;
}

static bool isDarkRed(const glm::vec4& color) {
    return color.r > 0.25f && color.g < 0.12f && color.b < 0.12f;
}

static glm::vec4 targetInnerRingColor(const glm::vec4& bullseyeColor) {
    return glm::vec4(
        std::min(1.0f, bullseyeColor.r + 0.08f),
        std::min(1.0f, bullseyeColor.g + 0.05f),
        std::min(1.0f, bullseyeColor.b + 0.05f),
        bullseyeColor.a);
}

static void appendRingCommands(std::vector<dsl::DslRenderCommand>& cmds,
                               float cx, float cy,
                               float outerRadius,
                               float thickness,
                               const glm::vec4& color) {
    const int segments = 32;
    const float innerRadius = std::max(1.0f, outerRadius - thickness);
    std::vector<glm::vec2> outer;
    std::vector<glm::vec2> inner;
    outer.reserve(segments);
    inner.reserve(segments);

    for (int i = 0; i < segments; i++) {
        float angle = (static_cast<float>(i) / segments) * 3.14159265f * 2.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        outer.push_back(glm::vec2(cx + c * outerRadius, cy + s * outerRadius));
        inner.push_back(glm::vec2(cx + c * innerRadius, cy + s * innerRadius));
    }

    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        dsl::DslRenderCommand triA;
        triA.type = dsl::DslRenderCommand::Type::Polygon;
        triA.polygonCenter = outer[i];
        triA.polygonVertices = { outer[i], outer[next], inner[i] };
        triA.color = color;
        cmds.push_back(triA);

        dsl::DslRenderCommand triB;
        triB.type = dsl::DslRenderCommand::Type::Polygon;
        triB.polygonCenter = inner[i];
        triB.polygonVertices = { inner[i], outer[next], inner[next] };
        triB.color = color;
        cmds.push_back(triB);
    }
}

void GameModule::InitFromStyle(const std::string& styleJson, float designW, float designH) {
    m_designW = designW;
    m_designH = designH;
    m_phase = GamePhase::Playing;
    m_score = 0;
    m_combo = 0;
    m_missCombo = 0;
    m_timeLeft = 180.0f;
    m_shakeTimer = 0;
    m_shakeIntensity = 0;
    m_nextTargetId = 0;
    m_targets.clear();
    m_particles.clear();
    m_hitRings.clear();
    m_hitFlashes.clear();
    m_hitSpikes.clear();
    m_fragments.clear();
    m_damages.clear();
    m_floatingTexts.clear();
    m_backgroundSrc.clear();

    // Crosshair starts at center
    m_crosshairX = designW / 2.0f;
    m_crosshairY = designH / 2.0f;

    // Default outer ring colors alternate from outside to inside.
    static const float defaultColors[5][3] = {
        {0.9f, 0.9f, 0.9f}, {0.2f, 0.4f, 0.8f},
        {0.9f, 0.9f, 0.9f}, {0.2f, 0.4f, 0.8f}, {0.9f, 0.9f, 0.9f}
    };

    try {
        auto json = nlohmann::json::parse(styleJson, nullptr, false);

        // Parse target config (type: "target" from updateComponents)
        if (json.contains("target")) {
            auto& ts = json["target"];
            m_targetCount = ts.value("targetCount", 3);
            m_targetCount = std::max(1, std::min(m_targetCount, 10));
            m_styleRings = ts.value("rings", 4);
            m_styleBullseyeSize = ts.value("bullseyeSize", 0.14f);
            m_style.targetCount = m_targetCount;

            // Parse ring colors (shared style for all targets, including respawns)
            m_styleRingColors.clear();
            if (ts.contains("colors") && ts["colors"].is_array()) {
                for (auto& c : ts["colors"]) {
                    m_styleRingColors.push_back(hexToVec4(c.get<std::string>()));
                }
            }
            if (m_styleRingColors.empty()) {
                for (int j = 0; j < 5; j++) {
                    m_styleRingColors.push_back(glm::vec4(defaultColors[j][0], defaultColors[j][1], defaultColors[j][2], 1.0f));
                }
            }

            // Parse bullseye color (shared style)
            if (ts.contains("bullseyeColor")) {
                m_styleBullseyeColor = hexToVec4(ts["bullseyeColor"].get<std::string>());
            }
            int visibleRingCount = std::max(0, m_styleRings - 1);
            int innerRingIndex = std::min(visibleRingCount, static_cast<int>(m_styleRingColors.size())) - 1;
            if (innerRingIndex >= 0 &&
                (isYellowish(m_styleRingColors[innerRingIndex]) || isDarkRed(m_styleRingColors[innerRingIndex]))) {
                m_styleRingColors[innerRingIndex] = targetInnerRingColor(m_styleBullseyeColor);
            }

            for (int i = 0; i < m_targetCount; i++) {
                TargetInfo t;
                t.id = m_nextTargetId++;
                t.rings = m_styleRings;
                t.bullseyeSize = m_styleBullseyeSize;
                t.ringColors = m_styleRingColors;
                t.bullseyeColor = m_styleBullseyeColor;

                t.radius = m_designH * 0.09f + randf() * m_designH * 0.06f;
                t.cx = t.radius + randf() * (designW - t.radius * 2);
                t.cy = t.radius + randf() * (designH - t.radius * 2);
                t.alive = true;
                m_targets.push_back(t);
            }
        } else {
            m_targetCount = 3;
            for (int i = 0; i < m_targetCount; i++) {
                spawnTarget();
            }
        }

        // Parse crosshair config (type: "crosshair" from updateComponents)
        if (json.contains("crosshair")) {
            auto& ch = json["crosshair"];
            m_crosshair.size = ch.value("size", 48.0f);
        }
        m_crosshair.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        m_crosshair.centerColor = m_crosshair.color;

        // Parse duration from updateDataModel
        if (json.contains("updateDataModel") && json["updateDataModel"].contains("duration")) {
            m_timeLeft = json["updateDataModel"]["duration"].get<float>();
        }

        // Parse background image path for fragment texturing
        if (json.contains("_backgroundSrc")) {
            m_backgroundSrc = json["_backgroundSrc"].get<std::string>();
        }

    } catch (const std::exception& e) {
        LOGE("GameModule::InitFromStyle parse error: %s", e.what());
        m_targetCount = 3;
        for (int i = 0; i < m_targetCount; i++) {
            spawnTarget();
        }
    }
}

void GameModule::spawnTarget() {
    TargetInfo t;
    t.id = m_nextTargetId++;
    t.rings = m_styleRings;
    t.radius = m_designH * 0.09f + randf() * m_designH * 0.06f;
    t.cx = t.radius + randf() * (m_designW - t.radius * 2);
    t.cy = t.radius + randf() * (m_designH - t.radius * 2);
    t.alive = true;
    t.bullseyeSize = m_styleBullseyeSize;
    t.bullseyeColor = m_styleBullseyeColor;
    t.ringColors = m_styleRingColors;

    m_targets.push_back(t);
}

void GameModule::Update(float dt) {
    if (m_phase != GamePhase::Playing) return;

    // Timer
    m_timeLeft -= dt;
    // Decay screen flash + shake
    if (m_shakeTimer > 0) m_shakeTimer = std::max(0.0f, m_shakeTimer - dt);
    if (m_timeLeft <= 0) {
        m_timeLeft = 0;
        m_phase = GamePhase::Over;
        return;
    }

    // Update particles
    for (auto& p : m_particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.vy += 0.15f; // gravity
        p.vx *= 0.96f;  // drag
        p.vy *= 0.96f;
        p.radius *= 0.98f;
        p.rotation += p.rotSpeed;
        p.rotSpeed *= 0.97f;
        p.life -= 0.025f;
    }
    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
            [](const Particle& p) { return p.life <= 0; }),
        m_particles.end());

    // Update floating texts (rise up and fade)
    for (auto& ft : m_floatingTexts) {
        ft.life -= 0.02f;
        ft.y = ft.startY - (1.0f - ft.life) * 80.0f;
        float pulse = 1.0f - ft.life;
        ft.scale = 1.0f + std::sin(std::min(1.0f, pulse * 4.0f) * 3.14159265f) * 0.18f;
    }
    m_floatingTexts.erase(
        std::remove_if(m_floatingTexts.begin(), m_floatingTexts.end(),
            [](const FloatingText& ft) { return ft.life <= 0; }),
        m_floatingTexts.end());

    for (auto& ring : m_hitRings) {
        ring.radius += ring.growSpeed;
        ring.growSpeed *= 0.98f;
        ring.thickness *= 0.985f;
        ring.life -= 0.04f;
    }
    m_hitRings.erase(
        std::remove_if(m_hitRings.begin(), m_hitRings.end(),
            [](const HitRing& ring) { return ring.life <= 0.0f || ring.thickness <= 0.5f; }),
        m_hitRings.end());

    for (auto& flash : m_hitFlashes) {
        flash.radius += flash.growSpeed;
        flash.growSpeed *= 0.96f;
        flash.life -= 0.06f;
    }
    m_hitFlashes.erase(
        std::remove_if(m_hitFlashes.begin(), m_hitFlashes.end(),
            [](const HitFlash& flash) { return flash.life <= 0.0f; }),
        m_hitFlashes.end());

    for (auto& spike : m_hitSpikes) {
        spike.length += spike.growSpeed;
        spike.width *= 0.985f;
        spike.growSpeed *= 0.95f;
        spike.life -= 0.05f;
    }
    m_hitSpikes.erase(
        std::remove_if(m_hitSpikes.begin(), m_hitSpikes.end(),
            [](const HitSpike& spike) { return spike.life <= 0.0f || spike.width <= 0.5f; }),
        m_hitSpikes.end());

    // Update fragments in-place
    float floorY = m_designH;
    size_t writeIdx = 0;
    for (size_t readIdx = 0; readIdx < m_fragments.size(); readIdx++) {
        auto& f = m_fragments[readIdx];
        if (!f.heavy) {
            f.x += f.vx;
            f.y += f.vy;
            f.vy += f.gravity;
            f.vx *= 0.96f;
            f.life -= 0.03f;
            if (f.life <= 0) continue;
        } else if (!f.settled) {
            f.x += f.vx;
            f.y += f.vy;
            f.vx *= 0.995f;
            f.vy += f.gravity;
            f.rotation += f.rotSpeed;
            if (f.vy > 25.0f) f.vy = 25.0f;

            float halfH = f.sizeH * 0.5f;
            if (f.y + halfH >= floorY) {
                f.y = floorY - halfH;
                if (std::fabs(f.vy) < 2.5f) {
                    f.vx = 0;
                    f.vy = 0;
                    f.rotSpeed = 0;
                    f.settled = true;
                } else {
                    f.vy = -f.vy * 0.3f;
                    f.vx *= 0.6f;
                    f.rotSpeed *= 0.5f;
                }
            }
        }
        // else: heavy and settled — stay put on the floor, do not fade
        if (writeIdx != readIdx) m_fragments[writeIdx] = std::move(f);
        writeIdx++;
    }
    m_fragments.resize(writeIdx);

    // Cap total fragments to prevent unlimited growth (keep last ~200)
    if (m_fragments.size() > 200) {
        // Remove oldest settled fragments first
        int toRemove = static_cast<int>(m_fragments.size()) - 200;
        for (auto it = m_fragments.begin(); it != m_fragments.end() && toRemove > 0; ) {
            if (it->settled) {
                it = m_fragments.erase(it);
                toRemove--;
            } else {
                ++it;
            }
        }
        // If still over limit, trim from front
        if (m_fragments.size() > 200) {
            m_fragments.erase(m_fragments.begin(), m_fragments.begin() + (m_fragments.size() - 200));
        }
    }

    // Update destroyed targets (fade timer)
    for (auto& t : m_targets) {
        if (!t.alive) {
            t.destroyTimer -= dt;
            t.hitScale = std::max(0.78f, 1.0f - (0.3f - std::max(0.0f, t.destroyTimer)) * 0.85f);
            t.hitFlash = std::min(1.0f, std::max(0.0f, t.destroyTimer / 0.14f));
        }
    }
    // Remove fully faded targets and respawn
    int aliveCount = 0;
    for (auto& t : m_targets) {
        if (t.alive) aliveCount++;
    }
    m_targets.erase(
        std::remove_if(m_targets.begin(), m_targets.end(),
            [](const TargetInfo& t) { return !t.alive && t.destroyTimer <= 0; }),
        m_targets.end());

    // Respawn destroyed targets to maintain count
    while (aliveCount < m_targetCount) {
        spawnTarget();
        aliveCount++;
    }

}

int GameModule::HandleTouch(float designX, float designY) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_phase != GamePhase::Playing) return -1;

    m_crosshairX = designX;
    m_crosshairY = designY;

    // Check hit on targets (front to back, last drawn = on top)
    for (int i = static_cast<int>(m_targets.size()) - 1; i >= 0; i--) {
        auto& t = m_targets[i];
        if (!t.alive) continue;

        float dx = designX - t.cx;
        float dy = designY - t.cy;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= t.radius) {
            // Hit! Calculate level using shooting_game's ratio-based thresholds
            // Level 4 (bullseye): ratio < 0.15, Level 3: < 0.35, etc.
            float ratio = dist / t.radius;
            int level;
            if (ratio < 0.15f)      level = 4;
            else if (ratio < 0.35f) level = 3;
            else if (ratio < 0.55f) level = 2;
            else if (ratio < 0.75f) level = 1;
            else                    level = 0;

            t.alive = false;
            t.destroyTimer = 0.34f;
            t.hitLevel = level;
            t.hitScale = 0.82f;
            t.hitFlash = 1.0f;

            // Score: all multiples of 10, routine play stays in 5k-30k range
            static const int BASE_SCORES[] = {10, 20, 30, 40, 50};
            int baseScore = (level < 5) ? BASE_SCORES[level] : 50;
            int comboBonus = std::min(m_combo, 8) * 10;  // +10 per combo, max +80
            int awardedScore = baseScore + comboBonus;    // always multiple of 10
            m_score += awardedScore;
            m_combo++;
            m_missCombo = 0;

            // Spawn effects (particles only for target hits)
            glm::vec4 hitColor = (level < static_cast<int>(t.ringColors.size()))
                ? t.ringColors[level] : glm::vec4(1, 1, 0, 1);
            spawnParticles(t.cx, t.cy, level, hitColor);
            spawnHitEffects(t.cx, t.cy, t.radius, level, hitColor);

            // Floating score text (shooting_game style)
            {
                static const char* LABELS[] = {"", "GOOD", "GREAT!", "EXCELLENT!", "BULLSEYE!"};
                FloatingText ft;
                ft.score = awardedScore;
                ft.text = "+" + std::to_string(ft.score);
                ft.x = t.cx;
                ft.y = t.cy;
                ft.startY = t.cy;
                ft.level = level;
                ft.life = 1.0f;
                ft.scale = 1.18f;
                m_floatingTexts.push_back(ft);
            }

            return level;
        }
    }

    // Miss — penalty ≈ level-2 hit, grows with consecutive misses
    m_combo = 0;
    m_missCombo++;
    int streakPenalty = std::min(m_missCombo, 5) * 10;     // grows with consecutive misses
    int penalty = 30 + streakPenalty;
    m_score = std::max(0, m_score - penalty);

    // --- Miss visual impact: intensifies with miss streak ---
    float missIntensity = std::min(static_cast<float>(m_missCombo), 5.0f);
    m_shakeTimer = 0.2f + missIntensity * 0.05f;       // 0.2s → 0.45s
    m_shakeIntensity = 6.0f + missIntensity * 3.0f + static_cast<float>(penalty) / 80.0f;

    // Floating penalty text with "MISS!" label
    {
        float penScale = 1.0f + std::min(static_cast<float>(penalty) / 200.0f, 0.8f);
        FloatingText ft;
        ft.score = -penalty;
        ft.text = "-" + std::to_string(penalty);
        ft.label = "MISS!";
        ft.x = designX;
        ft.y = designY;
        ft.startY = designY;
        ft.level = -1;
        ft.life = 1.2f;
        ft.scale = penScale;
        m_floatingTexts.push_back(ft);
    }

    // Red shockwave — 3 expanding circles at different speeds (Circle type, proven color correctness)
    {
        float baseRadius = 30.0f + std::min(static_cast<float>(penalty) / 6.0f, 40.0f);
        float intens = std::min(static_cast<float>(m_missCombo), 5.0f);
        // Fast circle
        HitFlash f1;
        f1.x = designX; f1.y = designY;
        f1.radius = baseRadius * 0.15f;
        f1.growSpeed = 12.0f + intens * 2.0f;
        f1.life = 0.6f;
        f1.color = glm::vec4(1.0f, 0.15f, 0.1f, 0.85f);
        m_hitFlashes.push_back(f1);
        // Medium circle
        HitFlash f2;
        f2.x = designX; f2.y = designY;
        f2.radius = baseRadius * 0.1f;
        f2.growSpeed = 7.0f + intens * 1.5f;
        f2.life = 0.8f;
        f2.color = glm::vec4(1.0f, 0.25f, 0.1f, 0.7f);
        m_hitFlashes.push_back(f2);
        // Slow outer circle
        HitFlash f3;
        f3.x = designX; f3.y = designY;
        f3.radius = baseRadius * 0.05f;
        f3.growSpeed = 4.0f + intens;
        f3.life = 1.0f;
        f3.color = glm::vec4(0.9f, 0.1f, 0.05f, 0.55f);
        m_hitFlashes.push_back(f3);
    }

    // Red spikes — 10 sharp spikes radiating outward
    {
        int spikeCount = 10;
        float spikeLen = 80.0f + std::min(static_cast<float>(penalty) / 4.0f, 100.0f);
        for (int i = 0; i < spikeCount; i++) {
            HitSpike spike;
            spike.x = designX;
            spike.y = designY;
            spike.angle = (static_cast<float>(i) / spikeCount) * 3.14159265f * 2.0f
                + (randf() - 0.5f) * 0.18f;
            spike.length = spikeLen * (0.7f + randf() * 0.5f);
            spike.width = 8.0f + randf() * 6.0f;
            spike.growSpeed = 5.0f + randf() * 3.0f;
            spike.life = 0.7f + randf() * 0.25f;
            spike.color = glm::vec4(1.0f, 0.1f + randf() * 0.15f, 0.05f, 0.9f);
            m_hitSpikes.push_back(spike);
        }
    }

    // Red center flash
    {
        HitFlash flash;
        flash.x = designX; flash.y = designY;
        flash.radius = 28.0f + std::min(static_cast<float>(penalty) / 8.0f, 30.0f);
        flash.growSpeed = 12.0f;
        flash.life = 0.5f;
        flash.color = glm::vec4(1.0f, 0.2f, 0.1f, 1.0f);
        m_hitFlashes.push_back(flash);
    }

    // Damage patch: gray circle with 5-6 short spikes, each hole unique
    {
        const float twoPi = 3.14159265f * 2.0f;
        float baseR = 36.0f + randf() * 10.0f;        // 36..46
        float spikeExtra = baseR * (0.3f + randf() * 0.2f); // spike protrusion: 30-50% of baseR
        DamagePatch dp;
        dp.cx = designX;
        dp.cy = designY;
        int spikes = 5 + (std::rand() % 2);  // 5 or 6
        int total = spikes * 2;
        float step = twoPi / static_cast<float>(total);
        for (int i = 0; i < total; i++) {
            float a = static_cast<float>(i) * step + (randf() - 0.5f) * step * 0.25f;
            float hr;
            if ((i & 1) == 0) {
                hr = baseR + spikeExtra * (0.7f + randf() * 0.6f);  // spike: varies per tip
            } else {
                hr = baseR * (0.85f + randf() * 0.15f);             // valley: 85-100% of baseR
            }
            dp.polyX.push_back(designX + std::cos(a) * hr);
            dp.polyY.push_back(designY + std::sin(a) * hr);
        }
        m_damages.push_back(std::move(dp));
    }
    if (m_damages.size() > 30) m_damages.erase(m_damages.begin());
    // Heavy fragments
    spawnFragments(designX, designY);
    return -1;
}

void GameModule::UpdateCrosshair(float designX, float designY) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_crosshairX = designX;
    m_crosshairY = designY;
}

void GameModule::spawnParticles(float cx, float cy, int level, const glm::vec4& color) {
    // Per-level color schemes (like shooting_game)
    static const float COLOR_SCHEMES[][3] = {
        {0.67f, 0.67f, 0.67f}, // level 0: grey
        {0.27f, 0.67f, 1.0f},  // level 1: blue
        {1.0f, 0.4f, 0.0f},    // level 2: orange
        {1.0f, 0.0f, 0.4f},    // level 3: pink
        {1.0f, 0.0f, 0.0f},    // level 4: red
    };
    static const int COUNTS[] = {8, 14, 20, 28, 36};
    static const float SPEEDS[] = {1.5f, 2.0f, 2.8f, 4.2f, 6.0f};
    static const float SIZE_MULS[] = {1.2f, 1.6f, 2.0f, 2.8f, 3.8f};

    if (level < 0) level = 0;
    if (level > 4) level = 4;

    int count = COUNTS[level];
    float speedMul = SPEEDS[level];
    float sizeMul = SIZE_MULS[level];
    float cr = COLOR_SCHEMES[level][0];
    float cg = COLOR_SCHEMES[level][1];
    float cb = COLOR_SCHEMES[level][2];

    for (int i = 0; i < count; i++) {
        float angle = randf() * 3.14159265f * 2.0f;
        float speed = (1.5f + randf() * 5.0f) * speedMul;

        Particle p;
        p.x = cx;
        p.y = cy;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.radius = (2.8f + randf() * 5.2f) * sizeMul;
        p.r = std::min(1.0f, std::max(0.0f, cr + (randf() - 0.5f) * 0.3f));
        p.g = std::min(1.0f, std::max(0.0f, cg + (randf() - 0.5f) * 0.3f));
        p.b = std::min(1.0f, std::max(0.0f, cb + (randf() - 0.5f) * 0.3f));
        p.life = 1.0f;
        p.kind = (i % 2 == 0) ? 1 : 0;
        p.rotation = randf() * 360.0f;
        p.rotSpeed = (randf() - 0.5f) * 22.0f;
        m_particles.push_back(p);
    }

    // Level 3+: ring particles (evenly spaced in a circle, yellow)
    if (level >= 3) {
        int ringCount = (level == 4) ? 24 : 12;
        for (int i = 0; i < ringCount; i++) {
            float angle = (static_cast<float>(i) / ringCount) * 3.14159265f * 2.0f;
            float speed = 6.0f * speedMul;
            Particle p;
            p.x = cx;
            p.y = cy;
            p.vx = std::cos(angle) * speed;
            p.vy = std::sin(angle) * speed;
            p.radius = 4.8f * sizeMul;
            p.r = 1.0f;
            p.g = 1.0f;
            p.b = 0.0f;  // yellow
            p.life = 1.0f;
            p.kind = 0;
            m_particles.push_back(p);
        }
    }

    int burstShards = 2 + level / 2;
    for (int i = 0; i < burstShards; i++) {
        float angle = randf() * 3.14159265f * 2.0f;
        float speed = 10.0f + randf() * (7.0f + level * 1.6f);
        Particle p;
        p.x = cx;
        p.y = cy;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.radius = 14.0f + randf() * (8.0f + level * 2.0f);
        p.r = std::min(1.0f, color.r * 0.7f + 0.35f);
        p.g = std::min(1.0f, color.g * 0.7f + 0.35f);
        p.b = std::min(1.0f, color.b * 0.7f + 0.35f);
        p.life = 1.0f;
        p.kind = 1;
        p.rotation = randf() * 360.0f;
        p.rotSpeed = (randf() - 0.5f) * 18.0f;
        m_particles.push_back(p);
    }

    if (level >= 4) {
        for (int i = 0; i < 10; i++) {
            float angle = randf() * 3.14159265f * 2.0f;
            float speed = 14.0f + randf() * 8.0f;
            Particle p;
            p.x = cx;
            p.y = cy;
            p.vx = std::cos(angle) * speed;
            p.vy = std::sin(angle) * speed;
            p.radius = 8.0f + randf() * 5.0f;
            p.r = 1.0f;
            p.g = 0.95f;
            p.b = 0.45f;
            p.life = 1.0f;
            p.kind = 1;
            p.rotation = randf() * 360.0f;
            p.rotSpeed = (randf() - 0.5f) * 30.0f;
            m_particles.push_back(p);
        }
    }
}

void GameModule::spawnHitEffects(float cx, float cy, float radius, int level, const glm::vec4& color) {
    HitFlash flash;
    flash.x = cx;
    flash.y = cy;
    flash.radius = radius * (0.16f + level * 0.02f);
    flash.growSpeed = 10.0f + level * 1.8f;
    flash.life = 1.0f;
    flash.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    m_hitFlashes.push_back(flash);

    HitRing ringA;
    ringA.x = cx;
    ringA.y = cy;
    ringA.radius = radius * 0.22f;
    ringA.thickness = 12.0f + level * 2.0f;
    ringA.growSpeed = 9.0f + level * 1.8f;
    ringA.life = 1.0f;
    ringA.color = glm::vec4(color.r, color.g, color.b, 1.0f);
    m_hitRings.push_back(ringA);

    HitRing ringB;
    ringB.x = cx;
    ringB.y = cy;
    ringB.radius = radius * 0.12f;
    ringB.thickness = 20.0f + level * 3.0f;
    ringB.growSpeed = 5.5f + level * 1.2f;
    ringB.life = 0.9f;
    ringB.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.9f);
    m_hitRings.push_back(ringB);

    if (level >= 4) {
        HitRing ringC;
        ringC.x = cx;
        ringC.y = cy;
        ringC.radius = radius * 0.08f;
        ringC.thickness = 16.0f;
        ringC.growSpeed = 12.5f;
        ringC.life = 1.1f;
        ringC.color = glm::vec4(1.0f, 0.86f, 0.2f, 1.0f);
        m_hitRings.push_back(ringC);
    }

    int spikeCount = 3 + level;
    for (int i = 0; i < spikeCount; i++) {
        HitSpike spike;
        spike.x = cx;
        spike.y = cy;
        spike.angle = (static_cast<float>(i) / static_cast<float>(spikeCount)) * 3.14159265f * 2.0f
            + (randf() - 0.5f) * 0.25f;
        spike.length = radius * (0.55f + randf() * 0.35f);
        spike.width = 10.0f + level * 1.6f + randf() * 5.0f;
        spike.growSpeed = 3.5f + level * 0.8f;
        spike.life = 1.0f - randf() * 0.15f;
        spike.color = (level >= 4)
            ? glm::vec4(1.0f, 0.88f, 0.25f, 1.0f)
            : glm::vec4(color.r * 0.8f + 0.2f, color.g * 0.8f + 0.2f, color.b * 0.8f + 0.2f, 1.0f);
        m_hitSpikes.push_back(spike);
    }
}

void GameModule::spawnFragments(float cx, float cy) {
    // Fewer but larger shards with stronger radial impulse so the break reads
    // as a clear "burst outward" instead of a local wobble.
    // origX/origY captures each chunk's spawn position so rendering can sample
    // the matching region of the background image (fragment = broken off piece).
    const int count = 4 + (std::rand() % 2);  // 4..5
    const float minDist = 70.0f;
    const float maxDist = 180.0f;

    for (int i = 0; i < count; i++) {
        float angle = (static_cast<float>(i) / static_cast<float>(count)) * 3.14159265f * 2.0f
                     + (randf() - 0.5f) * 0.28f;
        float dist = minDist + randf() * (maxDist - minDist);

        float wx = cx + std::cos(angle) * dist;
        float wy = cy + std::sin(angle) * dist;
        float fw = 150.0f + randf() * 150.0f;  // width: 150..300
        float fh = 90.0f + randf() * 110.0f;   // height: 90..200

        float spd = 8.0f + randf() * 8.0f;

        Fragment f;
        f.x = wx;
        f.y = wy;
        f.origX = wx;
        f.origY = wy;
        f.vx = std::cos(angle) * spd + (randf() - 0.5f) * 2.5f;
        f.vy = std::sin(angle) * spd * 0.75f - 6.0f - randf() * 4.0f;
        f.sizeW = fw;
        f.sizeH = fh;
        f.r = 1.0f; f.g = 1.0f; f.b = 1.0f;  // unused when sampled from background
        f.life = 1.0f;
        f.gravity = 0.72f;
        f.rotation = (randf() - 0.5f) * 40.0f;
        f.rotSpeed = (randf() - 0.5f) * 16.0f;
        f.heavy = true;
        f.settled = false;
        m_fragments.push_back(f);
    }
}

std::vector<dsl::DslRenderCommand> GameModule::GenerateRenderCommands() {
    std::vector<dsl::DslRenderCommand> cmds;

    // 0. Draw damage patches (irregular star polygon)
    for (const auto& dp : m_damages) {
        int n = static_cast<int>(dp.polyX.size());
        if (n < 3) continue;
        dsl::DslRenderCommand cmd;
        cmd.type = dsl::DslRenderCommand::Type::Polygon;
        cmd.polygonCenter = glm::vec2(dp.cx, dp.cy);
        cmd.color = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
        cmd.polygonVertices.resize(n);
        for (int i = 0; i < n; i++) {
            cmd.polygonVertices[i] = glm::vec2(dp.polyX[i], dp.polyY[i]);
        }
        cmds.push_back(cmd);
    }

    // 1. Draw targets (concentric circles)
    for (const auto& t : m_targets) {
        if (!t.alive && t.destroyTimer <= 0) continue;

        float alpha = t.alive ? 1.0f : std::min(1.0f, std::max(0.0f, t.destroyTimer / 0.3f));
        float scale = t.alive ? 1.0f : t.hitScale;

        int visibleRingCount = std::max(0, t.rings - 1);
        float bullR = t.radius * t.bullseyeSize * scale;
        float ringBand = visibleRingCount > 0
            ? (t.radius * scale - bullR) / static_cast<float>(visibleRingCount)
            : 0.0f;
        for (int i = 0; i < visibleRingCount && !t.ringColors.empty(); i++) {
            float ringRadius = bullR + ringBand * static_cast<float>(visibleRingCount - i);
            auto& rc = t.ringColors[i % std::min(2, static_cast<int>(t.ringColors.size()))];

            dsl::DslRenderCommand cmd;
            cmd.type = dsl::DslRenderCommand::Type::Circle;
            cmd.pos = glm::vec2(t.cx, t.cy);
            cmd.color = glm::vec4(rc.r, rc.g, rc.b, alpha);
            cmd.circleRadius = ringRadius;
            cmds.push_back(cmd);
        }

        dsl::DslRenderCommand bull;
        bull.type = dsl::DslRenderCommand::Type::Circle;
        bull.pos = glm::vec2(t.cx, t.cy);
        bull.color = glm::vec4(t.bullseyeColor.r, t.bullseyeColor.g, t.bullseyeColor.b, alpha);
        bull.circleRadius = bullR;
        cmds.push_back(bull);

        if (!t.alive && t.hitFlash > 0.0f) {
            dsl::DslRenderCommand flash;
            flash.type = dsl::DslRenderCommand::Type::Circle;
            flash.pos = glm::vec2(t.cx, t.cy);
            flash.color = glm::vec4(1.0f, 1.0f, 1.0f, t.hitFlash * 0.8f);
            flash.circleRadius = t.radius * scale * (0.7f + 0.18f * (1.0f - t.hitFlash));
            cmds.push_back(flash);
        }
    }

    for (const auto& flash : m_hitFlashes) {
        dsl::DslRenderCommand cmd;
        cmd.type = dsl::DslRenderCommand::Type::Circle;
        cmd.pos = glm::vec2(flash.x, flash.y);
        cmd.color = glm::vec4(flash.color.r, flash.color.g, flash.color.b, flash.life * 0.95f);
        cmd.circleRadius = flash.radius;
        cmds.push_back(cmd);
    }

    for (const auto& ring : m_hitRings) {
        const int segments = 12;
        std::vector<glm::vec2> outer;
        std::vector<glm::vec2> inner;
        outer.reserve(segments);
        inner.reserve(segments);
        float innerRadius = std::max(1.0f, ring.radius - ring.thickness);
        for (int i = 0; i < segments; i++) {
            float angle = (static_cast<float>(i) / segments) * 3.14159265f * 2.0f;
            float c = std::cos(angle);
            float s = std::sin(angle);
            outer.push_back(glm::vec2(ring.x + c * ring.radius, ring.y + s * ring.radius));
            inner.push_back(glm::vec2(ring.x + c * innerRadius, ring.y + s * innerRadius));
        }
        for (int i = 0; i < segments; i++) {
            int next = (i + 1) % segments;
            dsl::DslRenderCommand triA;
            triA.type = dsl::DslRenderCommand::Type::Polygon;
            triA.polygonCenter = outer[i];
            triA.polygonVertices = { outer[i], outer[next], inner[i] };
            triA.color = glm::vec4(ring.color.r, ring.color.g, ring.color.b, ring.life * ring.color.a);
            cmds.push_back(triA);

            dsl::DslRenderCommand triB;
            triB.type = dsl::DslRenderCommand::Type::Polygon;
            triB.polygonCenter = inner[i];
            triB.polygonVertices = { inner[i], outer[next], inner[next] };
            triB.color = triA.color;
            cmds.push_back(triB);
        }
    }

    for (const auto& spike : m_hitSpikes) {
        float cosA = std::cos(spike.angle);
        float sinA = std::sin(spike.angle);
        glm::vec2 dir(cosA, sinA);
        glm::vec2 normal(-sinA, cosA);
        glm::vec2 base(spike.x, spike.y);
        glm::vec2 tip = base + dir * spike.length;
        glm::vec2 left = base + normal * spike.width * 0.5f;
        glm::vec2 right = base - normal * spike.width * 0.5f;
        glm::vec2 inner = base + dir * (spike.length * 0.18f);

        dsl::DslRenderCommand triA;
        triA.type = dsl::DslRenderCommand::Type::Polygon;
        triA.polygonCenter = inner;
        triA.polygonVertices = { left, tip, right };
        triA.color = glm::vec4(spike.color.r, spike.color.g, spike.color.b, spike.life * spike.color.a);
        cmds.push_back(triA);

        dsl::DslRenderCommand triB;
        triB.type = dsl::DslRenderCommand::Type::Polygon;
        triB.polygonCenter = base;
        triB.polygonVertices = { left, inner, right };
        triB.color = glm::vec4(spike.color.r * 0.95f, spike.color.g * 0.95f, spike.color.b * 0.95f, spike.life * 0.72f);
        cmds.push_back(triB);
    }

    // 2. Draw fragments — heavy fragments sample the background image so each
    //    shard shows the piece of the scene it "broke off from". The engine's
    //    drawImage(clipVertices,...) builds UVs from (vertex - position) / size,
    //    so we draw the full background at (0,0)-(designW,designH) and clip
    //    to the rotated rect positioned at the fragment's ORIGINAL spawn point.
    //    Translation between origPos and current pos is done by shifting the
    //    clip vertices (same shape, same UVs → piece looks identical, just moves).
    for (const auto& f : m_fragments) {
        float halfW = f.sizeW * 0.5f;
        float halfH = f.sizeH * 0.5f;

        // Rotated corner offsets around fragment center
        float rad = f.rotation * 3.14159265f / 180.0f;
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);
        float offsets[4][2] = {
            { -halfW, -halfH },
            {  halfW, -halfH },
            {  halfW,  halfH },
            { -halfW,  halfH },
        };

        if (f.heavy && !m_backgroundSrc.empty()) {
            // Engine computes UVs from (clipVertex - imagePosition) / imageSize.
            // Clip polygon sits at the CURRENT position (so pixels draw there),
            // and we offset the image origin by (dx, dy) = (f.x - f.origX, ...)
            // so that the UV at the clip vertex evaluates to (f.origX + rx) /
            // designW — i.e. the original region of the background.
            float dx = f.x - f.origX;
            float dy = f.y - f.origY;

            std::vector<glm::vec2> clipVerts;
            clipVerts.reserve(4);
            for (int i = 0; i < 4; i++) {
                float rx = offsets[i][0] * cosR - offsets[i][1] * sinR;
                float ry = offsets[i][0] * sinR + offsets[i][1] * cosR;
                clipVerts.push_back(glm::vec2(f.x + rx, f.y + ry));
            }

            dsl::DslRenderCommand cmd;
            cmd.type = dsl::DslRenderCommand::Type::Image;
            cmd.imagePath = m_backgroundSrc;
            cmd.pos = glm::vec2(dx, dy);
            cmd.size = glm::vec2(m_designW, m_designH);
            cmd.clipVertices = std::move(clipVerts);
            cmd.clipCenter = glm::vec2(f.x, f.y);
            cmds.push_back(cmd);
        } else {
            // Non-heavy (or no background available) — fall back to colored polygon
            std::vector<glm::vec2> verts;
            verts.reserve(4);
            for (int i = 0; i < 4; i++) {
                float rx = offsets[i][0] * cosR - offsets[i][1] * sinR;
                float ry = offsets[i][0] * sinR + offsets[i][1] * cosR;
                verts.push_back(glm::vec2(f.x + rx, f.y + ry));
            }
            dsl::DslRenderCommand cmd;
            cmd.type = dsl::DslRenderCommand::Type::Polygon;
            cmd.polygonCenter = glm::vec2(f.x, f.y);
            cmd.polygonVertices = std::move(verts);
            cmd.color = glm::vec4(f.r, f.g, f.b, f.life * 0.5f);
            cmds.push_back(cmd);
        }
    }

    // 3. Draw particles (ember circles + shard diamonds)
    for (const auto& p : m_particles) {
        if (p.kind == 1) {
            float half = p.radius * 0.7f;
            float rad = p.rotation * 3.14159265f / 180.0f;
            float cosR = std::cos(rad);
            float sinR = std::sin(rad);
            std::vector<glm::vec2> verts;
            const glm::vec2 base[4] = {
                glm::vec2(0.0f, -half),
                glm::vec2(half * 0.8f, 0.0f),
                glm::vec2(0.0f, half),
                glm::vec2(-half * 0.8f, 0.0f)
            };
            for (const auto& v : base) {
                verts.push_back(glm::vec2(
                    p.x + v.x * cosR - v.y * sinR,
                    p.y + v.x * sinR + v.y * cosR));
            }
            dsl::DslRenderCommand cmd;
            cmd.type = dsl::DslRenderCommand::Type::Polygon;
            cmd.polygonCenter = glm::vec2(p.x, p.y);
            cmd.polygonVertices = std::move(verts);
            cmd.color = glm::vec4(p.r, p.g, p.b, p.life);
            cmds.push_back(cmd);
        } else {
            dsl::DslRenderCommand cmd;
            cmd.type = dsl::DslRenderCommand::Type::Circle;
            cmd.pos = glm::vec2(p.x, p.y);
            cmd.color = glm::vec4(p.r, p.g, p.b, p.life);
            cmd.circleRadius = p.radius * 0.5f;
            cmds.push_back(cmd);
        }
    }

    // 4. Draw crosshair (outer ring + two bars + center dot)
    if (m_phase == GamePhase::Playing) {
        float cs = std::max(m_crosshair.size, m_designH * 0.038f);
        float thick = std::max(6.0f, cs * 0.13f);
        float ringOuterRadius = cs * 0.82f;
        float ringThickness = std::max(4.0f, thick * 0.65f);
        bool hideOuterRingOnDamage = false;
        for (const auto& dp : m_damages) {
            float dx = m_crosshairX - dp.cx;
            float dy = m_crosshairY - dp.cy;
            if (dx * dx + dy * dy <= 90.0f * 90.0f) {
                hideOuterRingOnDamage = true;
                break;
            }
        }

        if (!hideOuterRingOnDamage) {
            appendRingCommands(cmds, m_crosshairX, m_crosshairY, ringOuterRadius,
                               ringThickness, m_crosshair.color);
        }

        // Horizontal bar
        dsl::DslRenderCommand hBar;
        hBar.type = dsl::DslRenderCommand::Type::Rect;
        hBar.pos = glm::vec2(m_crosshairX - cs, m_crosshairY - thick * 0.5f);
        hBar.size = glm::vec2(cs * 2, thick);
        hBar.color = m_crosshair.color;
        cmds.push_back(hBar);

        // Vertical bar
        dsl::DslRenderCommand vBar;
        vBar.type = dsl::DslRenderCommand::Type::Rect;
        vBar.pos = glm::vec2(m_crosshairX - thick * 0.5f, m_crosshairY - cs);
        vBar.size = glm::vec2(thick, cs * 2);
        vBar.color = m_crosshair.color;
        cmds.push_back(vBar);

        // Center dot
        float dotR = std::max(6.0f, thick * 0.95f);
        dsl::DslRenderCommand dot;
        dot.type = dsl::DslRenderCommand::Type::Circle;
        dot.pos = glm::vec2(m_crosshairX, m_crosshairY);
        dot.color = m_crosshair.centerColor;
        dot.circleRadius = dotR;
        cmds.push_back(dot);
    }

    // 4.5 Floating hit texts
    for (const auto& ft : m_floatingTexts) {
        // Miss penalty text — size scales with penalty amount
        if (ft.level < 0) {
            dsl::DslRenderCommand st;
            st.type = dsl::DslRenderCommand::Type::Text;
            float baseFontSize = 60.0f + std::min(static_cast<float>(std::abs(ft.score)) / 5.0f, 50.0f);
            float fontSize = baseFontSize * ft.scale;
            st.fontSize = static_cast<uint32_t>(baseFontSize);  // fixed size for atlas caching
            float textW = static_cast<float>(ft.text.size()) * fontSize * 0.5f;
            // Dark shadow
            dsl::DslRenderCommand shadow = st;
            shadow.fontSize = st.fontSize;
            shadow.pos = glm::vec2(ft.x - textW * 0.5f + 5.0f, ft.y + 5.0f);
            shadow.color = glm::vec4(0.2f, 0.0f, 0.0f, ft.life * 0.8f);
            shadow.text = ft.text;
            cmds.push_back(shadow);
            // Red penalty number
            st.pos = glm::vec2(ft.x - textW * 0.5f, ft.y);
            st.color = glm::vec4(1.0f, 0.15f, 0.1f, ft.life);
            st.text = ft.text;
            cmds.push_back(st);
            // "MISS!" label below
            if (!ft.label.empty()) {
                float labelSize = 44.0f * ft.scale;
                dsl::DslRenderCommand lt;
                lt.type = dsl::DslRenderCommand::Type::Text;
                lt.fontSize = 44;  // fixed size for atlas caching
                float labelW = static_cast<float>(ft.label.size()) * labelSize * 0.5f;
                lt.pos = glm::vec2(ft.x - labelW * 0.5f, ft.y + fontSize + 6.0f);
                lt.color = glm::vec4(1.0f, 0.3f, 0.15f, ft.life * 0.9f);
                lt.text = ft.label;
                cmds.push_back(lt);
            }
            continue;
        }

        static const char* LABELS[] = {"", "GOOD", "GREAT!", "EXCELLENT!", "BULLSEYE!"};
        int lvl = ft.level;
        if (lvl < 0) lvl = 0;
        if (lvl > 4) lvl = 4;

        // Score text
        dsl::DslRenderCommand st;
        st.type = dsl::DslRenderCommand::Type::Text;
        float baseFontSize = (lvl >= 3) ? 88.0f : (lvl >= 1 ? 72.0f : 56.0f);
        float fontSize = baseFontSize * ft.scale;
        st.fontSize = static_cast<uint32_t>(baseFontSize);  // fixed size for atlas caching
        float textW = static_cast<float>(ft.text.size()) * fontSize * 0.5f;
        dsl::DslRenderCommand shadow = st;
        shadow.fontSize = st.fontSize;
        shadow.pos = glm::vec2(ft.x - textW * 0.5f + 5.0f, ft.y + 5.0f);
        shadow.color = glm::vec4(0.1f, 0.08f, 0.08f, ft.life * 0.7f);
        shadow.text = ft.text;
        cmds.push_back(shadow);
        st.pos = glm::vec2(ft.x - textW * 0.5f, ft.y);
        // Color: 4=gold, 3=red, 2=orange, 1=lightblue, 0=gray
        if (lvl >= 4)      st.color = glm::vec4(1.0f, 0.84f, 0.0f, ft.life);
        else if (lvl >= 3) st.color = glm::vec4(1.0f, 0.27f, 0.27f, ft.life);
        else if (lvl >= 2) st.color = glm::vec4(1.0f, 0.67f, 0.0f, ft.life);
        else if (lvl >= 1) st.color = glm::vec4(0.4f, 0.8f, 1.0f, ft.life);
        else               st.color = glm::vec4(0.8f, 0.8f, 0.8f, ft.life);
        st.text = ft.text;
        cmds.push_back(st);

        // Label text (below score)
        if (lvl >= 1) {
            dsl::DslRenderCommand lt;
            lt.type = dsl::DslRenderCommand::Type::Text;
            float baseLabelSize = (lvl >= 3) ? 56.0f : 44.0f;
            float labelSize = baseLabelSize * (0.96f + (ft.scale - 1.0f) * 0.6f);
            lt.fontSize = static_cast<uint32_t>(baseLabelSize);  // fixed size for atlas caching
            float labelW = static_cast<float>(std::strlen(LABELS[lvl])) * labelSize * 0.5f;
            dsl::DslRenderCommand ltShadow = lt;
            ltShadow.fontSize = lt.fontSize;
            ltShadow.pos = glm::vec2(ft.x - labelW * 0.5f + 4.0f, ft.y + fontSize + 8.0f);
            ltShadow.color = glm::vec4(0.1f, 0.08f, 0.08f, ft.life * 0.65f);
            ltShadow.text = LABELS[lvl];
            cmds.push_back(ltShadow);
            lt.pos = glm::vec2(ft.x - labelW * 0.5f, ft.y + fontSize + 4.0f);
            lt.color = st.color;
            lt.text = LABELS[lvl];
            cmds.push_back(lt);
        }
    }

    // 5. HUD text (score, combo, time) — apply shake offset
    float shakeX = 0, shakeY = 0;
    if (m_shakeTimer > 0) {
        float t = m_shakeTimer / 0.25f;
        shakeX = (randf() - 0.5f) * 2.0f * m_shakeIntensity * t;
        shakeY = (randf() - 0.5f) * 2.0f * m_shakeIntensity * t;
    }

    dsl::DslRenderCommand scoreText;
    scoreText.type = dsl::DslRenderCommand::Type::Text;
    scoreText.pos = glm::vec2(30 + shakeX, 30 + shakeY);
    scoreText.fontSize = 48;
    scoreText.color = glm::vec4(1, 1, 1, 1);
    scoreText.text = "Score: " + std::to_string(m_score);
    cmds.push_back(scoreText);

    if (m_combo > 1) {
        dsl::DslRenderCommand comboText;
        comboText.type = dsl::DslRenderCommand::Type::Text;
        comboText.pos = glm::vec2(30 + shakeX, 90 + shakeY);
        comboText.fontSize = 36;
        comboText.color = glm::vec4(1, 0.85f, 0, 1);
        comboText.text = "Combo x" + std::to_string(m_combo);
        cmds.push_back(comboText);
    }

    dsl::DslRenderCommand timeText;
    timeText.type = dsl::DslRenderCommand::Type::Text;
    timeText.pos = glm::vec2(m_designW - 200 + shakeX, 30 + shakeY);
    timeText.fontSize = 48;
    timeText.color = (m_timeLeft <= 10) ? glm::vec4(1, 0.3f, 0.3f, 1) : glm::vec4(1, 1, 1, 1);
    timeText.text = std::to_string(static_cast<int>(m_timeLeft)) + "s";
    cmds.push_back(timeText);

    // Game over text
    if (m_phase == GamePhase::Over) {
        dsl::DslRenderCommand overText;
        overText.type = dsl::DslRenderCommand::Type::Text;
        overText.pos = glm::vec2(m_designW / 2 - 150, m_designH / 2 - 40);
        overText.fontSize = 72;
        overText.color = glm::vec4(1, 1, 1, 1);
        overText.text = "Game Over";
        cmds.push_back(overText);
    }

    return cmds;
}

void GameModule::RenderFrame(AgenUIEngine::Core::RenderContext* ctx) {
    if (!ctx) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_phase == GamePhase::Playing) {
        Update(0.016f);
    }

    auto cmds = GenerateRenderCommands();

    for (const auto& cmd : cmds) {
        switch (cmd.type) {
            case dsl::DslRenderCommand::Type::Rect:
                ctx->drawRoundedRect(cmd.pos, cmd.size, cmd.radius,
                    glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                break;
            case dsl::DslRenderCommand::Type::Circle:
                ctx->drawCircle(cmd.pos, cmd.circleRadius,
                    glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                break;
            case dsl::DslRenderCommand::Type::Text:
                ctx->drawText(cmd.text, cmd.pos, cmd.fontSize,
                    glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b));
                break;
            case dsl::DslRenderCommand::Type::Image:
                ctx->drawImage(cmd.imagePath, cmd.pos, cmd.size,
                    cmd.rotation, cmd.clipVertices, cmd.clipCenter);
                break;
            case dsl::DslRenderCommand::Type::Polygon:
                ctx->drawPolygon(cmd.polygonCenter, cmd.polygonVertices,
                    glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                break;
            default:
                break;
        }
    }
}

} // namespace application::game
