#include "../src/Game.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace boa;

namespace {
constexpr float Frame = 1.f / 60;
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

bool near(float a, float b) {
    return std::abs(a - b) < .002f;
}

void advance(Game& game, float seconds) {
    // 入力のないフレームを進め、端数の時間は最後のフレームで処理する。
    while (seconds > .0001f) {
        float step = std::min(Frame, seconds);
        game.update(step, {});
        seconds -= step;
    }
}

void pressAbsorb(Game& game) {
    Input input;
    input.absorbPress = true;
    game.update(Frame, input);
}

void releaseAbsorb(Game& game) {
    Input input;
    input.absorbRelease = true;
    game.update(Frame, input);
}

void select(Game& game, Action action) {
    Input input;
    input.selection = action;
    game.update(Frame, input);
}

void pressAction(Game& game, Action action) {
    Input input;
    input.selection = action;
    input.actionPress = true;
    game.update(Frame, input);
}

void releaseAction(Game& game) {
    Input input;
    input.actionRelease = true;
    game.update(Frame, input);
}

void pressSkill(Game& game) {
    Input input;
    input.skillPress = true;
    game.update(Frame, input);
}

Bullet bullet(Vec position, float energy = 6, bool rock = false) {
    Bullet b;
    b.p = position;
    b.energy = energy;
    b.rock = rock;
    return b;
}

Shot incomingShot(Vec target, float damage) {
    // 初期位置を標的の手前に置き、次のフレームで衝突させる。
    Shot s;
    s.p = s.previous = target - Vec{15, 0};
    s.kind = ShotKind::Charge;
    s.speed = 1000;
    s.damage = damage;
    return s;
}

const Enemy* findEnemy(const Game& game, int id) {
    auto match = [id](const Enemy& enemy) {
        return enemy.id == id;
    };

    auto enemy = std::find_if(game.enemies.begin(), game.enemies.end(), match);
    return enemy == game.enemies.end() ? nullptr : &*enemy;
}

void absorptionTests() {
    Game game;
    game.start();
    game.energy = 0;

    check(game.inCone(game.player + Vec{200, 0}), "front inside absorption cone");
    check(!game.inCone(game.player + Vec{-1, 0}), "rear excluded from absorption cone");
    check(!game.inCone(game.player + Vec{50, 200}), "side excluded from absorption cone");
    check(!game.inCone(game.player + Vec{game.range() + 1, 0}), "far bullet excluded");

    // 前方の通常弾２発と岩、扇形の外にある通常弾２発を同時に置く。
    game.bullets.push_back(bullet(game.player + Vec{200, 0}, 6));
    game.bullets.push_back(bullet(game.player + Vec{230, 20}, 4));
    game.bullets.push_back(bullet(game.player + Vec{50, 200}, 6));
    game.bullets.push_back(bullet(game.player + Vec{-65, 0}, 6));
    game.bullets.push_back(bullet(game.player + Vec{170, 0}, 6, true));

    pressAbsorb(game);

    check(game.absorbing, "Space starts absorption");
    check(game.bullets.size() == 3 && game.stats.absorbed == 2,
          "one update absorbs all in-range ordinary bullets");
    check(near(game.energy, 10) && near(game.stats.energyGained, 10),
          "full bullet energy available immediately");
    check(game.bullets.back().rock, "rocks remain even inside the active cone");

    game.bullets.push_back(bullet(game.player + Vec{0, 13}));
    advance(game, Frame);
    check(game.hp < game.maxHp(), "side bullet still hits while absorbing");

    // 空き容量が１なら、通常弾は消えるが記録される獲得量と経験値は１になる。
    game.start();
    game.energy = game.capacity() - 1;
    game.bullets.push_back(bullet(game.player + Vec{160, 0}, 6));
    pressAbsorb(game);
    check(game.bullets.empty(), "ordinary bullet is still removed when storage fills");
    check(near(game.energy, game.capacity()), "absorption clamps energy to cap");
    check(near(game.levelXp, 1) && near(game.stats.energyGained, 1),
          "only stored energy counts toward XP and gained energy");

    game.start();
    game.energy = game.capacity();
    game.bullets.push_back(bullet(game.player + Vec{160, 0}));
    pressAbsorb(game);
    check(game.bullets.empty() && near(game.levelXp, 0),
          "absorbing at full capacity is safe but does not yield XP");
}

void cooldownTests() {
    Game shortUse, longUse;
    shortUse.start();
    longUse.start();

    pressAbsorb(shortUse);
    advance(shortUse, .2f);
    float shortDuration = shortUse.absorbTime;
    releaseAbsorb(shortUse);
    check(!shortUse.absorbing && shortUse.absorbCooldown > 0, "Space release starts cooldown");
    check(near(shortUse.absorbCooldownTotal, shortDuration * CooldownRatio),
          "cooldown scales with use time");

    pressAbsorb(longUse);
    advance(longUse, .8f);
    releaseAbsorb(longUse);
    check(longUse.absorbCooldownTotal > shortUse.absorbCooldownTotal,
          "longer absorption produces longer cooldown");

    // クールタイム中に押した入力は破棄され、終了後に押し直すと吸収できる。
    longUse.bullets.push_back(bullet(longUse.player + Vec{200, 0}));
    pressAbsorb(longUse);
    check(!longUse.absorbing && longUse.bullets.size() == 1, "Space cannot absorb during cooldown");

    advance(longUse, longUse.absorbCooldown + .05f);
    check(!longUse.absorbing && near(longUse.absorbCooldown, 0),
          "cooldown completion does not start an old press");

    releaseAbsorb(longUse);
    pressAbsorb(longUse);
    check(longUse.absorbing && longUse.bullets.empty(), "fresh Space press works after cooldown");

    Game limit;
    limit.start();
    pressAbsorb(limit);
    advance(limit, AbsorbDuration);
    check(!limit.absorbing && limit.absorbCooldown > 0,
          "held absorption stops at its duration limit");
    check(near(limit.absorbCooldownTotal, AbsorbDuration * CooldownRatio),
          "timed expiry uses maximum duration for cooldown");

    advance(limit, limit.absorbCooldown + .03f);
    check(!limit.absorbing, "holding Space through expiry does not start another absorption");
}

void chargeTests() {
    Game game;
    game.start();
    float energy = game.energy;

    // 発射に必要な量に達する前に離すと、確保分が戻る。
    pressAction(game, Action::Charge);
    advance(game, .05f);
    check(game.charging && game.charge > 0 && game.shots.empty(),
          "Enter press reserves charge without firing");
    check(near(game.energy + game.charge, energy) && near(game.levelXp, 0),
          "reservation conserves energy and grants no XP");

    releaseAction(game);
    check(!game.charging && game.shots.empty() && near(game.charge, 0),
          "releasing below the minimum does not fire");
    check(near(game.energy, energy) && near(game.levelXp, 0),
          "short charge refunds its full reservation");

    // 短いチャージの弾を基準に、ためた量による威力と速度の変化を比べる。
    pressAction(game, Action::Charge);
    advance(game, .4f);
    float smallCharge = game.charge;
    releaseAction(game);
    check(game.shots.size() == 1 && game.shots.front().kind == ShotKind::Charge,
          "sufficient charge launches on Enter release");
    check(near(game.energy, energy - smallCharge) && near(game.levelXp, smallCharge),
          "released charge counts as actual energy spent");

    float smallDamage = game.shots.front().damage;
    float smallSpeed = game.shots.front().speed;

    game.start();
    pressAction(game, Action::Charge);
    advance(game, 1.25f);
    check(game.charge > smallCharge && game.shots.empty(),
          "continuing to hold increases charge without firing");

    releaseAction(game);
    check(game.shots.size() == 1 && game.shots.front().damage > smallDamage,
          "stronger charge increases damage");
    check(game.shots.front().speed > smallSpeed, "stronger charge increases speed");

    game.start();
    pressAction(game, Action::Charge);
    advance(game, .4f);
    select(game, Action::Grenade);
    check(game.selected == Action::Grenade && !game.charging && near(game.energy, energy),
          "switching action refunds reserved charge");

    releaseAction(game);
    check(game.shots.empty() && near(game.levelXp, 0),
          "release after switching actions spends nothing");

    pressAction(game, Action::Charge);
    advance(game, .4f);
    pressAbsorb(game);
    check(game.absorbing && !game.charging && near(game.energy, energy),
          "starting absorption refunds reserved charge");

    releaseAction(game);
    check(game.shots.empty(), "release after absorption cannot fire the cancelled charge");

    // 同じ射線上の２体を貫通させ、弾を止めて２体目へのダメージが１回に限られることを確かめる。
    Game pierce;
    pierce.start();
    pierce.spawnEnemy(EnemyKind::Scout, {500, 350});
    pierce.spawnEnemy(EnemyKind::Scout, {520, 350});
    Shot shot = incomingShot({470, 350}, 10);
    shot.speed = 1500;
    pierce.shots.push_back(shot);
    advance(pierce, .05f);
    check(pierce.enemies[0].hp < pierce.enemies[0].maxHp &&
          pierce.enemies[1].hp < pierce.enemies[1].maxHp,
          "charge pierces two enemies");

    float remainingHp = pierce.enemies[1].hp;
    pierce.shots.front().speed = 0;
    advance(pierce, Frame);
    check(near(pierce.enemies[1].hp, remainingHp), "piercing projectile hits each enemy only once");
}

void grenadeTests() {
    Game game;
    game.start();
    float energy = game.energy;

    select(game, Action::Grenade);
    check(game.shots.empty() && near(game.energy, energy), "selecting grenade does not execute it");

    // 直撃、射線から外れた爆風圏内、爆風圏外の３体でダメージの範囲を確認する。
    game.spawnEnemy(EnemyKind::Scout, game.player + Vec{143, 0});
    game.spawnEnemy(EnemyKind::Heavy, game.player + Vec{155, 60});
    game.spawnEnemy(EnemyKind::Heavy, game.player + Vec{155, ExplosionRadius + 80});
    int directId = game.enemies[0].id;
    int nearbyId = game.enemies[1].id;
    int distantId = game.enemies[2].id;

    pressAction(game, Action::Grenade);
    check(game.shots.size() == 1 && game.shots.front().kind == ShotKind::Grenade,
          "Enter press fires a grenade");
    check(near(game.energy, energy - GrenadeCost), "grenade consumes its cost once at launch");

    advance(game, .2f);
    const Enemy* direct = findEnemy(game, directId);
    const Enemy* nearby = findEnemy(game, nearbyId);
    const Enemy* distant = findEnemy(game, distantId);
    check(!direct || direct->hp < direct->maxHp, "grenade impact damages direct target");
    check(!nearby || nearby->hp < nearby->maxHp,
          "grenade explosion damages a nearby target off the firing line");
    check(distant && near(distant->hp, distant->maxHp),
          "grenade explosion leaves distant target unharmed");
    check(!game.explosions.empty() && game.shots.empty(),
          "impact replaces grenade with a visible explosion");

    releaseAction(game);
    check(near(game.energy, energy - GrenadeCost),
          "holding or releasing grenade action does not repeat it");

    game.energy = GrenadeCost - 1;
    pressAction(game, Action::Grenade);
    check(game.shots.empty() && near(game.energy, GrenadeCost - 1),
          "grenade cannot launch without sufficient energy");
}

void barrierTests() {
    Game game;
    game.start();
    float energy = game.energy;

    pressAbsorb(game);
    advance(game, .15f);
    game.bullets.push_back(bullet(game.player + Vec{40, 0}, 6, true));

    pressAction(game, Action::Barrier);
    check(game.barrierActive && !game.absorbing && game.absorbCooldownTotal > 0,
          "Enter starts barrier and ends absorption with cooldown");
    check(game.bullets.empty() && near(game.hp, game.maxHp()),
          "barrier blocks non-absorbable rock");

    // バリアの消費時間には、Enter を押したフレームも含まれる。
    advance(game, .25f);
    check(near(game.energy, energy - BarrierRate * (.25f + Frame)),
          "holding barrier drains energy over time");
    check(near(game.levelXp, energy - game.energy),
          "ongoing barrier cost contributes to level XP");

    releaseAction(game);
    check(!game.barrierActive, "releasing Enter closes the barrier");

    pressAction(game, Action::Barrier);
    game.energy = .1f;
    advance(game, 2 * Frame);
    check(!game.barrierActive && game.energy >= 0,
          "barrier cannot remain active after energy is depleted");
}

void skillTests() {
    Game game;
    game.start();
    float energy = game.energy;

    pressSkill(game);
    check(near(game.onParadeTime, 0) && near(game.energy, energy),
          "skill is unavailable before wreck recovery");

    // ガブの吸収中に当てた弾は消え、吸収が終わってからの弾で撃破できる。
    game.spawnEnemy(EnemyKind::Gabu, game.player + Vec{155, 0});
    game.enemies.front().age = 2.f;
    check(game.enemies.front().absorbingShots(), "Gabu has an active absorption window");
    game.shots.push_back(incomingShot(game.enemies.front().p, 400));
    advance(game, Frame);
    check(game.enemies.size() == 1 &&
          near(game.enemies.front().hp, game.enemies.front().maxHp),
          "Gabu intercepts a player shot during absorption");
    check(game.shots.empty(), "Gabu removes the absorbed shot");

    game.enemies.front().age = 3.f;
    check(!game.enemies.front().absorbingShots(), "Gabu leaves a counterattack window");
    game.shots.push_back(incomingShot(game.enemies.front().p, 400));
    advance(game, Frame);
    check(game.enemies.empty() && game.wrecks.size() == 1 && !game.skillUnlocked,
          "Gabu defeat leaves an uncollected skill wreck");

    pressAbsorb(game);
    check(game.wrecks.empty() && game.skillUnlocked, "Space recovers wreck and unlocks the skill");

    // 先に通常のクールタイムを発生させて、発動が残り時間にも効くことを確かめる。
    advance(game, .4f);
    releaseAbsorb(game);
    float baseRange = game.range();
    float baseAngle = game.halfAngle();
    float cooldown = game.absorbCooldown;
    energy = game.energy;

    pressSkill(game);
    check(game.onParadeTime > ParadeDuration - .05f,
          "recovered On Parade skill can be activated");
    check(near(game.energy, energy - ParadeCost), "On Parade spends the configured energy cost");
    check(game.range() > baseRange && game.halfAngle() > baseAngle,
          "On Parade expands the absorption cone");
    check(game.absorbCooldown < cooldown * .6f, "activating On Parade shortens existing cooldown");

    advance(game, game.absorbCooldown + .05f);
    pressAbsorb(game);
    advance(game, .25f);
    float duration = game.absorbTime;
    releaseAbsorb(game);
    check(near(game.absorbCooldownTotal, duration * CooldownRatio * .5f),
          "new cooldowns during On Parade are shorter");

    advance(game, ParadeDuration);
    check(near(game.range(), baseRange) && near(game.halfAngle(), baseAngle),
          "On Parade effect expires");
}

void progressionTests() {
    Game game;
    game.start();
    check(game.level == 1 && near(game.levelXp, 0), "a run begins at level one");
    float startingHp = game.maxHp();
    float startingCap = game.capacity();

    // 経験値をしきい値の直前に置き、獲得と消費のそれぞれでレベルを上げる。
    game.levelXp = game.xpToNextLevel() - 1;
    game.bullets.push_back(bullet(game.player + Vec{160, 0}));
    pressAbsorb(game);
    check(game.level == 2 && near(game.levelXp, 5), "earned energy can level up with leftover XP");
    check(game.capacity() > startingCap && game.maxHp() > startingHp,
          "level-up increases capacity and life");

    game.levelXp = game.xpToNextLevel() - 1;
    pressAction(game, Action::Grenade);
    check(game.level == 3 && near(game.levelXp, GrenadeCost - 1),
          "energy spending can independently level up");

    game.hp = 0;
    advance(game, Frame);
    check(game.phase == Phase::GameOver && game.profile.stage == 1,
          "failure leaves stage progress intact");

    game.start();
    check(game.phase == Phase::Playing && game.level == 1 && near(game.levelXp, 0),
          "retry resets in-stage growth");
    check(near(game.hp, startingHp) && near(game.capacity(), startingCap),
          "retry restores original stats");
}

void stageTests() {
    Game game;
    game.start();

    // モブの弾数と弾の大きさを控え、ほかの敵の攻撃と比較する。
    game.spawnEnemy(EnemyKind::Scout, {850, 300});
    game.enemyFire(game.enemies.front());
    const auto scoutAmount = game.bullets.size();
    check(scoutAmount > 0, "scout supplies absorbable bullets");
    const float scoutRadius = game.bullets.front().radius;

    game.bullets.clear();
    game.spawnEnemy(EnemyKind::Heavy, {850, 300});
    game.enemyFire(game.enemies.back());
    check(game.bullets.size() == scoutAmount * 3,
          "Martinez fires three times as many bullets as scout");
    check(game.bullets.front().radius < scoutRadius,
          "Martinez bullets are smaller than scout bullets");

    game.bullets.clear();
    game.spawnEnemy(EnemyKind::Gabu, {850, 300});
    game.enemyFire(game.enemies.back());
    check(game.bullets.size() == scoutAmount && near(game.bullets.front().radius, scoutRadius),
          "Gabu fires ordinary scout bullets");

    game.bullets.clear();
    game.spawnEnemy(EnemyKind::Rock, {850, 300});
    game.enemyFire(game.enemies.back());
    check(game.bullets.size() == 1 && game.bullets.front().rock,
          "rock enemy fires a non-absorbable projectile");

    game.start();
    game.spawnEnemy(EnemyKind::Boss, {1080, 380});
    game.enemies[0].age = 2;
    game.enemyFire(game.enemies[0]);
    check(!game.bullets.empty(), "boss attack phase supplies bullets");

    game.bullets.clear();
    game.enemies[0].age = 6;
    game.enemyFire(game.enemies[0]);
    check(game.bullets.empty(), "boss counterattack window stops firing");

    game.start();
    bool sawBoss = false;

    // 自機を無敵にし、敵の出現から弾の衝突による撃破とクリアまでを通す。
    for (int frame = 0; frame < 6000 && game.phase == Phase::Playing; ++frame) {
        game.invulnerable = 1;

        for (const auto& enemy : game.enemies) {
            if (!enemy.alive)
                continue;

            game.shots.push_back(incomingShot(enemy.p, 2000));

            if (enemy.kind == EnemyKind::Boss)
                sawBoss = true;
        }

        advance(game, Frame);
    }

    check(sawBoss && game.phase == Phase::Clear,
          "stage waves lead to a boss whose defeat clears the stage");
    check(game.stage == 1 && game.profile.stage == 2,
          "clear keeps the displayed stage and unlocks the next one");

    advance(game, 1.f);
    check(game.profile.stage == 2, "repeating clear does not advance twice");

    game.start();
    check(game.stage == 2, "next launch loads the unlocked stage");

    const Gimmick expected[] = {
        Gimmick::None, Gimmick::Gravity, Gimmick::Terrain, Gimmick::Meteors, Gimmick::Crossfire
    };

    for (int i = 0; i < 5; ++i) {
        game.profile.stage = i + 1;
        game.start();
        check(game.gimmick() == expected[i], "stage selects its corresponding gimmick");
        check(game.activeGimmick() == Gimmick::None,
              "gimmick is inactive before its stage window");

        game.clock = 20;
        check(game.activeGimmick() == expected[i],
              "designated gimmick is active during its stage window");

        game.clock = 44;
        check(game.activeGimmick() == Gimmick::None, "gimmick ends before the boss");
    }
}

void wheelSelectionTests() {
    check(cycleAction(Action::Charge, 1) == Action::Grenade &&
          cycleAction(Action::Grenade, 1) == Action::Barrier &&
          cycleAction(Action::Barrier, 1) == Action::Charge,
          "scrolling forward cycles through the three actions");
    check(cycleAction(Action::Charge, -1) == Action::Barrier,
          "scrolling backward wraps to the last action");
    check(cycleAction(Action::Charge, 5) == Action::Barrier &&
          cycleAction(Action::Barrier, -5) == Action::Charge,
          "multi-step wheel movement preserves direction and detent count");
}

void normalShotTests() {
    Game game;
    game.start();
    game.energy = 0;
    Input fire;
    fire.normalShotHeld = true;

    game.update(Frame, fire);
    check(game.shots.size() == 1 && game.shots.front().kind == ShotKind::Normal &&
          near(game.energy, 0) && near(game.levelXp, 0),
          "normal shot can fire immediately without energy or spending XP");
    for (int i = 0; i < 6; ++i)
        game.update(Frame, fire);
    check(game.shots.size() == 1, "holding fire respects the normal shot cooldown");
    for (int i = 0; i < 17; ++i)
        game.update(Frame, fire);
    check(game.shots.size() == 2, "normal shot repeats while the button is held");
    advance(game, .35f);
    check(game.shots.size() == 2, "releasing the button stops normal autofire");

    game.start();
    // 遠い敵を先に登録し、弾が登録順ではなく手前の敵で止まるかを確認する。
    game.spawnEnemy(EnemyKind::Scout, {250, 380});
    game.spawnEnemy(EnemyKind::Scout, {232, 380});
    game.update(.05f, fire);
    const auto& far = game.enemies.front();
    const auto& nearEnemy = game.enemies.back();
    check(near(far.hp, far.maxHp) && near(nearEnemy.hp, nearEnemy.maxHp - NormalShotDamage) &&
          game.shots.empty() && game.explosions.empty(),
          "normal shot damages only the first enemy and does not explode");
}

void gimmickTests() {
    Game game;

    // 通常の横移動が止まる時間帯の敵を、ブラックホールを挟んで自機の反対側に置く。
    game.profile.stage = 2;
    game.start();
    game.clock = 12;
    game.spawnEnemy(EnemyKind::Scout, {980, 520});
    game.enemies.front().age = 3;
    float enemyX = game.enemies.front().p.x;
    float playerX = game.player.x;

    advance(game, Frame);
    check(game.player.x > playerX && game.enemies.front().p.x < enemyX,
          "gravity moves both player and enemies toward the black hole");

    // 岩の内側から開始し、衝突処理で自機が外側へ押し出されるかを見る。
    game.profile.stage = 3;
    game.start();
    game.clock = 12;
    game.player = game.hazardPosition() + Vec{TerrainRadius - 5, 0};

    advance(game, Frame);
    check(length(game.player - game.hazardPosition()) >= TerrainRadius,
          "terrain rock blocks passage");

    game.profile.stage = 4;
    game.start();
    game.clock = 12;

    advance(game, Frame);
    check(game.bullets.size() == 1 && game.bullets.front().rock &&
          game.bullets.front().velocity.y > 0,
          "meteor shower produces falling non-absorbable rocks");

    // 第三勢力の近くに敵を置いて、発射だけでなく着弾まで確認する。
    game.profile.stage = 5;
    game.start();
    game.clock = 12;
    game.spawnEnemy(EnemyKind::Scout, game.hazardPosition() + Vec{75, 0});
    int id = game.enemies.front().id;

    advance(game, Frame);
    check(game.bullets.size() == 2 && game.bullets[0].crossfire && game.bullets[1].crossfire,
          "third party fires at both player and enemy");

    advance(game, .45f);
    const Enemy* target = findEnemy(game, id);
    check(target && target->hp < target->maxHp, "third-party fire can damage enemies");
}
}

int main() {
    try {
        absorptionTests();
        cooldownTests();
        chargeTests();
        grenadeTests();
        barrierTests();
        skillTests();
        progressionTests();
        stageTests();
        wheelSelectionTests();
        normalShotTests();
        gimmickTests();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "PASS: " << checks << " gameplay checks\n";
    return EXIT_SUCCESS;
}
