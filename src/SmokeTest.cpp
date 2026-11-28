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
bool Application::runSmoke() const {
    Game scenario;
    if (!smokeCapture(renderer, scenario, L"smoke-title.png"))
        return false;

    scenario.start();
    scenario.clock = 10.f;
    scenario.spawnEnemy(EnemyKind::Gabu, {660, 400});
    scenario.wrecks.push_back({scenario.player + Vec{90, 0}});
    for (int i = 0; i < 12; ++i) {
        Bullet bullet;
        bullet.p = scenario.player + Vec{120.f + 30.f * i, (i % 3 - 1) * 35.f};
        bullet.velocity = {-130, 0};
        scenario.bullets.push_back(bullet);
    }

    Input input;
    input.absorbPress = true;
    scenario.update(1.f / 60.f, input);
    if (!scenario.absorbing || scenario.stats.absorbed == 0 || !smokeCapture(renderer, scenario, L"smoke-absorb.png"))
        return false;

    for (int i = 0; i < 60; ++i)
        scenario.update(1.f / 60.f, {});
    input = {};
    input.absorbRelease = true;
    scenario.update(1.f / 60.f, input);
    if (scenario.absorbCooldown <= 0 || !smokeCapture(renderer, scenario, L"smoke-cooldown.png"))
        return false;

    input = {};
    input.actionPress = true;
    scenario.update(1.f / 60.f, input);
    for (int i = 0; i < 47; ++i)
        scenario.update(1.f / 60.f, {});
    if (scenario.charge < MinCharge || !smokeCapture(renderer, scenario, L"smoke-charge.png"))
        return false;

    scenario.profile.stage = 2;
    scenario.start();
    scenario.clock = 15.f;
    scenario.wrecks.push_back({scenario.player + Vec{100, 0}});
    input = {};
    input.absorbPress = true;
    scenario.update(1.f / 60.f, input);
    input = {};
    input.skillPress = true;
    scenario.update(1.f / 60.f, input);
    if (scenario.onParadeTime <= 0 || !smokeCapture(renderer, scenario, L"smoke-parade.png"))
        return false;

    scenario.profile.stage = 1;
    scenario.start();
    scenario.clock = 20.f;
    scenario.spawnEnemy(EnemyKind::Scout, scenario.player + Vec{175, 0});
    input = {};
    input.selection = Action::Grenade;
    input.actionPress = true;
    scenario.update(1.f / 60.f, input);
    for (int frame = 0; frame < 60 && scenario.explosions.empty(); ++frame)
        scenario.update(1.f / 60.f, {});
    if (scenario.explosions.empty() || !smokeCapture(renderer, scenario, L"smoke-grenade.png"))
        return false;

    scenario.start();
    scenario.clock = scenario.bossAt() + 1.f;
    scenario.bossSpawned = true;
    scenario.spawnEnemy(EnemyKind::Boss, {1080, 380});
    for (int i = 0; i < 4; ++i) {
        scenario.enemies.front().age = i * .65f;
        scenario.enemyFire(scenario.enemies.front());
    }
    input = {};
    input.selection = Action::Barrier;
    input.actionPress = true;
    scenario.update(1.f / 60.f, input);
    if (!scenario.barrierActive || !smokeCapture(renderer, scenario, L"smoke-boss.png"))
        return false;

    // とどめの弾を置き、通常の撃破処理を経てクリア画面へ遷移させる。
    const auto& boss = scenario.enemies.front();
    Shot finalShot;
    finalShot.p = boss.p + Vec{-64, 0};
    finalShot.previous = finalShot.p;
    finalShot.damage = boss.hp;
    scenario.shots.push_back(finalShot);
    for (int i = 0; i < 120 && scenario.phase == Phase::Playing; ++i)
        scenario.update(1.f / 60.f, {});

    return scenario.phase == Phase::Clear
        && scenario.profile.stage == 2
        && smokeCapture(renderer, scenario, L"smoke-clear.png");
}

} // namespace boa
