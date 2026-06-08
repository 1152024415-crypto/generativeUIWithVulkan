#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <vector>
#include <string>
#include "agenui_engine/thirdparty/glm/glm.hpp"

namespace application::game {

struct TargetInfo {
    int id = 0;
    float cx = 0, cy = 0;
    float radius = 80.0f;
    int rings = 4;
    std::vector<glm::vec4> ringColors;
    glm::vec4 bullseyeColor{0.8f, 0.0f, 0.0f, 1.0f};
    float bullseyeSize = 0.14f;
    bool alive = true;
    float destroyTimer = 0.0f;
    int score = 100;
    int hitLevel = -1;
    float hitScale = 1.0f;
    float hitFlash = 0.0f;
};

struct Particle {
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float radius = 4.0f;
    float r = 1, g = 1, b = 1;
    float life = 1.0f;
    float rotation = 0.0f;
    float rotSpeed = 0.0f;
    int kind = 0;
};

struct HitRing {
    float x = 0, y = 0;
    float radius = 10.0f;
    float thickness = 6.0f;
    float growSpeed = 12.0f;
    float life = 1.0f;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct HitFlash {
    float x = 0, y = 0;
    float radius = 12.0f;
    float growSpeed = 10.0f;
    float life = 1.0f;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct HitSpike {
    float x = 0, y = 0;
    float angle = 0.0f;
    float length = 40.0f;
    float width = 10.0f;
    float growSpeed = 5.0f;
    float life = 1.0f;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct Fragment {
    float x = 0, y = 0;
    float origX = 0, origY = 0;  // world position at creation time (for UV mapping)
    float vx = 0, vy = 0;
    float sizeW = 8.0f;  // width of the fragment
    float sizeH = 8.0f;  // height of the fragment
    float r = 1, g = 1, b = 1;
    float life = 1.0f;
    float gravity = 0.55f;
    float rotation = 0.0f;     // current angle in degrees
    float rotSpeed = 0.0f;     // degrees per frame
    bool heavy = false;    // heavy = big chunk from miss: bounces and settles at bottom
    bool settled = false;  // once settled, no more physics
};

// Damage patch: irregular star polygon left on background after miss
struct DamagePatch {
    float cx = 0, cy = 0;
    std::vector<float> polyX;  // polygon vertex X coords
    std::vector<float> polyY;  // polygon vertex Y coords
};

struct CrosshairConfig {
    float size = 48.0f;
    glm::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 centerColor{1.0f, 0.0f, 0.0f, 1.0f};
};

struct GameStyle {
    std::string background;
    int targetCount = 3;
    CrosshairConfig crosshair;
};

struct FloatingText {
    std::string text;
    std::string label;   // optional sub-label ("MISS!", "BULLSEYE!")
    float x = 0, y = 0;
    float startY = 0;
    int level = 0;       // 0-4 hit level, -1 for miss
    int score = 0;       // points gained (negative for miss)
    float life = 1.0f;   // fades over time
    float scale = 1.0f;
};

enum class GamePhase { Playing, Over };

} // namespace application::game

#endif // GAME_TYPES_H
