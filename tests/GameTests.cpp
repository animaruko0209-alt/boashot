#include "../src/Game.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace boa;
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool near(float a, float b, float epsilon = .001f) { return std::abs(a - b) < epsilon; }
Bullet bullet(Vec p, float energy = 6) { Bullet b; b.p = p; b.velocity = {-100, 0}; b.energy = b.maxEnergy = energy; return b; }
void absorptionTests() {
    Game g; g.start(); g.energy = 0; g.absorbing = true;
    check(g.inCone(g.player + Vec{200, 0}), "front inside cone");
    check(!g.inCone(g.player + Vec{-1, 0}), "rear excluded");
    check(!g.inCone(g.player + Vec{50, 200}), "side excluded");
    g.bullets.push_back(bullet(g.player + Vec{200, 0}));
    g.updateBullets(.1f);
    check(near(g.energy, 1.1f), "energy available before arrival");
    check(near(g.bullets[0].energy, 4.9f), "siphoning conserves energy");
    check(g.bullets[0].alive, "not fully absorbed before arrival");
    float position = g.bullets[0].p.x;
    g.absorbing = false; g.updateBullets(.1f);
    check(near(g.bullets[0].p.x, position - 10), "release restores original velocity");
    check(near(g.bullets[0].energy, 4.9f), "release preserves weakening");
    g.bullets[0].p = g.player + Vec{16, 0}; g.bullets[0].velocity = {-10, 0};
    g.updateBullets(.1f);
    check(near(g.hp, 100 - 14 * 4.9f / 6), "partial bullet inflicts proportional damage when absorption off");
    g.start(); g.absorbing = true; g.bullets.push_back(bullet(g.player + Vec{18, 0}));
    g.updateBullets(.1f);
    check(!g.bullets[0].alive && near(g.hp, 100) && near(g.energy, 41), "active cone arrival safely collects remaining energy");
    g.start(); g.absorbing = true; g.bullets.push_back(bullet(g.player + Vec{0, 15}));
    g.updateBullets(.01f);
    check(g.hp < 100, "outside cone hits even while absorbing");
    g.start(); g.absorbing = true; auto rock = bullet(g.player + Vec{200, 0}); rock.rock = true;
    g.bullets.push_back(rock); g.updateBullets(.1f);
    check(near(g.energy, 35) && near(g.bullets[0].p.x, g.player.x + 190), "rocks cannot be siphoned or pulled");
    g.start(); g.energy = 0; g.absorbing = true; g.bullets.push_back(bullet(g.player + Vec{260, 0}, .1f));
    g.updateBullets(.1f);
    check(g.bullets[0].alive && near(g.bullets[0].energy, 0), "empty shell remains until arrival");
    g.absorbing = false; g.bullets[0].p = g.player; g.updateBullets(.001f);
    check(near(g.hp, 100) && g.bullets[0].alive, "empty shell harmless after release");
    Game many, one; many.start(); one.start(); many.absorbing = one.absorbing = true;
    many.energy = one.energy = 0;
    one.bullets.push_back(bullet(one.player + Vec{250, 0}));
    for (int i = 0; i < 10; ++i) many.bullets.push_back(bullet(many.player + Vec{250, 0}));
    many.updateBullets(.1f); one.updateBullets(.1f);
    check(many.bullets[0].p.x > one.bullets[0].p.x, "more bullets slow attraction");
    check(near(many.bullets[0].energy, one.bullets[0].energy), "bullet count does not reduce siphon rate");
    check(near(many.energy, one.energy * 10), "parallel energy acquisition");
    many.energy = many.capacity() - 2; many.charge = 2;
    float before = many.bullets[0].energy; many.updateBullets(.01f);
    check(near(many.bullets[0].energy, before), "full storage does not destroy energy while siphoning");
}
void controlTests() {
    Game g; g.start(); Input i; i.absorbPress = true; g.controls(i);
    check(g.absorbing, "hold starts absorption");
    i = {}; i.firePress = true; g.controls(i);
    check(g.firing && !g.absorbing, "fire overrides absorption");
    g.controls({}); check(!g.absorbing, "held old action cannot automatically resume");
    g.energy = 10; g.charge = 30;
    i = {}; i.absorbPress = true; g.controls(i);
    check(!g.firing && g.absorbing && near(g.energy, 40) && near(g.charge, 0), "absorb cancels charge with full refund");
    i = {}; i.fireRelease = true; g.controls(i);
    check(g.shots.empty() && g.absorbing, "releasing displaced fire cannot launch charge");
    g.setToggle(true); i = {}; i.absorbPress = true; g.controls(i);
    i = {}; i.absorbRelease = true; g.controls(i);
    check(g.absorbing, "toggle survives release");
    i = {}; i.absorbPress = true; g.controls(i); check(!g.absorbing, "toggle second press disables");
    g.setToggle(false); i = {}; i.absorbPress = true; g.controls(i);
    i = {}; i.absorbRelease = true; g.controls(i); check(!g.absorbing, "hold release disables");
    g.energy = 120; g.charge = 0; g.charging = true;
    for (int f = 0; f < 600; ++f) g.updateWeapons(1.f / 60);
    check(near(g.energy, 0) && near(g.charge, 120), "holding reserves all energy without normal-fire cost");
    check(g.shots.empty(), "holding charge never fires, even after energy is exhausted");
    float charge = g.charge;
    i = {}; i.chargeRelease = true; g.controls(i);
    check(near(g.charge, 0) && g.shots.back().charged && near(g.shots.back().damage, charge * 2.6f), "release consumes reservation and scales damage");
    g.start(); g.charge = 4; g.energy = 10; g.charging = true; g.controls(i);
    check(g.shots.empty() && near(g.energy, 14), "short Space release refunds reservation without a normal shot");
    g.controls(i);
    check(g.shots.empty() && near(g.energy, 14), "displaced release cannot spend energy");
    g.start(); i = {}; i.firePress = true; g.controls(i);
    g.updateWeapons(.01f);
    check(g.shots.size() == 1 && !g.shots[0].charged && near(g.charge, 0), "left press fires normal shot without charging");
    for (int frame = 0; frame < 100; ++frame) g.updateWeapons(.01f);
    check(g.shots.size() > 1 && near(g.charge, 0), "held left click repeats normal shots without charging");
    check(near(g.energy, 35 - 1.2f * static_cast<float>(g.shots.size())), "normal fire consumes only per-shot cost");
    auto count = g.shots.size();
    i = {}; i.fireRelease = true; g.controls(i);
    g.updateWeapons(.2f);
    check(g.shots.size() == count && !g.firing, "left release stops fire without a charged shot");
    g.start(); g.energy = 0; i = {}; i.firePress = true; g.controls(i);
    g.updateWeapons(1.f); i = {}; i.fireRelease = true; g.controls(i);
    check(g.shots.empty() && near(g.energy, 0), "empty-energy normal fire cannot fire for free");
    g.start(); i = {}; i.firePress = true; g.controls(i);
    i = {}; i.chargePress = true; g.controls(i);
    g.updateWeapons(.1f);
    check(g.charging && !g.firing && g.shots.empty() && near(g.charge, 2.8f), "Space switches from normal fire to charge only");
    i = {}; i.fireRelease = true; g.controls(i);
    check(g.charging, "left release cannot release Space charge");
    i = {}; i.firePress = true; g.controls(i);
    check(g.firing && !g.charging && near(g.charge, 0) && near(g.energy, 35), "left press cancels charge and refunds it");
    i = {}; i.chargeRelease = true; g.controls(i);
    check(g.firing && g.shots.empty(), "cancelled Space release cannot fire a charge shot");
    g.start(); i = {}; i.chargePress = true; g.controls(i); g.updateWeapons(.4f);
    i = {}; i.absorbPress = true; g.controls(i);
    check(g.absorbing && !g.charging && near(g.energy, 35), "absorption cancels Space charge and fully refunds");
    i = {}; i.chargeRelease = true; g.controls(i);
    check(g.shots.empty(), "Space release after absorption cannot fire");
}
void combatTests() {
    Game g; g.start();
    g.spawnEnemy(EnemyKind::Scout, {500, 350});
    g.spawnEnemy(EnemyKind::Scout, {520, 350});
    Shot s; s.p = s.previous = {450, 350}; s.charged = true; s.damage = 15; s.speed = 1500;
    g.shots.push_back(s); g.updateWeapons(.05f);
    check(g.enemies[0].hp < 22 && g.enemies[1].hp < 22, "charge pierces multiple enemies");
    float hp = g.enemies[1].hp; g.shots[0].speed = 0; g.updateWeapons(.01f);
    check(near(g.enemies[1].hp, hp), "piercing shot cannot repeatedly damage same enemy");
    g.start(); g.profile.branch = Branch::Defense; g.energy = 30;
    auto b = bullet(g.player + Vec{40, 0}); b.rock = true; g.bullets.push_back(b);
    Input i; i.barrier = true; g.update(.01f, i);
    check(g.bullets.empty() && near(g.hp, 100) && g.energy < 30, "defense barrier blocks rocks and consumes energy");
    g.start(); g.profile.branch = Branch::Attack; g.update(.01f, i);
    check(!g.barrierActive, "barrier exclusive to defense branch");
    g.start(); g.spawnEnemy(EnemyKind::Boss, {1080, 380});
    g.enemies[0].age = 2; g.enemyFire(g.enemies[0]); check(!g.bullets.empty(), "boss attack phase supplies bullets");
    g.bullets.clear(); g.enemies[0].age = 6; g.enemyFire(g.enemies[0]);
    check(g.bullets.empty(), "boss counterattack window stops firing");
    // Simulate an entire stage, use real shots to defeat each enemy and the boss.
    // Invulnerability isolates scheduling, collision, clear and reward transitions.
    g = Game{}; g.start(); bool sawBoss = false;
    for (int frame = 0; frame < 6000 && g.phase == Phase::Playing; ++frame) {
        g.invulnerable = 1;
        for (const auto& e : g.enemies) if (e.alive) {
            Shot hit; hit.p = hit.previous = {e.p.x - 10, e.p.y}; hit.speed = 1000; hit.damage = 2000;
            g.shots.push_back(hit);
            if (e.kind == EnemyKind::Boss) sawBoss = true;
        }
        g.update(1.f / 60, {});
    }
    check(sawBoss && g.phase == Phase::Clear, "stage waves lead to boss and real kill leads to clear");
    check(g.profile.xp == 100 && g.profile.stage == 2, "clear awards XP and unlocks next stage");
    g.complete(); check(g.profile.xp == 100, "clear reward cannot be claimed twice");
}
void progressionTests() {
    Game g; g.start(); g.stats.kills = 20;
    check(g.profile.xp == 0, "kills do not grant persistent XP");
    check(!g.evolve(Branch::Defense), "evolution requires clear XP");
    g.profile.xp = 100; check(g.evolve(Branch::Defense), "clear XP unlocks branch");
    check(g.profile.xp == 0 && g.profile.rank == 1, "evolution consumes XP");
    g.profile.xp = 1000;
    check(!g.evolve(Branch::Attack) && g.profile.branch == Branch::Defense, "branch is permanent");
    check(g.upgrade() && g.profile.rank == 2, "same branch can grow");
    g.energy = 0; g.charge = 80; g.hp = 1; g.start();
    check(near(g.energy, 35) && near(g.charge, 0) && near(g.hp, g.maxHp()), "stage resets resources and health");
    check(g.profile.branch == Branch::Defense && g.profile.rank == 2, "evolution survives restart");
    g.hp = 0; g.update(.01f, {});
    check(g.phase == Phase::GameOver && g.profile.xp == 800, "failure grants no XP and preserves progression");
}
int main() {
    try { absorptionTests(); controlTests(); combatTests(); progressionTests(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return EXIT_FAILURE; }
    std::cout << "PASS: " << checks << " gameplay checks\n";
    return EXIT_SUCCESS;
}
