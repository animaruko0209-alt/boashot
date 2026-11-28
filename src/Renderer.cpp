#include "Renderer.h"

#include "DxLib.h"

namespace boa {
namespace {

const unsigned Ink = GetColor(7, 13, 27), White = GetColor(228, 241, 247);
const unsigned Cyan = GetColor(77, 227, 233), Gold = GetColor(255, 198, 98);
const unsigned Red = GetColor(255, 100, 116), Muted = GetColor(120, 148, 169);
const unsigned Violet = GetColor(174, 166, 255), Green = GetColor(132, 233, 169);
const unsigned Track = GetColor(30, 43, 59), Rule = GetColor(39, 62, 78);

// DxLib の図形描画に渡す座標へ変換する。
void box(int x, int y, int w, int h, unsigned color, bool fill = true) {
    DrawBox(x, y, x + w, y + h, color, fill ? TRUE : FALSE);
}

void line(float x, float y, float x2, float y2, unsigned color, int width = 1) {
    DrawLine(
        static_cast<int>(x), static_cast<int>(y),
        static_cast<int>(x2), static_cast<int>(y2),
        color, width);
}

void circle(Vec p, float radius, unsigned color, bool fill = true) {
    DrawCircle(
        static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(radius),
        color, fill ? TRUE : FALSE);
}

void triangle(Vec a, Vec b, Vec c, unsigned color, bool fill = true) {
    DrawTriangle(
        static_cast<int>(a.x), static_cast<int>(a.y),
        static_cast<int>(b.x), static_cast<int>(b.y),
        static_cast<int>(c.x), static_cast<int>(c.y),
        color, fill ? TRUE : FALSE);
}

std::wstring number(float n) {
    return std::to_wstring(static_cast<int>(std::max(0.f, n)));
}

std::wstring seconds(float value) {
    const int tenths = static_cast<int>(std::ceil(std::max(0.f, value) * 10.f));
    return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10) + L" 秒";
}

void background(float time) {
    ClearDrawScreen();
    box(0, 0, 1280, 720, Ink);

    // 星を3層に分け、流れる速さの差で横スクロールを見せる。
    for (int layer = 0; layer < 3; ++layer) {
        for (int i = 0; i < 65; ++i) {
            float x = std::fmod(i * 157.37f + layer * 263 - time * (18 + layer * 35), 1320.f);
            if (x < 0)
                x += 1320;

            int y = 115 + (i * 73 + layer * 117) % 530;
            unsigned c = layer == 2
                ? GetColor(121, 164, 184)
                : GetColor(29 + layer * 20, 48 + layer * 20, 70 + layer * 20);
            box(static_cast<int>(x), y, layer == 2 ? 3 : 2, 2, c);
        }
    }

    Vec planet{1040 - time * 9.f, 760};
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100);
    circle(planet, 328, GetColor(19, 29, 49));
    circle(planet, 328, GetColor(65, 105, 127), false);
    circle(planet, 339, GetColor(40, 68, 88), false);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void ship(Vec p, float time, bool absorbing, float scale = 1) {
    auto point = [&](float x, float y) {
        return p + Vec{x * scale, y * scale};
    };

    const unsigned engine = absorbing ? Cyan : Gold;
    for (float side : {-1.f, 1.f}) {
        const float tail = -34.f + std::sin(time * 44.f + side) * 6.f;
        triangle(point(-17, side * 10), point(tail, side * 14), point(-17, side * 18), engine);
        triangle(point(12, side * 5), point(-21, side * 25), point(-17, side * 6), Cyan);
        triangle(point(0, side * 13), point(-17, side * 23), point(-14, side * 13), White);
    }

    triangle(point(29, 0), point(-20, -10), point(-20, 10), White);
    triangle(point(14, 0), point(-5, -5), point(-5, 5), GetColor(9, 54, 78));
    circle(p, 2 * scale, White);
}

void cone(const Game& g) {
    if (!g.absorbing)
        return;

    constexpr int segments = 28;
    float a = g.halfAngle(), r = g.range();
    const unsigned color = g.onParadeTime > 0 ? Violet : Cyan;

    // 奥の背景が見える薄い塗りと、境界を示す輪郭を別々に描く。
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 23);
    for (int i = 0; i < segments; ++i) {
        float angle1 = -a + 2 * a * i / segments,
              angle2 = -a + 2 * a * (i + 1) / segments;
        triangle(
            g.player,
            g.player + Vec{std::cos(angle1) * r, std::sin(angle1) * r},
            g.player + Vec{std::cos(angle2) * r, std::sin(angle2) * r},
            color);
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 125);
    for (int i = 0; i < segments; ++i) {
        float a1 = -a + 2 * a * i / segments,
              a2 = -a + 2 * a * (i + 1) / segments;
        Vec v1 = g.player + Vec{std::cos(a1) * r, std::sin(a1) * r};
        Vec v2 = g.player + Vec{std::cos(a2) * r, std::sin(a2) * r};
        line(v1.x, v1.y, v2.x, v2.y, color);
    }
    for (float side : {-a, a})
        line(
            g.player.x, g.player.y,
            g.player.x + std::cos(side) * r, g.player.y + std::sin(side) * r,
            color);

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void meterFill(int x, int y, int w, float amount, float maximum, unsigned color) {
    if (maximum <= 0)
        return;

    const int filled = static_cast<int>(w * clamp(amount / maximum, 0, 1));
    if (filled > 0)
        box(x, y, filled, 8, color);
}

void meter(int x, int y, int w, float amount, float maximum, unsigned color) {
    box(x, y, w, 8, Track);
    meterFill(x, y, w, amount, maximum, color);
}

void drawGimmick(const Game& g) {
    const Vec p = g.hazardPosition();
    switch (g.activeGimmick()) {
    case Gimmick::Gravity:
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 36);
        circle(p, 142, Violet);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        for (int i = 0; i < 3; ++i) {
            const float radius = 50.f + std::fmod(g.clock * 39.f + i * 33.f, 98.f);
            circle(p, radius, Rule, false);
        }
        circle(p, 34, Violet);
        circle(p, 30, Ink);
        break;
    case Gimmick::Terrain:
        circle(p, TerrainRadius, GetColor(58, 60, 82));
        circle(p + Vec{-21, -16}, 27, Track);
        circle(p + Vec{20, 34}, 37, Track);
        circle(p + Vec{39, -31}, 13, Track);
        circle(p, TerrainRadius, Muted, false);
        break;
    case Gimmick::Crossfire:
        circle(p, 24, Track);
        triangle(p + Vec{0, 26}, p + Vec{-17, -18}, p + Vec{17, -18}, Green);
        circle(p, 7, Ink);
        break;
    default:
        break;
    }
}

void drawShot(const Shot& shot) {
    if (shot.kind == ShotKind::Normal) {
        line(shot.p.x - 14, shot.p.y, shot.p.x, shot.p.y, White, 3);
        circle(shot.p, 3, Gold);
        return;
    }
    if (shot.kind == ShotKind::Grenade) {
        line(shot.p.x - 20, shot.p.y, shot.p.x, shot.p.y, Red, 3);
        circle(shot.p, shot.radius + 3, Gold, false);
        circle(shot.p, shot.radius, Red);
        circle(shot.p, 3, White);
        return;
    }

    SetDrawBlendMode(DX_BLENDMODE_ADD, 80);
    circle(shot.p, shot.radius + 12, Gold);
    line(shot.p.x - 75, shot.p.y, shot.p.x, shot.p.y, Gold, static_cast<int>(shot.radius));
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    circle(shot.p, shot.radius, Gold);
    circle(shot.p, shot.radius * .5f, White);
}

void drawBullet(const Bullet& bullet) {
    const Vec p = bullet.p;
    const float r = bullet.radius;
    if (bullet.rock) {
        triangle(p + Vec{-r, -.4f * r}, p + Vec{.35f * r, -r}, p + Vec{r, .55f * r}, Red);
        triangle(p + Vec{-r, -.4f * r}, p + Vec{r, .55f * r}, p + Vec{-.5f * r, r}, GetColor(149, 75, 81));
        line(p.x - r * .35f, p.y - r * .4f, p.x + r * .4f, p.y + r * .15f, Gold, 2);
        return;
    }

    const unsigned color = bullet.crossfire ? Green : Cyan;
    SetDrawBlendMode(DX_BLENDMODE_ADD, 65);
    circle(p, r + 5, color);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    circle(p, r, color);
    circle(p, std::max(1.f, r * .4f), White);
}

void veil() {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
    box(0, 110, 1280, 542, Ink);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

} // namespace

void Renderer::createFonts() {
    fontSmall = CreateFontToHandle(L"Yu Gothic UI", 17, 4, DX_FONTTYPE_ANTIALIASING_4X4);
    fontBody = CreateFontToHandle(L"Yu Gothic UI", 23, 5, DX_FONTTYPE_ANTIALIASING_4X4);
    fontTitle = CreateFontToHandle(L"Yu Gothic UI", 34, 7, DX_FONTTYPE_ANTIALIASING_4X4);
    fontHero = CreateFontToHandle(L"Bahnschrift", 104, 9, DX_FONTTYPE_ANTIALIASING_4X4);
}

void Renderer::deleteFonts() {
    DeleteFontToHandle(fontSmall);
    DeleteFontToHandle(fontBody);
    DeleteFontToHandle(fontTitle);
    DeleteFontToHandle(fontHero);
}

void Renderer::text(int x, int y, const std::wstring& value, unsigned color, int font) const {
    DrawStringToHandle(x, y, value.c_str(), color, font < 0 ? fontBody : font);
}

void Renderer::centered(int y, const std::wstring& value, unsigned color, int font) const {
    if (font < 0)
        font = fontBody;

    int w = GetDrawStringWidthToHandle(value.c_str(), static_cast<int>(value.size()), font);
    text((1280 - w) / 2, y, value, color, font);
}

void Renderer::enemySprite(const Enemy& e, const Game& g) const {
    unsigned color = e.kind == EnemyKind::Rock ? Red
        : e.kind == EnemyKind::Heavy ? Gold
        : GetColor(159, 148, 236);

    Vec p = e.p;
    if (e.kind == EnemyKind::Boss) {
        unsigned armor = GetColor(46, 62, 83);
        triangle(p + Vec{-66, 0}, p + Vec{44, -58}, p + Vec{44, 58}, armor);
        box(static_cast<int>(p.x + 10), static_cast<int>(p.y - 80), 72, 160, armor);
        for (int side : {-1, 1}) {
            triangle(p + Vec{30, side * 50.f}, p + Vec{-30, side * 100.f}, p + Vec{78, side * 76.f}, Muted);
            line(p.x - 30, p.y + side * 100, p.x + 30, p.y + side * 50, Cyan, 3);
        }
        float cycle = std::fmod(e.age, 7.5f);
        circle(p, 32, cycle > 4.6f ? Gold : Cyan, false);
        circle(p, 17 + std::sin(e.age * 6) * 3, cycle > 4.6f ? Gold : Cyan);
        text(
            900, 164,
            cycle > 4.6f ? L"反撃のチャンス" : L"弾幕展開中",
            cycle > 4.6f ? Gold : Cyan, fontSmall);
        return;
    }

    if (e.kind == EnemyKind::Rock) {
        box(static_cast<int>(p.x - 22), static_cast<int>(p.y - 17), 44, 34, GetColor(75, 44, 56));
        triangle(p + Vec{-34, 0}, p + Vec{-8, -13}, p + Vec{-8, 13}, color);
        box(static_cast<int>(p.x + 9), static_cast<int>(p.y - 24), 7, 48, color);
    } else if (e.kind == EnemyKind::Gabu) {
        triangle(p + Vec{-28, -21}, p + Vec{25, -15}, p + Vec{12, 0}, Violet);
        triangle(p + Vec{-28, 21}, p + Vec{25, 15}, p + Vec{12, 0}, Violet);
        circle(p + Vec{13, 0}, 8, White);
        if (e.absorbingShots()) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 36);
            circle(p, 44, Violet);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            circle(p, 44, Violet, false);
            circle(p, 36 + std::sin(g.clock * 22.f) * 4.f, Violet, false);
        }
    } else {
        float r = e.kind == EnemyKind::Heavy ? 34.f : 23.f;
        triangle(p + Vec{-r, 0}, p + Vec{r * .8f, -r * .7f}, p + Vec{r * .8f, r * .7f}, color);
        triangle(p + Vec{-r * .65f, 0}, p + Vec{r * .55f, -r * .4f}, p + Vec{r * .55f, r * .4f}, Ink);
        circle(p, 5, color);
    }
    if (e.nextShot - e.age > 0 && e.nextShot - e.age < .45f && e.age < 7.2f) {
        circle(p + Vec{-23, 0}, 13 + std::sin(g.clock * 25) * 3, White, false);
    }
    if (e.hp < e.maxHp) {
        int x = static_cast<int>(p.x - 24), y = static_cast<int>(p.y - 38);
        box(x, y, 48, 3, GetColor(46, 56, 75));
        box(x, y, static_cast<int>(48 * e.hp / e.maxHp), 3, color);
    }
}

void Renderer::drawHud(const Game& g) const {
    box(0, 0, 1280, 110, Ink);
    line(26, 109, 1254, 109, Rule);
    text(27, 14, L"BOASHOT", White, fontTitle);
    text(29, 62, L"STAGE " + std::to_wstring(g.stage), Muted, fontSmall);

    text(244, 21, L"ライフ  " + number(std::ceil(g.hp)) + L" / " + number(g.maxHp()), White, fontSmall);
    meter(244, 51, 160, g.hp, g.maxHp(), g.hp < 30 ? Red : Green);

    text(
        433, 21, L"エネルギー  " + number(g.energy) + L" / " + number(g.capacity()),
        Cyan, fontSmall);
    // 合計を金色で塗り、使用可能分を水色で重ねてチャージ確保分を示す。
    meter(433, 51, 224, g.energy + g.charge, g.capacity(), Gold);
    meterFill(433, 51, 224, g.energy, g.capacity(), Cyan);
    if (g.charging) {
        const std::wstring status = g.charge >= MinCharge
            ? L"  発射可能"
            : L"  / 最低 " + number(MinCharge);
        text(433, 70, L"チャージ  " + number(g.charge) + status, Gold, fontSmall);
    }

    const std::wstring level = L"Lv " + std::to_wstring(g.level);
    const float nextLevel = g.xpToNextLevel();
    if (nextLevel > 0) {
        text(685, 21, level + L"    " + number(g.levelXp) + L" / " + number(nextLevel), Violet, fontSmall);
        meter(685, 51, 180, g.levelXp, nextLevel, Violet);
    } else {
        text(685, 21, level + L"    MAX", Violet, fontSmall);
        meter(685, 51, 180, 1, 1, Violet);
    }

    // 吸収中は残り時間、クールタイム中は再使用までの進み具合を表示する。
    if (g.absorbing) {
        const float left = AbsorbDuration - g.absorbTime;
        text(898, 21, L"吸収中  " + seconds(left), Cyan, fontSmall);
        meter(898, 51, 352, left, AbsorbDuration, Cyan);
    } else if (g.absorbCooldown > 0) {
        text(898, 21, L"吸収クールタイム  " + seconds(g.absorbCooldown), Gold, fontSmall);
        meter(
            898, 51, 352, g.absorbCooldownTotal - g.absorbCooldown,
            g.absorbCooldownTotal, Gold);
    } else {
        text(898, 21, L"吸収可能  [右クリック]", Cyan, fontSmall);
        meter(898, 51, 352, 1, 1, Cyan);
    }
    if (g.onParadeTime > 0) {
        text(898, 70, L"オンパレード  " + seconds(g.onParadeTime), Violet, fontSmall);
    }

    box(0, 652, 1280, 68, Ink);
    line(26, 652, 1254, 652, Rule);
    text(28, 656, L"左クリック  射撃  /  右クリック  吸収", White, fontSmall);
    text(28, 676, L"ホイール  選択  /  Space  発動", White, fontSmall);
    text(28, 696, L"WASD / 矢印  移動  /  P  一時停止", Muted, fontSmall);

    const wchar_t* labels[] = {L"チャージ", L"グレネード", L"バリア"};
    const std::wstring notes[] = {
        L"長押し → 離す",
        number(GrenadeCost) + L" 消費",
        L"長押し " + number(BarrierRate) + L" / 秒"
    };
    for (int i = 0; i < static_cast<int>(ActionOrder.size()); ++i) {
        const int x = 355 + i * 163;
        const bool selected = g.selected == ActionOrder[i];
        box(x, 663, 149, 47, selected ? Track : Ink);
        box(x, 663, 149, 47, selected ? Gold : Rule, false);
        text(x + 10, 665, labels[i], selected ? Gold : White, fontSmall);
        text(x + 10, 688, notes[i], Muted, fontSmall);
    }

    if (g.skillUnlocked) {
        text(864, 663, L"I   オンパレード  /  " + number(ParadeCost) + L" 消費", Violet, fontSmall);
        text(864, 688, L"吸収範囲拡大・クールタイム短縮", Muted, fontSmall);
    } else {
        text(864, 663, L"スキル  未取得", Muted, fontSmall);
        text(864, 688, L"紫の残骸を吸収すると解放", Muted, fontSmall);
    }
}

void Renderer::drawGame(const Game& g) const {
    background(g.clock);

    // 弾やエフェクトの描画を上下のHUDの間に制限する。
    SetDrawArea(0, 110, 1280, 652);
    drawGimmick(g);
    cone(g);

    for (const auto& wreck : g.wrecks) {
        const Vec p = wreck.p;
        triangle(p + Vec{0, -14}, p + Vec{14, 0}, p + Vec{0, 14}, Violet);
        triangle(p + Vec{0, -14}, p + Vec{-14, 0}, p + Vec{0, 14}, Violet);
        circle(p, 4, White);
        circle(p, 18 + std::sin(g.clock * 12.f) * 4.f, Violet, false);
    }
    for (const auto& e : g.enemies)
        enemySprite(e, g);
    for (const auto& shot : g.shots)
        drawShot(shot);
    for (const auto& bullet : g.bullets)
        drawBullet(bullet);

    for (const auto& explosion : g.explosions) {
        const float remaining = clamp(explosion.life / ExplosionDuration, 0, 1);
        const float radius = ExplosionRadius * (1.f - .72f * remaining);
        SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(108 * remaining));
        circle(explosion.p, radius, Gold);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255 * remaining));
        circle(explosion.p, radius, Gold, false);
    }

    for (const auto& particle : g.particles) {
        const unsigned color = particle.kind == 2 ? Red
            : particle.kind == 1 ? Gold
            : particle.kind == 3 ? Violet
            : Cyan;
        SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(220 * particle.life / particle.fullLife));
        circle(particle.p, 2 + particle.life * 3, color);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    if (g.invulnerable <= 0 || static_cast<int>(g.clock * 16) % 2 == 0) {
        ship(g.player, g.clock, g.absorbing);
    }

    // 無敵中に機体を点滅させても、当たり判定の目印は表示し続ける。
    circle(g.player, 9, Ink, false);
    circle(g.player, 8, White, false);
    if (g.charging) {
        circle(g.player + Vec{25, 0}, 7 + std::sqrt(g.charge) * 1.5f, Gold, false);
    }
    if (g.barrierActive) {
        circle(g.player, 54, Violet, false);
        circle(g.player, 50, Violet, false);
    }

    if (g.flash > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 35);
        box(0, 110, 1280, 542, Red);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // HUDと戦闘中の案内はエフェクトの手前に描く。
    SetDrawArea(0, 0, 1280, 720);
    drawHud(g);
    if (!g.bossSpawned) {
        const float progress = clamp(g.clock / g.bossAt(), 0, 1);
        box(28, 104, static_cast<int>(1225 * progress), 2, Cyan);
        if (g.clock < 4)
            centered(144, L"青い弾を右クリックで吸収。ホイールで技を選び、Spaceで発動", Cyan, fontSmall);
        if (g.clock > g.bossAt() - 3)
            centered(300, L"WARNING  /  大型機接近", Red, fontTitle);
    }

    const wchar_t* hazard = nullptr;
    switch (g.activeGimmick()) {
    case Gimmick::Gravity:
        hazard = L"ブラックホール";
        break;
    case Gimmick::Terrain:
        hazard = L"巨大な岩";
        break;
    case Gimmick::Meteors:
        hazard = L"流星群";
        break;
    case Gimmick::Crossfire:
        hazard = L"第三勢力";
        break;
    default:
        break;
    }
    if (hazard)
        text(28, 123, hazard, Gold, fontSmall);

    for (const auto& e : g.enemies)
        if (e.kind == EnemyKind::Boss) {
            text(340, 124, L"BULWARK", Muted, fontSmall);
            meter(520, 129, 450, e.hp, e.maxHp, Red);
        }

    if (g.energy < 1 && g.charge < 1 && g.clearDelay < 0) {
        centered(609, L"エネルギー不足  /  敵弾を吸収しよう", Gold, fontSmall);
    }
}

void Renderer::title(const Game& g, float time) const {
    background(time);
    text(85, 94, L"トップビュー横スクロールシューティング", Cyan, fontSmall);
    text(78, 141, L"BOASHOT", White, fontHero);
    text(85, 250, L"ボアショット", Muted, fontTitle);
    text(85, 330, L"敵の猛攻を、自分の弾薬に。", White, fontTitle);
    text(85, 385, L"左クリックで射撃、右クリックで吸収。", Muted);
    text(85, 416, L"ホイールでアクションを選び、Spaceで発動。", Muted);
    text(85, 454, L"Enter  出撃   /   STAGE " + std::to_wstring(g.profile.stage), Gold, fontTitle);
    text(85, 517, L"Esc  終了", Muted, fontSmall);
    ship({982, 364}, time, true, 4);
    line(800, 364, 902, 364, Cyan);
    for (int i = 0; i < 8; ++i) {
        const float offset = std::fmod(time * 85 + i * 43, 300.f);
        circle({1045 + offset, 260 + i * 30.f}, static_cast<float>(4 + i % 3), Cyan, false);
    }
}

void Renderer::drawScene(const Game& g, float menuTime, bool saveFailed) const {
    if (g.phase == Phase::Title) {
        title(g, menuTime);
    } else {
        drawGame(g);

        // 一時停止と戦闘結果は、最後に描いた戦闘画面に重ねる。
        if (g.phase == Phase::Paused) {
            veil();
            centered(265, L"PAUSED", White, fontHero);
            centered(420, L"Enter / P / Esc  再開     Q  タイトル", Gold);
        } else if (g.phase == Phase::Clear) {
            veil();
            centered(200, L"STAGE CLEAR", Cyan, fontHero);
            centered(345, L"到達レベル  " + std::to_wstring(g.level), Gold, fontTitle);
            centered(
                415, L"撃破 " + std::to_wstring(g.stats.kills)
                    + L"   回収 " + std::to_wstring(g.stats.absorbed)
                    + L"   獲得エネルギー " + number(g.stats.energyGained),
                White);
            centered(470, L"最大チャージ  " + number(g.stats.biggestCharge), Muted);
            centered(540, L"Enter  次のステージへ     Esc  タイトル", Gold);
        } else if (g.phase == Phase::GameOver) {
            veil();
            centered(240, L"GAME OVER", Red, fontHero);
            centered(415, L"Enter  同じステージを再挑戦     Esc  タイトル", Gold);
        }
    }

    if (saveFailed)
        centered(3, L"ステージの進行データを読み書きできません。", Red, fontSmall);
}

} // namespace boa
