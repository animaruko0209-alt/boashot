#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace boa {

// 座標と半径はピクセル、時間とクールタイムは秒で扱う。
constexpr float Width = 1280, Height = 720, Top = 110, Bottom = 652;
constexpr float Pi = 3.14159265f;

constexpr float AbsorbDuration = 2.4f, CooldownRatio = .9f;
// チャージ・グレネード・バリアの消費量
constexpr float MinCharge = 8.f, GrenadeCost = 50.f, BarrierRate = 14.f;
constexpr float NormalShotInterval = .28f, NormalShotDamage = 4.f;
constexpr float ExplosionRadius = 110.f, ExplosionDuration = .42f;
constexpr float ParadeCost = 20.f, ParadeDuration = 6.f, TerrainRadius = 94.f;
constexpr int MaxLevel = 10;

struct Vec {
    float x = 0, y = 0;

    Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    Vec operator*(float s) const { return {x * s, y * s}; }
};

inline float length(Vec v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline Vec unit(Vec v) {
    float d = length(v);
    return d > .001f ? v * (1 / d) : Vec{-1, 0};
}

inline float clamp(float v, float a, float b) { return std::max(a, std::min(v, b)); }

enum class Phase {
    Title,
    Playing,
    Paused,
    Clear,
    GameOver
};

enum class Action {
    Charge,
    Grenade,
    Barrier
};

// ホイールの選択順と、HUDに並べる順番をそろえる。
inline constexpr std::array ActionOrder{Action::Charge, Action::Grenade, Action::Barrier};
Action cycleAction(Action selected, int steps);

enum class ShotKind {
    Charge,
    Grenade,
    Normal
};

enum class EnemyKind {
    Scout,
    Heavy,
    Rock,
    Gabu,
    Boss
};

enum class Gimmick {
    None,
    Gravity,
    Terrain,
    Meteors,
    Crossfire
};

struct Profile {
    // 次に出撃するステージ。描画には出撃時に確定した Game::stage を使う。
    int stage = 1;
};

struct Input {
    Vec move;
    std::optional<Action> selection;

    // キーを押したフレームと離したフレームだけ、それぞれのフラグを立てる。
    bool absorbPress = false, absorbRelease = false;
    bool actionPress = false, actionRelease = false, skillPress = false;
    bool normalShotHeld = false;
};

struct Bullet {
    Vec p, velocity;
    bool rock = false, alive = true, crossfire = false;
    // 吸収量
    float energy = 1.5f, damage = 14, radius = 7;
};

struct Shot {
    Vec p, previous;
    float damage = 0, radius = 4, speed = 850, remainingRange = 800;
    ShotKind kind = ShotKind::Charge;
    bool alive = true;

    // 貫通弾が同じ敵に次のフレームでも重なった場合の二重ヒットを防ぐ。
    std::vector<int> hit;
};

struct Enemy {
    int id = 0;
    EnemyKind kind = EnemyKind::Scout;
    Vec p;
    float hp = 20, maxHp = 20, age = 0, nextShot = 1.25f, anchorY = 0;
    bool alive = true;

    bool absorbingShots() const;
};

struct Wreck {
    Vec p;
    bool alive = true;
};

struct Explosion {
    Vec p;
    float life = ExplosionDuration;
};

struct Particle {
    Vec p, velocity;
    float life = 0, fullLife = 1;
    int kind = 0;
};

struct Stats {
    int kills = 0, absorbed = 0;
    float energyGained = 0, biggestCharge = 0;
};

// 1回の出撃とステージ進行を管理する。入力を受け取り、DxLib に依存せずに戦闘を進める。
class Game {
public:
    Profile profile;
    Phase phase = Phase::Title;
    Action selected = Action::Charge;

    // 自機の状態と、戦闘・被弾・ステージクリアの経過時間。
    Vec player{170, 380};
    float hp = 100, energy = 35, charge = 0, clock = 0, invulnerable = 0;
    float shake = 0, flash = 0, clearDelay = -1;

    // アクションの継続時間と、描画にも使う戦闘中のフラグ。
    float absorbTime = 0, absorbCooldown = 0, absorbCooldownTotal = 0, onParadeTime = 0;
    bool absorbing = false, charging = false, barrierActive = false,
        bossSpawned = false, skillUnlocked = false;

    int wave = 0, nextId = 1, stage = 1, level = 1;
    float levelXp = 0;
    Stats stats;

    // 戦闘中の物体とエフェクト。描画は更新が終わった時点の内容を参照する。
    std::vector<Bullet> bullets;
    std::vector<Shot> shots;
    std::vector<Enemy> enemies;
    std::vector<Particle> particles;
    std::vector<Wreck> wrecks;
    std::vector<Explosion> explosions;

    float capacity() const { return 120.f + (level - 1) * 20.f; }
    float maxHp() const { return 100.f + (level - 1) * 8.f; }
    float xpToNextLevel() const { return level < MaxLevel ? 45.f + (level - 1) * 20.f : 0.f; }

    float range() const { return onParadeTime > 0 ? 420.f : 280.f; }
    float halfAngle() const { return onParadeTime > 0 ? .95f : .65f; }

    float bossAt() const { return 47.f; }
    Gimmick gimmick() const;
    Gimmick activeGimmick() const;
    Vec hazardPosition() const;

    bool inCone(Vec p) const;

    void start();
    void stopActions();
    void update(float dt, const Input& input);
    void spawnEnemy(EnemyKind kind, Vec position);
    void enemyFire(Enemy& enemy);

private:
    uint32_t randomState = 0xB0A501u;
    float nextHazard = 0;
    float normalShotCooldown = 0;

    float random();
    void cancelCharge();
    void endAbsorb();
    void addLevelXp(float amount);
    void gainEnergy(float amount);
    bool spendEnergy(float amount);

    void updateCombat(float dt, const Input& input);
    void updateEffects(float dt);
    void removeExpiredObjects();

    void controls(const Input& input);
    void launchNormalShot();
    void launchCharge();
    void launchGrenade();
    void updateBullets(float dt);
    void updateWeapons(float dt);
    void updateEnemies(float dt);
    void updateWrecks(float dt);
    void updateGimmick();
    void spawnWave();

    void explode(Vec p, float damage);
    void damageEnemy(Enemy& enemy, float damage, int effect);
    void burst(Vec p, int kind, int count);
    void hurt(float damage);
    void complete();
};

} // namespace boa
