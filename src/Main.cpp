#include "DxLib.h"
#include "Game.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace boa;
namespace {
const unsigned Ink = GetColor(7, 13, 27), White = GetColor(228, 241, 247);
const unsigned Cyan = GetColor(77, 227, 233), Gold = GetColor(255, 198, 98);
const unsigned Red = GetColor(255, 100, 116), Muted = GetColor(120, 148, 169);
int fontSmall = -1, fontBody = -1, fontTitle = -1, fontHero = -1;
std::filesystem::path savePath;
bool saveFailed = false, noSave = false;

void text(int x, int y, const std::wstring& value, unsigned color = White, int font = -1) {
    DrawStringToHandle(x, y, value.c_str(), color, font < 0 ? fontBody : font);
}
void centered(int y, const std::wstring& value, unsigned color = White, int font = -1) {
    if (font < 0) font = fontBody;
    int w = GetDrawStringWidthToHandle(value.c_str(), static_cast<int>(value.size()), font);
    text((1280 - w) / 2, y, value, color, font);
}
void box(int x, int y, int w, int h, unsigned color, bool fill = true) {
    DrawBox(x, y, x + w, y + h, color, fill ? TRUE : FALSE);
}
void line(float x, float y, float x2, float y2, unsigned color, int width = 1) {
    DrawLine(static_cast<int>(x), static_cast<int>(y), static_cast<int>(x2), static_cast<int>(y2), color, width);
}
void circle(Vec p, float radius, unsigned color, bool fill = true) {
    DrawCircle(static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(radius), color, fill ? TRUE : FALSE);
}
void triangle(Vec a, Vec b, Vec c, unsigned color, bool fill = true) {
    DrawTriangle(static_cast<int>(a.x), static_cast<int>(a.y), static_cast<int>(b.x), static_cast<int>(b.y),
        static_cast<int>(c.x), static_cast<int>(c.y), color, fill ? TRUE : FALSE);
}
std::wstring number(float n) { return std::to_wstring(static_cast<int>(std::ceil(std::max(0.f, n)))); }
std::wstring branchName(Branch branch) {
    switch (branch) {
    case Branch::Attack: return L"攻撃特化型";
    case Branch::Absorb: return L"吸収特化型";
    case Branch::Defense: return L"防御特化型";
    default: return L"基本機体";
    }
}
unsigned branchColor(Branch branch) {
    return branch == Branch::Attack ? Gold : branch == Branch::Defense ? GetColor(155, 172, 255) : Cyan;
}
void save(const Profile& p) {
    if (noSave) return;
    auto temp = savePath; temp += L".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << "BOASHOT_1\n" << p.xp << ' ' << p.stage << ' ' << p.rank << ' '
            << static_cast<int>(p.branch) << ' ' << p.toggleAbsorb << '\n';
        out.close();
        if (!out) { saveFailed = true; return; }
    }
    saveFailed = !MoveFileExW(temp.c_str(), savePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
void load(Profile& p) {
    std::ifstream in(savePath);
    if (!in) return;
    std::string version;
    int branch = 0, toggle = 0;
    Profile candidate;
    if (in >> version >> candidate.xp >> candidate.stage >> candidate.rank >> branch >> toggle &&
        version == "BOASHOT_1" && candidate.xp >= 0 && candidate.xp < 10000000 &&
        candidate.stage >= 1 && candidate.stage < 100000 && candidate.rank >= 0 && candidate.rank <= 3 &&
        branch >= 0 && branch <= 3 && toggle >= 0 && toggle <= 1 &&
        ((branch == 0 && candidate.rank == 0) || (branch > 0 && candidate.rank > 0))) {
        candidate.branch = static_cast<Branch>(branch); candidate.toggleAbsorb = toggle != 0; p = candidate;
    } else { saveFailed = true; }
}

void background(float time) {
    ClearDrawScreen();
    box(0, 0, 1280, 720, Ink);
    // Three parallax layers make the automatic horizontal scroll visible.
    for (int layer = 0; layer < 3; ++layer) {
        for (int i = 0; i < 65; ++i) {
            float x = std::fmod(i * 157.37f + layer * 263 - time * (18 + layer * 35), 1320.f);
            if (x < 0) x += 1320;
            int y = 115 + (i * 73 + layer * 117) % 530;
            unsigned c = layer == 2 ? GetColor(121, 164, 184) : GetColor(29 + layer * 20, 48 + layer * 20, 70 + layer * 20);
            box(static_cast<int>(x), y, layer == 2 ? 3 : 2, 2, c);
        }
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 55);
    for (int i = 0; i < 20; ++i) {
        float x = std::fmod(i * 90.f - time * 65, 1400.f);
        if (x < 0) x += 1400;
        line(x, 652, x + 180, 540, GetColor(38, 79, 94));
    }
    for (int y = 560; y < 652; y += 23) line(0, static_cast<float>(y), 1280, static_cast<float>(y), GetColor(38, 79, 94));
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
void ship(Vec p, Branch branch, float time, bool absorbing, bool firing, float scale = 1) {
    unsigned c = branchColor(branch);
    auto point = [&](float x, float y) { return p + Vec{x * scale, y * scale}; };
    float tail = 21 + std::sin(time * 47) * 6 + (firing ? 8 : 0);
    triangle(point(-18, -7), point(-tail - 14, 0), point(-18, 7), absorbing ? Cyan : Gold);
    triangle(point(28, 0), point(-19, -11), point(-10, 0), White);
    triangle(point(28, 0), point(-19, 11), point(-10, 0), c);
    triangle(point(0, -4), point(-20, -23), point(-13, -3), c);
    triangle(point(0, 4), point(-20, 23), point(-13, 3), c);
    triangle(point(7, 0), point(-5, -4), point(-5, 4), GetColor(9, 54, 78));
    circle(p, 3 * scale, White);
}
void cone(const Game& g) {
    if (!g.absorbing) return;
    constexpr int segments = 28;
    float a = g.halfAngle(), r = g.range();
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 23);
    for (int i = 0; i < segments; ++i) {
        float angle1 = -a + 2 * a * i / segments, angle2 = -a + 2 * a * (i + 1) / segments;
        triangle(g.player, g.player + Vec{std::cos(angle1) * r, std::sin(angle1) * r},
            g.player + Vec{std::cos(angle2) * r, std::sin(angle2) * r}, Cyan);
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 125);
    for (int i = 0; i < segments; ++i) {
        float a1 = -a + 2 * a * i / segments, a2 = -a + 2 * a * (i + 1) / segments;
        Vec v1 = g.player + Vec{std::cos(a1) * r, std::sin(a1) * r};
        Vec v2 = g.player + Vec{std::cos(a2) * r, std::sin(a2) * r};
        line(v1.x, v1.y, v2.x, v2.y, Cyan);
    }
    for (float side : {-a, a}) line(g.player.x, g.player.y, g.player.x + std::cos(side) * r, g.player.y + std::sin(side) * r, Cyan);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
void enemySprite(const Enemy& e, const Game& g) {
    unsigned color = e.kind == EnemyKind::Rock ? Red : e.kind == EnemyKind::Heavy ? Gold : GetColor(159, 148, 236);
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
        text(900, 164, cycle > 4.6f ? L"反撃のチャンス" : L"弾幕展開中", cycle > 4.6f ? Gold : Cyan, fontSmall);
        return;
    }
    if (e.kind == EnemyKind::Rock) {
        box(static_cast<int>(p.x - 22), static_cast<int>(p.y - 17), 44, 34, GetColor(75, 44, 56));
        triangle(p + Vec{-34, 0}, p + Vec{-8, -13}, p + Vec{-8, 13}, color);
        box(static_cast<int>(p.x + 9), static_cast<int>(p.y - 24), 7, 48, color);
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
        box(x, y, 48, 3, GetColor(46, 56, 75)); box(x, y, static_cast<int>(48 * e.hp / e.maxHp), 3, color);
    }
}
void meter(int x, int y, int w, float amount, float maximum, unsigned c) {
    box(x, y, w, 8, GetColor(30, 43, 59));
    box(x, y, static_cast<int>(w * clamp(amount / maximum, 0, 1)), 8, c);
}
void drawGame(const Game& g) {
    background(g.clock);
    SetDrawArea(0, 110, 1280, 652);
    cone(g);
    for (const auto& e : g.enemies) enemySprite(e, g);
    for (const auto& s : g.shots) {
        if (s.charged) {
            SetDrawBlendMode(DX_BLENDMODE_ADD, 80);
            circle(s.p, s.radius + 12, Gold);
            line(s.p.x - 75, s.p.y, s.p.x, s.p.y, Gold, static_cast<int>(s.radius));
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            circle(s.p, s.radius, Gold); circle(s.p, s.radius * .5f, White);
        } else { line(s.p.x - 20, s.p.y, s.p.x + 7, s.p.y, Gold, 3); }
    }
    for (const auto& b : g.bullets) {
        if (b.rock) {
            triangle(b.p + Vec{-13, -6}, b.p + Vec{5, -14}, b.p + Vec{14, 8}, Red);
            triangle(b.p + Vec{-13, -6}, b.p + Vec{14, 8}, b.p + Vec{-7, 14}, GetColor(149, 75, 81));
            line(b.p.x - 5, b.p.y - 5, b.p.x + 6, b.p.y + 2, Gold, 2);
        } else {
            float fraction = b.energy / b.maxEnergy;
            if (fraction <= .001f) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90); circle(b.p, 5, Muted, false);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            } else {
                float radius = 3 + b.radius * fraction;
                SetDrawBlendMode(DX_BLENDMODE_ADD, 65); circle(b.p, radius + 5, Cyan);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                circle(b.p, radius, Cyan); circle(b.p, std::max(1.f, radius * .4f), White);
            }
            if (b.siphoning) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 45);
                line(b.p.x, b.p.y, g.player.x, g.player.y, Cyan);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            }
        }
    }
    for (const auto& p : g.particles) {
        unsigned c = p.kind == 2 ? Red : p.kind == 1 ? Gold : p.kind == 3 ? GetColor(155, 172, 255) : Cyan;
        SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(220 * p.life / p.fullLife));
        circle(p.p, 2 + p.life * 3, c);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    if (g.invulnerable <= 0 || static_cast<int>(g.clock * 16) % 2 == 0)
        ship(g.player, g.profile.branch, g.clock, g.absorbing, g.firing);
    if (g.charge > 0) {
        circle(g.player + Vec{22, 0}, 7 + std::sqrt(g.charge) * 1.5f, Gold, false);
        text(static_cast<int>(g.player.x - 20), static_cast<int>(g.player.y + 29), number(g.charge), Gold, fontSmall);
    }
    if (g.barrierActive) { circle(g.player, 54, branchColor(Branch::Defense), false); circle(g.player, 50, branchColor(Branch::Defense), false); }
    if (g.flash > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 35); box(0, 110, 1280, 542, Red); SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    SetDrawArea(0, 0, 1280, 720);
    box(0, 0, 1280, 110, Ink); line(26, 109, 1254, 109, GetColor(39, 62, 78));
    text(28, 17, L"BOASHOT", White, fontTitle);
    text(29, 66, L"STAGE " + std::to_wstring(g.profile.stage) + L"  /  " + branchName(g.profile.branch), Muted, fontSmall);
    text(325, 19, L"HULL  " + number(g.hp) + L" / " + number(g.maxHp()), White, fontSmall);
    meter(325, 48, 200, g.hp, g.maxHp(), g.hp < 30 ? Red : Cyan);
    text(566, 19, L"ENERGY  " + number(g.energy) + L" / " + number(g.capacity()), Cyan, fontSmall);
    meter(566, 48, 285, g.energy + g.charge, g.capacity(), Gold);
    meter(566, 48, 285, g.energy, g.capacity(), Cyan);
    text(566, 69, L"チャージ確保  " + number(g.charge), Gold, fontSmall);
    text(901, 20, g.absorbing ? L"ABSORB / 吸収中" : g.charging ? L"CHARGE / チャージ中" : g.firing ? L"FIRE / 連射中" : g.barrierActive ? L"BARRIER" : L"READY", g.absorbing ? Cyan : Gold, fontBody);
    text(902, 65, L"撃破 " + std::to_wstring(g.stats.kills) + L"   回収 " + std::to_wstring(g.stats.absorbed), Muted, fontSmall);
    box(0, 652, 1280, 68, Ink); line(26, 652, 1254, 652, GetColor(39, 62, 78));
    text(28, 665, L"移動 WASD / 矢印    吸収 右クリック    連射 左クリック長押し", Muted, fontSmall);
    text(28, 690, L"Space長押し：チャージ → 離して発射   赤い岩：回避" + std::wstring(g.profile.branch == Branch::Defense ? L" / Shift バリア" : L""), Muted, fontSmall);
    text(830, 666, g.profile.toggleAbsorb ? L"右：ON / OFF 切替" : L"右：押している間だけ吸収", Cyan, fontSmall);
    text(1010, 690, L"F1 設定   P 一時停止", Muted, fontSmall);
    if (!g.bossSpawned) {
        float progress = clamp(g.clock / g.bossAt(), 0, 1);
        box(28, 104, static_cast<int>(1225 * progress), 2, Cyan);
        if (g.clock < 5) centered(145, L"右クリックで吸収、左クリック長押しで連射。Spaceをためて離すと大技。", Cyan, fontSmall);
        if (g.clock > g.bossAt() - 3) centered(300, L"WARNING  /  大型機接近", Red, fontTitle);
    }
    for (const auto& e : g.enemies) if (e.kind == EnemyKind::Boss) {
        text(340, 124, L"01 / BULWARK", Muted, fontSmall); meter(520, 129, 450, e.hp, e.maxHp, Red);
    }
    if (g.energy < 1 && g.charge < 1 && g.clearDelay < 0) centered(606, L"エネルギー不足 — 敵の青い弾を吸収しよう", Gold, fontSmall);
}
void veil() {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225); box(0, 110, 1280, 542, Ink);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
void settings(const Game& g, bool opened) {
    if (!opened) return;
    veil(); centered(190, L"操作設定", White, fontTitle);
    centered(262, L"X：吸収方式を変更", Muted);
    centered(310, g.profile.toggleAbsorb ? L"切り替え式：一度押すと開始、もう一度で解除" : L"長押し式：押している間だけ吸収", Cyan);
    centered(375, L"左クリック：連射 / Space：チャージ。最後に押した操作を優先します。", White, fontSmall);
    centered(412, L"吸収・連射に切り替えると、チャージを中止して確保分を返却します。", White, fontSmall);
    centered(491, L"F1 / Esc で戻る", Gold);
}
void title(const Game& g, float time) {
    background(time);
    text(84, 80, L"ABSORB. RESERVE. RELEASE.", Cyan, fontSmall);
    text(78, 141, L"BOASHOT", White, fontHero);
    text(85, 250, L"ボアショット", Muted, fontTitle);
    text(85, 330, L"敵の猛攻を、自分の弾薬に。", White, fontTitle);
    text(85, 397, L"弾幕を吸い込み、ためた力で撃ち返せ。", Muted);
    text(85, 454, L"ENTER   出撃 / 続きから", Gold);
    text(85, 503, L"F1  操作設定     Esc  終了", Muted, fontSmall);
    text(85, 598, L"STAGE " + std::to_wstring(g.profile.stage) + L"  ·  " + branchName(g.profile.branch) + L"  ·  XP " + std::to_wstring(g.profile.xp), Cyan, fontSmall);
    ship({1005, 364}, g.profile.branch, time, true, false, 4);
    line(800, 364, 902, 364, Cyan);
    for (int i = 0; i < 8; ++i) {
        float offset = std::fmod(time * 85 + i * 43, 300.f);
        circle({1045 + offset, 260 + i * 30.f}, static_cast<float>(4 + i % 3), Cyan, false);
    }
    text(85, 671, L"DxLib / C++  |  PLAYABLE PROTOTYPE 01", Muted, fontSmall);
}
void hangar(const Game& g, int selected, bool confirm) {
    background(g.clock);
    text(64, 48, L"機体進化 / HANGAR", White, fontTitle);
    text(65, 108, L"次の出撃 STAGE " + std::to_wstring(g.profile.stage) + L"    所持 XP " + std::to_wstring(g.profile.xp), Cyan);
    if (g.profile.branch == Branch::Base) {
        text(65, 167, L"初回進化：100 XP。選択した系統は変更できません。", Gold);
        const wchar_t* descriptions[] = {L"射撃・チャージの威力が上昇", L"吸収範囲が拡大、引き寄せが高速化", L"耐久力が上昇、Shift長押しでバリア"};
        const wchar_t* weaknesses[] = {L"通常射撃の消費エネルギーが増加", L"通常弾とチャージの威力は低下", L"バリア中は吸収・射撃不可、エネルギー消費"};
        for (int i = 0; i < 3; ++i) {
            Branch b = static_cast<Branch>(i + 1);
            int x = 65 + i * 402;
            if (selected == i + 1) { box(x - 10, 225, 380, 260, GetColor(19, 33, 49)); line(static_cast<float>(x - 10), 225, static_cast<float>(x + 370), 225, branchColor(b), 3); }
            ship({static_cast<float>(x + 155), 285}, b, g.clock, false, false, 2);
            text(x, 346, std::to_wstring(i + 1) + L"  " + branchName(b), branchColor(b));
            text(x, 396, descriptions[i], White, fontSmall);
            text(x, 433, weaknesses[i], Muted, fontSmall);
        }
        if (confirm) {
            centered(536, branchName(static_cast<Branch>(selected)) + L"へ進化しますか？  Enter 確定 / Esc 戻る", Gold);
        } else {
            centered(536, L"1 / 2 / 3 で選択 → Enter で確認", Gold);
            centered(581, L"Space：進化を保留して出撃", Muted, fontSmall);
        }
    } else {
        ship({640, 300}, g.profile.branch, g.clock, false, false, 4);
        centered(412, branchName(g.profile.branch) + L" / RANK " + std::to_wstring(g.profile.rank), branchColor(g.profile.branch), fontTitle);
        centered(473, L"同じ系統で成長します。別系統への変更はできません。", Muted, fontSmall);
        centered(528, L"Enter：次のステージへ出撃", Gold);
        centered(577, g.profile.rank < 3 ? L"U：同系統を強化（" + std::to_wstring(100 * (g.profile.rank + 1)) + L" XP）" : L"試作版の最大ランクに到達", Cyan, fontSmall);
    }
    centered(665, L"進化・強化・次ステージの難易度は試遊用の仮設定です。", Muted, fontSmall);
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int) {
    const std::string args = commandLine;
    bool smoke = args.find("--smoke") != std::string::npos;
    noSave = smoke;
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    savePath = std::filesystem::path(exe).parent_path() / L"boashot-save.txt";
    ChangeWindowMode(TRUE);
    SetGraphMode(1280, 720, 32);
    SetMainWindowText(L"ボアショット / BOASHOT — DxLib Prototype");
    SetOutApplicationLogValidFlag(FALSE);
    SetAlwaysRunFlag(TRUE);
    if (smoke) SetWaitVSyncFlag(FALSE);
    if (DxLib_Init() < 0) return 1;
    SetDrawScreen(DX_SCREEN_BACK);
    SetBackgroundColor(7, 13, 27);
    fontSmall = CreateFontToHandle(L"Yu Gothic UI", 17, 4, DX_FONTTYPE_ANTIALIASING_4X4);
    fontBody = CreateFontToHandle(L"Yu Gothic UI", 23, 5, DX_FONTTYPE_ANTIALIASING_4X4);
    fontTitle = CreateFontToHandle(L"Yu Gothic UI", 34, 7, DX_FONTTYPE_ANTIALIASING_4X4);
    fontHero = CreateFontToHandle(L"Bahnschrift", 104, 9, DX_FONTTYPE_ANTIALIASING_4X4);
    Game game;
    if (!smoke) load(game.profile);
    std::array<char, 256> keys{}, previous{};
    bool running = true, options = false, confirm = false, focused = true;
    int selected = 1, frames = 0, previousMouse = 0;
    float menuTime = 0;
    auto then = std::chrono::steady_clock::now();
    if (smoke) game.start();
    while (running && ProcessMessage() == 0) {
        auto now = std::chrono::steady_clock::now();
        float dt = clamp(std::chrono::duration<float>(now - then).count(), 0, .05f); then = now;
        menuTime += dt;
        GetHitKeyStateAll(keys.data());
        int mouse = GetMouseInput();
        if (mouse < 0) mouse = 0;
        auto pressed = [&](int k) { return keys[k] && !previous[k]; };
        auto released = [&](int k) { return !keys[k] && previous[k]; };
        auto mousePressed = [&](int button) { return (mouse & button) && !(previousMouse & button); };
        auto mouseReleased = [&](int button) { return !(mouse & button) && (previousMouse & button); };
        if (!smoke && GetActiveFlag() == 0) {
            if (game.phase == Phase::Playing) { game.stopActions(); game.phase = Phase::Paused; }
            focused = false; previous = keys; previousMouse = mouse; WaitTimer(20); continue;
        }
        if (!focused) { previous = keys; previousMouse = mouse; focused = true; }
        bool consumed = false;
        if (pressed(KEY_INPUT_F1)) {
            options = !options; game.stopActions(); consumed = true;
        }
        if (options) {
            if (pressed(KEY_INPUT_X)) { game.setToggle(!game.profile.toggleAbsorb); save(game.profile); }
            if (pressed(KEY_INPUT_ESCAPE)) { options = false; consumed = true; }
        } else if (!consumed) {
            if (game.phase == Phase::Title) {
                if (pressed(KEY_INPUT_RETURN)) {
                    if (game.profile.stage > 1 || game.profile.xp >= 100) game.phase = Phase::Hangar;
                    else game.start();
                    consumed = true;
                }
                if (pressed(KEY_INPUT_ESCAPE)) running = false;
            } else if (game.phase == Phase::Hangar) {
                if (game.profile.branch == Branch::Base) {
                    if (!confirm) {
                        if (pressed(KEY_INPUT_1)) selected = 1;
                        if (pressed(KEY_INPUT_2)) selected = 2;
                        if (pressed(KEY_INPUT_3)) selected = 3;
                        if (pressed(KEY_INPUT_RETURN) && game.profile.xp >= 100) confirm = true;
                        if (pressed(KEY_INPUT_SPACE)) game.start();
                    } else {
                        if (pressed(KEY_INPUT_ESCAPE)) confirm = false;
                        else if (pressed(KEY_INPUT_RETURN)) { game.evolve(static_cast<Branch>(selected)); save(game.profile); confirm = false; }
                    }
                } else {
                    if (pressed(KEY_INPUT_U) && game.upgrade()) save(game.profile);
                    if (pressed(KEY_INPUT_RETURN)) game.start();
                }
                consumed = true;
            } else if (game.phase == Phase::Paused) {
                if (pressed(KEY_INPUT_RETURN) || pressed(KEY_INPUT_P) || pressed(KEY_INPUT_ESCAPE)) { game.phase = Phase::Playing; consumed = true; }
                if (pressed(KEY_INPUT_Q)) { game.stopActions(); game.phase = Phase::Title; }
            } else if (game.phase == Phase::Clear) {
                if (pressed(KEY_INPUT_RETURN)) { game.phase = Phase::Hangar; confirm = false; }
            } else if (game.phase == Phase::GameOver) {
                if (pressed(KEY_INPUT_RETURN)) { game.start(); consumed = true; }
                if (pressed(KEY_INPUT_ESCAPE)) game.phase = Phase::Title;
            } else if (pressed(KEY_INPUT_ESCAPE) || pressed(KEY_INPUT_P)) {
                game.stopActions(); game.phase = Phase::Paused;
            }
        }
        if (game.phase == Phase::Playing && !options && !consumed) {
            Input input;
            input.move = {static_cast<float>((keys[KEY_INPUT_D] || keys[KEY_INPUT_RIGHT]) - (keys[KEY_INPUT_A] || keys[KEY_INPUT_LEFT])),
                static_cast<float>((keys[KEY_INPUT_S] || keys[KEY_INPUT_DOWN]) - (keys[KEY_INPUT_W] || keys[KEY_INPUT_UP]))};
            input.absorbPress = mousePressed(MOUSE_INPUT_RIGHT); input.absorbRelease = mouseReleased(MOUSE_INPUT_RIGHT);
            input.firePress = mousePressed(MOUSE_INPUT_LEFT); input.fireRelease = mouseReleased(MOUSE_INPUT_LEFT);
            input.chargePress = pressed(KEY_INPUT_SPACE); input.chargeRelease = released(KEY_INPUT_SPACE);
            input.barrier = keys[KEY_INPUT_LSHIFT] || keys[KEY_INPUT_RSHIFT];
            if (smoke) {
                dt = 1.f / 60;
                input = {};
                if (frames == 0) input.absorbPress = true;
                if (frames == 600) input.chargePress = true;
                if (frames == 720) input.chargeRelease = true;
                if (frames == 750) input.absorbPress = true;
            }
            Phase before = game.phase; game.update(dt, input);
            if (before == Phase::Playing && game.phase == Phase::Clear) save(game.profile);
        }
        if (game.phase == Phase::Title) title(game, menuTime);
        else if (game.phase == Phase::Hangar) hangar(game, selected, confirm);
        else {
            drawGame(game);
            if (game.phase == Phase::Paused) {
                veil(); centered(265, L"PAUSED", White, fontHero);
                centered(420, L"Enter / P：再開     F1：設定     Q：タイトル", Gold);
            } else if (game.phase == Phase::Clear) {
                veil(); centered(200, L"STAGE CLEAR", Cyan, fontHero);
                centered(345, L"獲得経験値  +100 XP", Gold, fontTitle);
                centered(415, L"撃破 " + std::to_wstring(game.stats.kills) + L"   回収 " + std::to_wstring(game.stats.absorbed) + L"   獲得エネルギー " + number(game.stats.energyGained), Muted);
                centered(470, L"最大チャージ  " + number(game.stats.biggestCharge), Gold);
                centered(540, L"Enter：機体進化へ", White);
            } else if (game.phase == Phase::GameOver) {
                veil(); centered(240, L"GAME OVER", Red, fontHero);
                centered(415, L"Enter：同じステージを再挑戦    Esc：タイトル", Gold);
                centered(465, L"進化と獲得済み経験値は維持されます。", Muted);
            }
        }
        settings(game, options);
        if (saveFailed) centered(3, L"保存データを読み書きできません。保存先・内容を確認してください。", Red, fontSmall);
        if (smoke) {
            if (frames == 540 && SaveDrawScreenToPNG(0, 0, 1280, 720, L"smoke-absorb.png") < 0) return 2;
            if (frames == 720 && SaveDrawScreenToPNG(0, 0, 1280, 720, L"smoke-charge.png") < 0) return 2;
            if (frames == 2900) {
                game.start(); game.clock = 49; game.bossSpawned = true;
                game.spawnEnemy(EnemyKind::Boss, {1080, 380}); game.absorbing = true;
                for (int i = 0; i < 4; ++i) { game.enemies.front().age = i * .65f; game.enemyFire(game.enemies.front()); }
            }
            if (frames == 2930) { SaveDrawScreenToPNG(0, 0, 1280, 720, L"smoke-boss.png"); }
            if (frames == 2940) { game.phase = Phase::Hangar; game.profile.xp = 100; }
            if (frames == 2941) SaveDrawScreenToPNG(0, 0, 1280, 720, L"smoke-hangar.png");
            if (frames > 2942) running = false;
            ++frames;
        }
        previous = keys;
        previousMouse = mouse;
        ScreenFlip();
        if (!smoke) WaitTimer(1);
    }
    DeleteFontToHandle(fontSmall); DeleteFontToHandle(fontBody); DeleteFontToHandle(fontTitle); DeleteFontToHandle(fontHero);
    DxLib_End();
    return 0;
}
