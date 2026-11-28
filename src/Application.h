#pragma once

#include "Game.h"
#include "ProfileStore.h"
#include "Renderer.h"

#include <array>

namespace boa {

// Windows版の起動、入力、画面遷移を管理する。戦闘の進行は Game に任せる。
class Application {
public:
    int run(const char* commandLine);

private:
    Game game;
    Renderer renderer;
    ProfileStore profileStore;

    std::array<char, 256> keys{};
    std::array<char, 256> previousKeys{};
    int previousMouseButtons = 0;

    void play();
    bool updateFrame(float dt, int mouseButtons, int wheelSteps);
    bool pressed(int key) const;
    bool released(int key) const;
    Input readCombatInput(int mouseButtons, int wheelSteps) const;

    // 固定入力で各場面を描く。通常プレイ用のセーブは読み書きしない。
    bool runSmoke();
};

} // namespace boa
