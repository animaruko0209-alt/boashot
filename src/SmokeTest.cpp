#include "Application.h"

#include "DxLib.h"

namespace boa {
namespace {

bool smokeCapture(const Renderer& renderer, const Game& g, const wchar_t* file) {
    if (ProcessMessage() != 0)
        return false;

    renderer.drawScene(g, 0.f, false);
    const bool saved = SaveDrawScreenToPNG(0, 0, 1280, 720, file) >= 0;
    ScreenFlip();
    return saved;
}

} // namespace

// 各場面の開始状態を固定し、1/60秒刻みで入力を再現して画面を保存する。
bool Application::runSmoke() {
    if (!smokeCapture(renderer, game, L"smoke-title.png"))
        return false;

    game.start();
    game.clock = 10.f;
    game.spawnEnemy(EnemyKind::Gabu, {660, 400});
    game.wrecks.push_back({game.player + Vec{90, 0}});
    for (int i = 0; i < 12; ++i) {
        Bullet bullet;
        bullet.p = game.player + Vec{120.f + 30.f * i, (i % 3 - 1) * 35.f};
        bullet.velocity = {-130, 0};
        game.bullets.push_back(bullet);
    }

    // DxLibのボタン状態を模擬し、通常プレイと同じ入力変換を通す。
    game.update(1.f / 60.f, readCombatInput(MOUSE_INPUT_RIGHT | MOUSE_INPUT_LEFT, 0));
    if (!game.absorbing || game.stats.absorbed == 0 ||
        game.shots.empty() || game.shots.front().kind != ShotKind::Normal ||
        !smokeCapture(renderer, game, L"smoke-absorb.png"))
        return false;

    // 吸収開始の画面に続けて、残り半分と終了直前の色を記録する。
    for (int i = 0; i < 71; ++i)
        game.update(1.f / 60.f, {});
    if (!game.absorbing || !smokeCapture(renderer, game, L"smoke-absorb-mid.png"))
        return false;
    for (int i = 0; i < 60; ++i)
        game.update(1.f / 60.f, {});
    if (!game.absorbing || !smokeCapture(renderer, game, L"smoke-absorb-low.png"))
        return false;

    previousMouseButtons = MOUSE_INPUT_RIGHT;
    game.update(1.f / 60.f, readCombatInput(0, 0));
    previousMouseButtons = 0;
    if (game.absorbCooldown <= 0 || !smokeCapture(renderer, game, L"smoke-cooldown.png"))
        return false;

    keys[KEY_INPUT_SPACE] = 1;
    game.update(1.f / 60.f, readCombatInput(0, 0));
    for (int i = 0; i < 47; ++i)
        game.update(1.f / 60.f, {});
    if (game.charge < MinCharge || !smokeCapture(renderer, game, L"smoke-charge.png"))
        return false;

    previousKeys[KEY_INPUT_SPACE] = 1;
    keys[KEY_INPUT_SPACE] = 0;
    game.update(1.f / 60.f, readCombatInput(0, 0));
    previousKeys[KEY_INPUT_SPACE] = 0;
    if (std::none_of(game.shots.begin(), game.shots.end(),
            [](const Shot& shot) { return shot.kind == ShotKind::Charge; }))
        return false;

    game.profile.stage = 2;
    game.start();
    game.clock = 15.f;
    game.wrecks.push_back({game.player + Vec{100, 0}});
    game.update(1.f / 60.f, readCombatInput(MOUSE_INPUT_RIGHT, 0));
    previousMouseButtons = MOUSE_INPUT_RIGHT;
    keys[KEY_INPUT_I] = 1;
    game.update(1.f / 60.f, readCombatInput(MOUSE_INPUT_RIGHT, 0));
    keys[KEY_INPUT_I] = 0;
    previousMouseButtons = 0;
    if (game.onParadeTime <= 0 || !smokeCapture(renderer, game, L"smoke-parade.png"))
        return false;

    game.profile.stage = 1;
    game.start();
    game.clock = 20.f;
    game.spawnEnemy(EnemyKind::Scout, game.player + Vec{175, 0});
    keys[KEY_INPUT_SPACE] = 1;
    game.update(1.f / 60.f, readCombatInput(0, 1));
    keys[KEY_INPUT_SPACE] = 0;
    for (int frame = 0; frame < 60 && game.explosions.empty(); ++frame)
        game.update(1.f / 60.f, {});
    if (game.explosions.empty() || !smokeCapture(renderer, game, L"smoke-grenade.png"))
        return false;

    game.start();
    game.clock = game.bossAt() + 1.f;
    game.bossSpawned = true;
    game.spawnEnemy(EnemyKind::Boss, {1080, 380});
    for (int i = 0; i < 4; ++i) {
        game.enemies.front().age = i * .65f;
        game.enemyFire(game.enemies.front());
    }
    keys[KEY_INPUT_SPACE] = 1;
    game.update(1.f / 60.f, readCombatInput(0, -1));
    if (!game.barrierActive || !smokeCapture(renderer, game, L"smoke-boss.png"))
        return false;

    // とどめの弾を置き、通常の撃破処理を経てクリア画面へ遷移させる。
    const auto& boss = game.enemies.front();
    Shot finalShot;
    finalShot.p = boss.p + Vec{-64, 0};
    finalShot.previous = finalShot.p;
    finalShot.damage = boss.hp;
    game.shots.push_back(finalShot);
    for (int i = 0; i < 120 && game.phase == Phase::Playing; ++i)
        game.update(1.f / 60.f, {});

    return game.phase == Phase::Clear
        && game.profile.stage == 2
        && smokeCapture(renderer, game, L"smoke-clear.png");
}

} // namespace boa
