#include "Game.h"

namespace boa {
float Game::random() {
    randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
    return (randomState & 0xffff) / 65535.f;
}
bool Game::inCone(Vec p) const {
    Vec d = p - player;
    return d.x >= 0 && length(d) <= range() && std::abs(std::atan2(d.y, d.x)) <= halfAngle();
}
void Game::start() {
    phase = Phase::Playing;
    player = {170, 380}; hp = maxHp(); energy = 35; charge = 0;
    clock = fireClock = invulnerable = shake = flash = 0;
    clearDelay = -1; wave = 0; nextId = 1; bossSpawned = false;
    absorbing = firing = charging = barrierActive = false;
    bullets.clear(); shots.clear(); enemies.clear(); particles.clear(); stats = {};
}
void Game::cancelCharge() {
    energy = std::min(capacity(), energy + charge);
    charge = 0;
}
void Game::stopActions() {
    cancelCharge(); absorbing = firing = charging = barrierActive = false;
}
void Game::setToggle(bool toggle) { stopActions(); profile.toggleAbsorb = toggle; }
bool Game::evolve(Branch branch) {
    if (profile.branch != Branch::Base || branch == Branch::Base || profile.xp < 100) return false;
    profile.xp -= 100; profile.branch = branch; profile.rank = 1;
    return true;
}
bool Game::upgrade() {
    if (profile.branch == Branch::Base || profile.rank >= 3) return false;
    int cost = 100 * (profile.rank + 1);
    if (profile.xp < cost) return false;
    profile.xp -= cost; ++profile.rank; return true;
}
void Game::controls(const Input& input) {
    // Press edges determine priority. Releasing a displaced action never resumes it.
    if (input.absorbPress) {
        bool enable = profile.toggleAbsorb ? !absorbing : true;
        if (enable) { cancelCharge(); firing = charging = false; }
        absorbing = enable;
    }
    if (input.absorbRelease && !profile.toggleAbsorb) absorbing = false;
    if (input.firePress) {
        cancelCharge(); absorbing = charging = false; firing = true; fireClock = 0;
    }
    if (input.fireRelease) firing = false;
    if (input.chargePress) { absorbing = firing = false; charging = true; }
    if (input.chargeRelease && charging) {
        charging = false;
        if (charge >= 8) {
            Shot s;
            s.p = s.previous = player + Vec{25, 0}; s.charged = true;
            s.damage = charge * 2.6f * chargeMultiplier() * (1 + .08f * profile.rank);
            s.radius = 8 + std::sqrt(charge) * 1.6f; s.speed = 1050;
            shots.push_back(s);
            stats.biggestCharge = std::max(stats.biggestCharge, charge);
            burst(s.p, 1, 18); shake = .16f;
            charge = 0;
        } else { cancelCharge(); }
    }
}
void Game::update(float dt, const Input& input) {
    if (phase != Phase::Playing) return;
    dt = clamp(dt, 0, .05f);
    clock += dt;
    invulnerable = std::max(0.f, invulnerable - dt);
    shake = std::max(0.f, shake - dt); flash = std::max(0.f, flash - dt);
    controls(input);
    Vec move = input.move;
    if (length(move) > 1) move = unit(move);
    player = player + move * (310 * dt);
    player.x = clamp(player.x, 32, Width - 50); player.y = clamp(player.y, Top + 24, Bottom - 24);
    barrierActive = input.barrier && profile.branch == Branch::Defense && energy >= 14 * dt;
    if (barrierActive) {
        cancelCharge(); absorbing = firing = charging = false; energy = std::max(0.f, energy - 14 * dt);
    }
    if (clock >= 1.5f + wave * 4.2f && clock < bossAt() - 4) spawnWave();
    if (!bossSpawned && clock >= bossAt()) {
        spawnEnemy(EnemyKind::Boss, {1150, 370}); bossSpawned = true;
    }
    updateWeapons(dt);
    updateEnemies(dt);
    updateBullets(dt);
    for (auto& p : particles) { p.p = p.p + p.velocity * dt; p.life -= dt; }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) {return p.life <= 0;}), particles.end());
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) {return !b.alive;}), bullets.end());
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Shot& s) {return !s.alive;}), shots.end());
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) {return !e.alive;}), enemies.end());
    if (hp <= 0) { phase = Phase::GameOver; stopActions(); return; }
    if (clearDelay >= 0) { clearDelay -= dt; if (clearDelay <= 0) complete(); }
}
void Game::updateWeapons(float dt) {
    if (charging && !barrierActive) {
        // Space reserves charge without firing; release launches the charged shot.
        float reserve = std::min(energy, 28.f * dt);
        energy -= reserve; charge += reserve;
    }
    if (firing && !barrierActive) {
        fireClock -= dt;
        if (fireClock <= 0) {
            fireClock += .12f;
            float cost = profile.branch == Branch::Attack ? 1.6f : 1.2f;
            if (energy >= cost) {
                energy -= cost;
                Shot s; s.p = s.previous = player + Vec{25, 0};
                s.damage = (profile.branch == Branch::Attack ? 7.f : profile.branch == Branch::Absorb ? 4.3f : 5.5f) * (1 + .08f * profile.rank);
                shots.push_back(s);
            }
        }
    }
    for (auto& s : shots) {
        s.previous = s.p; s.p.x += s.speed * dt;
        for (auto& e : enemies) {
            if (!e.alive || std::find(s.hit.begin(), s.hit.end(), e.id) != s.hit.end()) continue;
            float radius = e.kind == EnemyKind::Boss ? 57.f : e.kind == EnemyKind::Heavy ? 28.f : 20.f;
            float nearestX = clamp(e.p.x, s.previous.x, s.p.x);
            if (length(e.p - Vec{nearestX, s.p.y}) > radius + s.radius) continue;
            e.hp -= s.damage; s.hit.push_back(e.id);
            burst(e.p, s.charged ? 1 : 0, s.charged ? 12 : 3);
            if (!s.charged) s.alive = false;
            if (e.hp <= 0) {
                e.alive = false; ++stats.kills; burst(e.p, 2, e.kind == EnemyKind::Boss ? 100 : 24);
                shake = e.kind == EnemyKind::Boss ? .8f : .12f;
                if (e.kind == EnemyKind::Boss) {
                    clearDelay = 1.6f;
                    // Boss defeat ends combat. Ordinary enemy defeat never deletes its bullets.
                    bullets.clear();
                    for (auto& other : enemies) other.alive = false;
                    stopActions();
                }
            }
            if (!s.alive) break;
        }
        if (s.p.x > Width + 120) s.alive = false;
    }
}
void Game::updateBullets(float dt) {
    int count = 0;
    for (const auto& b : bullets) if (b.alive && !b.rock && absorbing && inCone(b.p)) ++count;
    float speed = std::max(65.f, 420.f / (1 + .17f * std::max(0, count - 1)));
    if (profile.branch == Branch::Absorb) speed *= 1.35f;
    for (auto& b : bullets) {
        if (!b.alive) continue;
        b.siphoning = absorbing && !b.rock && inCone(b.p);
        if (b.siphoning) {
            // Per-bullet siphon rate is independent of the number of bullets.
            // No energy is removed if both the free and reserved stores are full.
            float take = std::min({b.energy, 11.f * dt, std::max(0.f, capacity() - energy - charge)});
            b.energy -= take; energy += take; stats.energyGained += take;
            float distance = length(player - b.p);
            if (distance <= 17 + speed * dt) {
                float remaining = std::min(b.energy, std::max(0.f, capacity() - energy - charge));
                energy += remaining; stats.energyGained += remaining;
                b.alive = false; ++stats.absorbed; burst(b.p, 0, 5);
                continue;
            }
            b.p = b.p + unit(player - b.p) * (speed * dt);
        } else {
            // Original velocity is never overwritten by attraction.
            b.p = b.p + b.velocity * dt;
        }
        float d = length(b.p - player);
        if (barrierActive && d < 54 + b.radius) {
            b.alive = false; burst(b.p, 3, 6); continue;
        }
        if (d < 9 + b.radius) {
            float damage = b.rock ? b.damage : b.damage * b.energy / b.maxEnergy;
            if (damage > .01f) { hurt(damage); b.alive = false; }
            // Empty shells remain harmless until collected or off-screen.
        }
        if (b.p.x < -70 || b.p.x > Width + 180 || b.p.y < Top - 100 || b.p.y > Bottom + 100) b.alive = false;
    }
}
void Game::spawnEnemy(EnemyKind kind, Vec position) {
    Enemy e; e.id = nextId++; e.kind = kind; e.p = position; e.anchorY = position.y;
    float scale = 1 + .2f * std::min(9, profile.stage - 1);
    e.hp = e.maxHp = (kind == EnemyKind::Boss ? 850.f : kind == EnemyKind::Heavy ? 100.f : kind == EnemyKind::Rock ? 35.f : 22.f) * scale;
    enemies.push_back(e);
}
void Game::spawnWave() {
    // Brief entrance + telegraph; staggered formations overlap the previous ones.
    if (wave % 4 == 2) {
        spawnEnemy(EnemyKind::Heavy, {1220, 250 + (wave % 3) * 100.f});
        spawnEnemy(EnemyKind::Scout, {1310, 530});
    } else if (wave % 4 == 3) {
        spawnEnemy(EnemyKind::Rock, {1220, 220});
        spawnEnemy(EnemyKind::Scout, {1260, 400});
        spawnEnemy(EnemyKind::Scout, {1310, 530});
    } else {
        for (int i = 0; i < 4; ++i)
            spawnEnemy(EnemyKind::Scout, {1220 + i * 55.f, 205 + i * 105.f});
    }
    ++wave;
}
void Game::enemyFire(Enemy& e) {
    if (e.kind == EnemyKind::Boss) {
        float t = std::fmod(e.age, 7.5f);
        if (t > 4.6f) return; // A visible counterattack window, without invulnerability.
        for (int i = 0; i < 17; ++i) {
            float angle = Pi + (i - 8) * .072f + std::sin(e.age * 2) * .16f;
            Bullet b; b.p = e.p + Vec{-55, 0}; b.velocity = {std::cos(angle) * 185, std::sin(angle) * 185};
            bullets.push_back(b);
        }
        if (static_cast<int>(e.age * 2) % 3 == 0) {
            Bullet rock; rock.p = e.p + Vec{-45, 45}; rock.rock = true; rock.radius = 13;
            rock.velocity = unit(player - rock.p) * 165; rock.damage = 22; bullets.push_back(rock);
        }
        return;
    }
    int amount = e.kind == EnemyKind::Heavy ? 13 : e.kind == EnemyKind::Rock ? 1 : 3;
    float aim = std::atan2(player.y - e.p.y, player.x - e.p.x);
    for (int i = 0; i < amount; ++i) {
        float angle = aim + (i - (amount - 1) * .5f) * (e.kind == EnemyKind::Heavy ? .075f : .15f);
        Bullet b; b.p = e.p + Vec{-18, 0};
        b.velocity = {std::cos(angle) * 195, std::sin(angle) * 195};
        b.rock = e.kind == EnemyKind::Rock;
        if (b.rock) { b.radius = 13; b.damage = 22; }
        bullets.push_back(b);
    }
}
void Game::updateEnemies(float dt) {
    for (auto& e : enemies) {
        if (!e.alive) continue;
        e.age += dt;
        if (e.kind == EnemyKind::Boss) {
            e.p.x = std::max(1080.f, e.p.x - 85 * dt);
            e.p.y = 380 + std::sin(e.age * .7f) * 140;
            if (e.age >= e.nextShot) { enemyFire(e); e.nextShot = e.age + .65f; }
        } else {
            if (e.age < 1.25f) e.p.x -= 190 * dt;
            else if (e.age > 7.6f) e.p.x -= 260 * dt;
            e.p.y = e.anchorY + std::sin(e.age * 1.7f + e.id) * 20;
            if (e.age >= e.nextShot && e.age < 7.2f) {
                enemyFire(e);
                e.nextShot += e.kind == EnemyKind::Heavy ? 1.35f : 2.f;
            }
        }
        if (length(e.p - player) < (e.kind == EnemyKind::Boss ? 65.f : 27.f)) {
            if (!barrierActive) hurt(22);
        }
        if (e.p.x < -80) e.alive = false;
    }
}
void Game::hurt(float damage) {
    if (invulnerable > 0 || clearDelay >= 0 || damage <= 0) return;
    hp = std::max(0.f, hp - damage); invulnerable = .7f; flash = .12f; shake = .25f;
    burst(player, 2, 18);
}
void Game::burst(Vec p, int kind, int count) {
    for (int i = 0; i < count && particles.size() < 1600; ++i) {
        float angle = random() * 2 * Pi, speed = 40 + random() * 210;
        float life = .2f + random() * .5f;
        particles.push_back({p, {std::cos(angle) * speed, std::sin(angle) * speed}, life, life, kind});
    }
}
void Game::complete() {
    if (phase != Phase::Playing) return;
    phase = Phase::Clear; stopActions(); profile.xp += 100; ++profile.stage;
}
}
