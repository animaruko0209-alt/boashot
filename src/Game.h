#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace boa {
constexpr float Width = 1280, Height = 720, Top = 110, Bottom = 652;
constexpr float Pi = 3.14159265f;
struct Vec {
    float x = 0, y = 0;
    Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    Vec operator*(float s) const { return {x * s, y * s}; }
};
inline float length(Vec v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline Vec unit(Vec v) { float d = length(v); return d > .001f ? v * (1 / d) : Vec{-1, 0}; }
inline float clamp(float v, float a, float b) { return std::max(a, std::min(v, b)); }
enum class Branch { Base, Attack, Absorb, Defense };
enum class Phase { Title, Playing, Paused, Clear, GameOver, Hangar };
enum class EnemyKind { Scout, Heavy, Rock, Boss };
struct Profile {
    int xp = 0, stage = 1, rank = 0;
    Branch branch = Branch::Base;
    bool toggleAbsorb = false;
};
struct Input {
    Vec move;
    bool absorbPress = false, absorbRelease = false;
    bool firePress = false, fireRelease = false;
    bool chargePress = false, chargeRelease = false;
    bool barrier = false;
};
struct Bullet {
    Vec p, velocity;
    bool rock = false, alive = true, siphoning = false;
    float energy = 6, maxEnergy = 6, damage = 14, radius = 7;
};
struct Shot {
    Vec p, previous;
    float damage = 5, radius = 4, speed = 850;
    bool charged = false, alive = true;
    std::vector<int> hit;
};
struct Enemy {
    int id = 0;
    EnemyKind kind = EnemyKind::Scout;
    Vec p;
    float hp = 20, maxHp = 20, age = 0, nextShot = 1.25f, anchorY = 0;
    bool alive = true;
};
struct Particle { Vec p, velocity; float life = 0, fullLife = 1; int kind = 0; };
struct Stats { int kills = 0, absorbed = 0; float energyGained = 0, biggestCharge = 0; };

class Game {
public:
    Profile profile;
    Phase phase = Phase::Title;
    Vec player{170, 380};
    float hp = 100, energy = 35, charge = 0, clock = 0, invulnerable = 0;
    float fireClock = 0, shake = 0, flash = 0, clearDelay = -1;
    bool absorbing = false, firing = false, charging = false, barrierActive = false, bossSpawned = false;
    int wave = 0, nextId = 1;
    Stats stats;
    std::vector<Bullet> bullets;
    std::vector<Shot> shots;
    std::vector<Enemy> enemies;
    std::vector<Particle> particles;

    float capacity() const { return 120.f + profile.rank * 20.f; }
    float maxHp() const { return profile.branch == Branch::Defense ? 130.f : 100.f; }
    float range() const { return profile.branch == Branch::Absorb ? 360.f : 280.f; }
    float halfAngle() const { return profile.branch == Branch::Absorb ? .82f : .65f; }
    float chargeMultiplier() const { return profile.branch == Branch::Attack ? 1.4f : profile.branch == Branch::Absorb ? .85f : 1.f; }
    float bossAt() const { return 47.f; }
    bool inCone(Vec p) const;
    void start();
    void cancelCharge();
    void stopActions();
    void setToggle(bool toggle);
    bool evolve(Branch branch);
    bool upgrade();
    void update(float dt, const Input& input);
    void controls(const Input& input);
    void updateBullets(float dt);
    void updateWeapons(float dt);
    void updateEnemies(float dt);
    void spawnWave();
    void spawnEnemy(EnemyKind kind, Vec position);
    void enemyFire(Enemy& enemy);
    void burst(Vec p, int kind, int count);
    void hurt(float damage);
    void complete();
private:
    uint32_t randomState = 0xB0A501u;
    float random();
};
}
