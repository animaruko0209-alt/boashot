#include "Game.h"

namespace boa {
namespace {

constexpr float HazardStart = 8.f, HazardEnd = 43.f;
constexpr float ShipRadius = 9.f;

float enemyRadius(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Boss: return 57.f;
    case EnemyKind::Heavy: return 28.f;
    default: return 20.f;
    }
}

// 弾の移動区間で最も近い点を調べ、フレーム間のすり抜けを防ぐ。
bool passesWithin(Vec from, Vec to, Vec target, float radius) {
    Vec delta = to - from;
    float distanceSquared = delta.x * delta.x + delta.y * delta.y;
    float t = 0;

    if (distanceSquared > 0) {
        Vec offset = target - from;
        t = clamp((offset.x * delta.x + offset.y * delta.y) / distanceSquared, 0, 1);
    }

    return length(target - (from + delta * t)) <= radius;
}

// 右へ進む弾が最初に当たるX座標を求める。戻り値が現在の弾より右なら、このフレームでは届いていない。
float shotContact(const Shot& shot, const Enemy& enemy, float radius) {
    float dy = enemy.p.y - shot.p.y;
    float widthSquared = radius * radius - dy * dy;
    if (widthSquared < 0) return shot.p.x + 1;
    float width = std::sqrt(widthSquared);
    if (enemy.p.x + width < shot.previous.x) return shot.p.x + 1;
    return std::max(shot.previous.x, enemy.p.x - width);
}

} // namespace

bool Enemy::absorbingShots() const {
    float cycle = std::fmod(age, 4.4f);
    return kind == EnemyKind::Gabu && cycle >= 1.6f && cycle < 2.8f;
}

float Game::random() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return (randomState & 0xffff) / 65535.f;
}

bool Game::inCone(Vec p) const {
    Vec d = p - player;
    return d.x >= 0 && length(d) <= range() && std::abs(std::atan2(d.y, d.x)) <= halfAngle();
}

Gimmick Game::gimmick() const {
    constexpr Gimmick course[] = {
        Gimmick::None,
        Gimmick::Gravity,
        Gimmick::Terrain,
        Gimmick::Meteors,
        Gimmick::Crossfire
    };
    return course[(stage - 1) % 5];
}

Gimmick Game::activeGimmick() const {
    return clock >= HazardStart && clock < HazardEnd ? gimmick() : Gimmick::None;
}

Vec Game::hazardPosition() const {
    switch (gimmick()) {
    case Gimmick::Gravity:
        return {695, 380};
    case Gimmick::Terrain:
        return {1390 - std::fmod(std::max(0.f, clock - HazardStart) * 105, 1530.f), 380};
    case Gimmick::Crossfire:
        return {730, Top + 70};
    default:
        return {};
    }
}

void Game::start() {
    // 出撃中の成長・弾・スキルは、再挑戦でも初期状態から始める。
    phase = Phase::Playing;
    stage = profile.stage;
    level = 1;
    levelXp = 0;
    selected = Action::Charge;
    player = {170, 380};
    hp = maxHp();
    energy = 35;
    charge = 0;

    clock = invulnerable = shake = flash = 0;
    absorbTime = absorbCooldown = absorbCooldownTotal = onParadeTime = 0;
    clearDelay = -1;
    nextHazard = HazardStart;
    wave = 0;
    nextId = 1;
    randomState = 0xB0A501u;
    absorbing = charging = barrierActive = bossSpawned = skillUnlocked = false;

    bullets.clear();
    shots.clear();
    enemies.clear();
    particles.clear();
    wrecks.clear();
    explosions.clear();
    stats = {};
}

void Game::cancelCharge() {
    energy = std::min(capacity(), energy + charge);
    charge = 0;
    charging = false;
}

void Game::endAbsorb() {
    if (!absorbing) return;

    absorbCooldown = absorbTime * CooldownRatio;
    if (onParadeTime > 0) absorbCooldown *= .5f;

    // 開始時のクールタイムを保持し、HUDでは残り時間と回復割合の両方を表示する。
    absorbCooldownTotal = absorbCooldown;
    absorbing = false;
}

void Game::stopActions() {
    cancelCharge();
    endAbsorb();
    barrierActive = false;
}

void Game::addLevelXp(float amount) {
    if (level >= MaxLevel) return;

    levelXp += amount;

    // 一度の獲得・消費で複数レベル分に達した場合も、余った経験値を次へ回す。
    while (level < MaxLevel && levelXp >= xpToNextLevel()) {
        levelXp -= xpToNextLevel();
        ++level;
        hp = std::min(maxHp(), hp + 8.f);
        burst(player, 3, 12);
    }

    if (level == MaxLevel) levelXp = 0;
}

void Game::gainEnergy(float amount) {
    // チャージの確保分も所持上限に含め、実際に入った分だけを経験値に加える。
    float gained = std::min(amount, std::max(0.f, capacity() - energy - charge));
    energy += gained;
    stats.energyGained += gained;
    addLevelXp(gained);
}

bool Game::spendEnergy(float amount) {
    if (energy < amount) return false;
    energy -= amount;
    addLevelXp(amount);
    return true;
}

void Game::controls(const Input& input) {
    // 選択を切り替えた時点で、長押し中のアクションを解除する。
    if (input.selection && *input.selection != selected) {
        cancelCharge();
        barrierActive = false;
        selected = *input.selection;
    }

    if (input.skillPress && skillUnlocked && onParadeTime <= 0 && spendEnergy(ParadeCost)) {
        onParadeTime = ParadeDuration;
        absorbCooldown *= .5f;
        absorbCooldownTotal *= .5f;
        burst(player, 3, 24);
    }

    if (input.absorbRelease) endAbsorb();

    // 吸収を開始できるフレームだけチャージを返却し、バリアを閉じる。
    if (input.absorbPress && !absorbing && absorbCooldown <= 0) {
        cancelCharge();
        barrierActive = false;
        absorbTime = 0;
        absorbing = true;
    }

    if (input.actionRelease) {
        if (charging) launchCharge();
        barrierActive = false;
    }

    // 同時に押された場合は、実行できるEnterのアクションが吸収に優先する。
    if (!input.actionPress) return;

    switch (selected) {
    case Action::Charge:
        if (energy > 0) {
            endAbsorb();
            charging = true;
        }
        break;
    case Action::Grenade:
        if (spendEnergy(GrenadeCost)) {
            endAbsorb();
            launchGrenade();
        }
        break;
    case Action::Barrier:
        if (energy > 0) {
            endAbsorb();
            barrierActive = true;
        }
        break;
    }
}

void Game::launchCharge() {
    if (charge < MinCharge) {
        cancelCharge();
        return;
    }

    charging = false;
    Shot shot;
    shot.p = shot.previous = player + Vec{25, 0};
    shot.damage = charge * 2.6f * (1 + .08f * (level - 1));
    shot.radius = 8 + std::sqrt(charge) * 1.6f;
    shot.speed = 760 + 4 * charge;
    shots.push_back(shot);

    // 発射が確定した時点で、取り置きしたエネルギーを消費として記録する。
    stats.biggestCharge = std::max(stats.biggestCharge, charge);
    addLevelXp(charge);
    charge = 0;
    burst(shot.p, 1, 18);
    shake = .16f;
}

void Game::launchGrenade() {
    Shot shot;
    shot.kind = Action::Grenade;
    shot.p = shot.previous = player + Vec{25, 0};
    shot.damage = 48.f * (1 + .08f * (level - 1));
    shot.radius = 9;
    shot.speed = 650;
    shots.push_back(shot);
    burst(shot.p, 1, 6);
}

void Game::update(float dt, const Input& input) {
    if (phase != Phase::Playing) return;

    dt = clamp(dt, 0, .05f);
    clock += dt;
    invulnerable = std::max(0.f, invulnerable - dt);
    shake = std::max(0.f, shake - dt);
    flash = std::max(0.f, flash - dt);

    if (clearDelay < 0) {
        absorbCooldown = std::max(0.f, absorbCooldown - dt);
        onParadeTime = std::max(0.f, onParadeTime - dt);

        Vec move = input.move;
        if (length(move) > 1) move = unit(move);
        player = player + move * (310 * dt);

        if (activeGimmick() == Gimmick::Gravity) {
            Vec pull = hazardPosition() - player;
            player = player + pull * (dt * .36f);
        }
        if (activeGimmick() == Gimmick::Terrain) {
            Vec offset = player - hazardPosition();
            if (length(offset) < TerrainRadius + ShipRadius) {
                player = hazardPosition() + unit(offset) * (TerrainRadius + ShipRadius);
            }
        }

        player.x = clamp(player.x, 32, Width - 50);
        player.y = clamp(player.y, Top + 24, Bottom - 24);

        controls(input);

        if (clock >= 1.5f + wave * 4.2f && clock < bossAt() - 4) spawnWave();
        if (!bossSpawned && clock >= bossAt()) {
            spawnEnemy(EnemyKind::Boss, {1150, 370});
            bossSpawned = true;
        }

        // 自機の弾でボスを倒した場合は、その時点で敵の更新を打ち切る。
        updateWeapons(dt);
        if (clearDelay < 0) {
            updateEnemies(dt);
            updateGimmick();
            updateBullets(dt);
            updateWrecks(dt);
        }

        // 吸収判定を済ませたあとで、そのフレームの継続時間を加える。
        if (absorbing) {
            absorbTime = std::min(AbsorbDuration, absorbTime + dt);
            if (AbsorbDuration - absorbTime < .0001f) {
                absorbTime = AbsorbDuration;
                endAbsorb();
            }
        }
    }

    // ボス撃破後も、クリア画面に切り替わるまで演出を進める。
    for (auto& p : particles) {
        p.p = p.p + p.velocity * dt;
        p.life -= dt;
    }
    for (auto& explosion : explosions) explosion.life -= dt;

    // 当たり判定の走査がすべて終わってから、消滅した対象を取り除く。
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& p) { return p.life <= 0; }), particles.end());
    explosions.erase(std::remove_if(explosions.begin(), explosions.end(),
        [](const Explosion& e) { return e.life <= 0; }), explosions.end());
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
        [](const Bullet& b) { return !b.alive; }), bullets.end());
    shots.erase(std::remove_if(shots.begin(), shots.end(),
        [](const Shot& s) { return !s.alive; }), shots.end());
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](const Enemy& e) { return !e.alive; }), enemies.end());
    wrecks.erase(std::remove_if(wrecks.begin(), wrecks.end(),
        [](const Wreck& w) { return !w.alive; }), wrecks.end());

    if (hp <= 0) {
        phase = Phase::GameOver;
        stopActions();
    } else if (clearDelay >= 0) {
        clearDelay -= dt;
        if (clearDelay <= 0) complete();
    }
}

void Game::updateWeapons(float dt) {
    if (charging) {
        float reserve = std::min(energy, 28.f * dt);
        energy -= reserve;
        charge += reserve;
    }

    if (barrierActive && !spendEnergy(BarrierRate * dt)) barrierActive = false;

    for (auto& shot : shots) {
        shot.previous = shot.p;

        if (shot.kind == Action::Grenade) {
            float travel = std::min(shot.remainingRange, shot.speed * dt);
            shot.p.x += travel;
            shot.remainingRange -= travel;

            // グレネードは進行方向で最初に触れた敵か、射程の端で爆発する。
            float impactX = shot.p.x + 1;
            for (const auto& enemy : enemies) {
                if (!enemy.alive) continue;
                impactX = std::min(impactX, shotContact(shot, enemy, enemyRadius(enemy.kind) + shot.radius));
            }

            if (impactX <= shot.p.x || shot.remainingRange <= 0 || shot.p.x >= Width) {
                shot.p.x = std::min({impactX, shot.p.x, Width});
                shot.alive = false;
                explode(shot.p, shot.damage);
            }
        } else {
            shot.p.x += shot.speed * dt;

            // 先にガブの吸収位置を求め、そこまでに接触した敵だけに貫通ダメージを与える。
            float blockedAt = shot.p.x + 1;
            for (const auto& enemy : enemies) {
                if (!enemy.alive || !enemy.absorbingShots()) continue;
                blockedAt = std::min(blockedAt,
                    shotContact(shot, enemy, enemyRadius(enemy.kind) + shot.radius + 19));
            }

            for (auto& enemy : enemies) {
                if (!enemy.alive ||
                    std::find(shot.hit.begin(), shot.hit.end(), enemy.id) != shot.hit.end())
                    continue;

                float contact = shotContact(shot, enemy, enemyRadius(enemy.kind) + shot.radius);
                if (contact > shot.p.x || contact >= blockedAt) continue;

                shot.hit.push_back(enemy.id);
                damageEnemy(enemy, shot.damage, 1);
            }

            if (blockedAt <= shot.p.x) {
                shot.alive = false;
                burst({blockedAt, shot.p.y}, 3, 12);
            }

            if (shot.p.x > Width + 120) shot.alive = false;
        }

        if (clearDelay >= 0) break;
    }
}

void Game::explode(Vec p, float damage) {
    explosions.push_back({p});
    burst(p, 2, 34);
    shake = .2f;
    for (auto& enemy : enemies) {
        if (enemy.alive && length(enemy.p - p) <= ExplosionRadius + enemyRadius(enemy.kind)) {
            damageEnemy(enemy, damage, 2);
        }
    }
}

void Game::updateBullets(float dt) {
    for (auto& bullet : bullets) {
        if (!bullet.alive) continue;

        Vec previous = bullet.p;
        bullet.p = bullet.p + bullet.velocity * dt;

        // 吸収とバリアを先に判定し、残った弾だけを機体への接触判定へ回す。
        if (absorbing && !bullet.rock && (inCone(previous) || inCone(bullet.p))) {
            gainEnergy(bullet.energy);
            bullet.alive = false;
            ++stats.absorbed;
            burst(bullet.p, 0, 5);
            continue;
        }

        if (barrierActive && passesWithin(previous, bullet.p, player, 54 + bullet.radius)) {
            bullet.alive = false;
            burst(bullet.p, 3, 6);
            continue;
        }

        if (passesWithin(previous, bullet.p, player, ShipRadius + bullet.radius)) {
            hurt(bullet.damage);
            bullet.alive = false;
            continue;
        }

        if (bullet.crossfire) {
            for (auto& enemy : enemies) {
                if (!enemy.alive ||
                    !passesWithin(previous, bullet.p, enemy.p, enemyRadius(enemy.kind) + bullet.radius))
                    continue;

                damageEnemy(enemy, 18, 2);
                bullet.alive = false;
                break;
            }
        }

        if (bullet.p.x < -70 || bullet.p.x > Width + 180 || bullet.p.y < Top - 100 || bullet.p.y > Bottom + 100) {
            bullet.alive = false;
        }
    }
}

void Game::updateWrecks(float dt) {
    for (auto& wreck : wrecks) {
        Vec previous = wreck.p;
        wreck.p.x -= 125.f * dt;

        if (absorbing && (inCone(previous) || inCone(wreck.p))) {
            skillUnlocked = true;
            wreck.alive = false;
            burst(wreck.p, 3, 20);
        } else if (wreck.p.x < -70) {
            wreck.alive = false;
        }
    }
}

void Game::spawnEnemy(EnemyKind kind, Vec position) {
    Enemy enemy;
    enemy.id = nextId++;
    enemy.kind = kind;
    enemy.p = position;
    enemy.anchorY = position.y;

    switch (kind) {
    case EnemyKind::Boss:
        enemy.hp = 460;
        break;
    case EnemyKind::Heavy:
        enemy.hp = 100;
        break;
    case EnemyKind::Rock:
        enemy.hp = 35;
        break;
    case EnemyKind::Gabu:
        enemy.hp = 44;
        break;
    default:
        enemy.hp = 22;
        break;
    }

    enemy.hp *= 1 + .2f * std::min(9, stage - 1);
    enemy.maxHp = enemy.hp;
    enemies.push_back(enemy);
}

void Game::spawnWave() {
    if (wave % 4 == 2) {
        spawnEnemy(EnemyKind::Heavy, {1220, 250 + (wave % 3) * 100.f});
        spawnEnemy(EnemyKind::Scout, {1310, 530});
    } else if (wave % 4 == 3) {
        spawnEnemy(EnemyKind::Rock, {1220, 220});
        spawnEnemy(EnemyKind::Gabu, {1260, 400});
        spawnEnemy(EnemyKind::Scout, {1310, 530});
    } else {
        for (int i = 0; i < 4; ++i) {
            spawnEnemy(EnemyKind::Scout, {1220 + i * 55.f, 205 + i * 105.f});
        }
    }
    ++wave;
}

void Game::enemyFire(Enemy& enemy) {
    if (enemy.kind == EnemyKind::Boss) {
        // 弾幕の合間に射撃を休み、プレイヤーの反撃時間を作る。
        if (std::fmod(enemy.age, 7.5f) > 4.6f) return;

        for (int i = 0; i < 17; ++i) {
            float angle = Pi + (i - 8) * .072f + std::sin(enemy.age * 2) * .16f;
            Bullet bullet;
            bullet.p = enemy.p + Vec{-55, 0};
            bullet.velocity = {std::cos(angle) * 185, std::sin(angle) * 185};
            bullets.push_back(bullet);
        }

        if (static_cast<int>(enemy.age * 2) % 3 == 0) {
            Bullet rock;
            rock.p = enemy.p + Vec{-45, 45};
            rock.rock = true;
            rock.radius = 13;
            rock.velocity = unit(player - rock.p) * 165;
            rock.damage = 22;
            bullets.push_back(rock);
        }

        return;
    }

    bool heavy = enemy.kind == EnemyKind::Heavy;
    bool rock = enemy.kind == EnemyKind::Rock;
    int amount = heavy ? 9 : rock ? 1 : 3;
    float aim = std::atan2(player.y - enemy.p.y, player.x - enemy.p.x);

    for (int i = 0; i < amount; ++i) {
        float angle = aim + (i - (amount - 1) * .5f) * (heavy ? .09f : .15f);
        Bullet bullet;
        bullet.p = enemy.p + Vec{-18, 0};
        bullet.velocity = {std::cos(angle) * 195, std::sin(angle) * 195};
        bullet.rock = rock;

        if (rock) {
            bullet.radius = 13;
            bullet.damage = 22;
        } else if (heavy) {
            bullet.radius = 5;
            bullet.energy = 4;
            bullet.damage = 11;
        }

        bullets.push_back(bullet);
    }
}

void Game::updateEnemies(float dt) {
    auto hazard = activeGimmick();
    Vec center = hazardPosition();

    for (auto& enemy : enemies) {
        if (!enemy.alive) continue;

        enemy.age += dt;
        bool boss = enemy.kind == EnemyKind::Boss;

        if (boss) {
            enemy.p.x = std::max(1080.f, enemy.p.x - 85 * dt);
            enemy.p.y = 380 + std::sin(enemy.age * .7f) * 140;
            if (enemy.age >= enemy.nextShot) {
                enemyFire(enemy);
                enemy.nextShot = enemy.age + .65f;
            }
        } else {
            // 登場と退場だけ大きく横移動し、その間は前方で攻撃を続ける。
            if (enemy.age < 1.25f) enemy.p.x -= 190 * dt;
            else if (enemy.age > 7.6f) enemy.p.x -= 260 * dt;

            if (hazard == Gimmick::Gravity && enemy.age <= 7.6f) {
                enemy.p.x += (center.x - enemy.p.x) * dt * .24f;
                enemy.anchorY += (center.y - enemy.anchorY) * dt * .24f;
            }

            enemy.p.y = enemy.anchorY + std::sin(enemy.age * 1.7f + enemy.id) * 20;
            if (hazard == Gimmick::Terrain) {
                Vec offset = enemy.p - center;
                float spacing = TerrainRadius + enemyRadius(enemy.kind);
                if (length(offset) < spacing) enemy.p = center + unit(offset) * spacing;
            }

            if (enemy.age >= enemy.nextShot && enemy.age < 7.2f) {
                enemyFire(enemy);
                enemy.nextShot += enemy.kind == EnemyKind::Heavy ? 1.35f : 2.f;
            }
        }

        if (length(enemy.p - player) < (boss ? 65.f : 27.f) && !barrierActive) hurt(22);
        if (enemy.p.x < -80) enemy.alive = false;
    }
}

void Game::updateGimmick() {
    if (clock < nextHazard) return;

    switch (activeGimmick()) {
    case Gimmick::Meteors: {
        Bullet meteor;
        meteor.p = {100 + random() * 1240, Top - 26};
        meteor.velocity = {-80, 280};
        meteor.rock = true;
        meteor.radius = 15;
        meteor.damage = 22;
        bullets.push_back(meteor);
        nextHazard = clock + .48f;
        break;
    }
    case Gimmick::Crossfire: {
        Vec origin = hazardPosition();
        Bullet bullet;
        bullet.p = origin;
        bullet.velocity = unit(player - origin) * 175;
        bullet.crossfire = true;
        bullet.energy = 5;
        bullets.push_back(bullet);

        // 自機を狙う弾に加え、砲台から最も近い敵にも一発撃つ。
        const Enemy* target = nullptr;
        float nearest = Width * 2;
        for (const auto& enemy : enemies) {
            if (!enemy.alive) continue;
            float distance = length(enemy.p - origin);
            if (distance >= nearest) continue;
            target = &enemy;
            nearest = distance;
        }

        if (target) {
            bullet.velocity = unit(target->p - origin) * 175;
            bullets.push_back(bullet);
        }

        nextHazard = clock + 1.15f;
        break;
    }
    default:
        break;
    }
}

void Game::damageEnemy(Enemy& enemy, float damage, int effect) {
    enemy.hp -= damage;
    burst(enemy.p, effect, 9);
    if (enemy.hp > 0) return;

    enemy.alive = false;
    ++stats.kills;
    bool boss = enemy.kind == EnemyKind::Boss;
    burst(enemy.p, 2, boss ? 100 : 24);
    shake = boss ? .8f : .12f;
    if (enemy.kind == EnemyKind::Gabu && !skillUnlocked) wrecks.push_back({enemy.p});

    if (!boss) return;

    clearDelay = 1.6f;
    // ボス撃破で戦闘を終了する。走査中の参照を保つため、ここでは消滅フラグだけを立てる。
    for (auto& bullet : bullets) bullet.alive = false;
    for (auto& other : enemies) other.alive = false;
    for (auto& shot : shots) shot.alive = false;
    for (auto& wreck : wrecks) wreck.alive = false;
    stopActions();
}

void Game::hurt(float damage) {
    if (invulnerable > 0 || clearDelay >= 0 || damage <= 0) return;

    hp = std::max(0.f, hp - damage);
    invulnerable = .7f;
    flash = .12f;
    shake = .25f;
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
    phase = Phase::Clear;
    stopActions();
    profile.stage = std::min(99999, stage + 1);
}

} // namespace boa
