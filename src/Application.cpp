#include "Application.h"

#include "DxLib.h"

#include <chrono>
#include <string>

namespace boa {

int Application::run(const char* commandLine) {
    const std::string args = commandLine;
    const bool smoke = args.find("--smoke") != std::string::npos;

    ChangeWindowMode(TRUE);
    SetGraphMode(1280, 720, 32);
#ifdef _DEBUG
    // DxLib は同じウィンドウ名の二重起動を防ぐため、Debug 版には専用の名前を付ける。
    SetMainWindowText(L"ボアショット / BOASHOT [Debug]");
#else
    SetMainWindowText(L"ボアショット / BOASHOT");
#endif
    SetOutApplicationLogValidFlag(FALSE);
    SetAlwaysRunFlag(TRUE);
    if (smoke)
        SetWaitVSyncFlag(FALSE);
    if (DxLib_Init() < 0)
        return 1;

    SetDrawScreen(DX_SCREEN_BACK);
    SetBackgroundColor(7, 13, 27);
    renderer.createFonts();

    // 自動描画はセーブを読み込む前に分岐し、DxLib の終了処理でフォントも解放する。
    if (smoke) {
        const bool passed = runSmoke();
        DxLib_End();
        return passed ? 0 : 2;
    }

    play();
    renderer.deleteFonts();
    DxLib_End();
    return 0;
}

void Application::play() {
    profileStore.load(game.profile);
    bool running = true;
    bool focused = true;
    float menuTime = 0;
    auto then = std::chrono::steady_clock::now();

    while (running && ProcessMessage() == 0) {
        const auto now = std::chrono::steady_clock::now();
        const float dt = clamp(std::chrono::duration<float>(now - then).count(), 0, .05f);
        then = now;
        menuTime += dt;
        GetHitKeyStateAll(keys.data());
        const int mouseButtons = std::max(0, GetMouseInput());
        // 毎フレーム読み取ってカウンターをリセットし、ポーズ中の回転も持ち越さない。
        int wheelSteps = GetMouseWheelRotVol();

        if (!GetActiveFlag()) {
            // フォーカスを失ったらアクションを終了し、戦闘を一時停止する。
            if (game.phase == Phase::Playing) {
                game.stopActions();
                game.phase = Phase::Paused;
            }
            focused = false;
            WaitTimer(20);
            continue;
        }
        if (!focused) {
            // フォーカス復帰時に、押しっぱなしを新たな押下として扱わない。
            previousKeys = keys;
            previousMouseButtons = mouseButtons;
            wheelSteps = 0;
            focused = true;
        }

        running = updateFrame(dt, mouseButtons, wheelSteps);
        renderer.drawScene(game, menuTime, profileStore.failed());
        previousKeys = keys;
        previousMouseButtons = mouseButtons;
        ScreenFlip();
        WaitTimer(1);
    }
}

bool Application::updateFrame(float dt, int mouseButtons, int wheelSteps) {
    const bool enter = pressed(KEY_INPUT_RETURN);
    const bool escape = pressed(KEY_INPUT_ESCAPE);
    const bool pause = pressed(KEY_INPUT_P);

    // 出撃や再開のフレームは画面遷移だけを処理し、次のフレームから戦闘入力を受け取る。
    switch (game.phase) {
    case Phase::Title:
        if (escape)
            return false;
        if (enter)
            game.start();
        break;
    case Phase::Paused:
        if (pressed(KEY_INPUT_Q))
            game.phase = Phase::Title;
        else if (enter || pause || escape)
            game.phase = Phase::Playing;
        break;
    case Phase::Clear:
    case Phase::GameOver:
        if (escape)
            game.phase = Phase::Title;
        else if (enter)
            game.start();
        break;
    case Phase::Playing:
        if (pause || escape) {
            game.stopActions();
            game.phase = Phase::Paused;
        } else {
            game.update(dt, readCombatInput(mouseButtons, wheelSteps));
            // 戦闘からクリアに切り替わったフレームで、次の出撃先を保存する。
            if (game.phase == Phase::Clear)
                profileStore.save(game.profile);
        }
        break;
    }

    return true;
}

bool Application::pressed(int key) const {
    return keys[key] && !previousKeys[key];
}

bool Application::released(int key) const {
    return !keys[key] && previousKeys[key];
}

Input Application::readCombatInput(int mouseButtons, int wheelSteps) const {
    Input input;
    // 移動は押している間、アクションは押した瞬間と離した瞬間を Game に渡す。
    input.move = {
        static_cast<float>(
            (keys[KEY_INPUT_D] || keys[KEY_INPUT_RIGHT]) -
            (keys[KEY_INPUT_A] || keys[KEY_INPUT_LEFT])),
        static_cast<float>(
            (keys[KEY_INPUT_S] || keys[KEY_INPUT_DOWN]) -
            (keys[KEY_INPUT_W] || keys[KEY_INPUT_UP]))
    };

    if (wheelSteps != 0)
        input.selection = cycleAction(game.selected, wheelSteps);

    const bool rightHeld = (mouseButtons & MOUSE_INPUT_RIGHT) != 0;
    const bool rightWasHeld = (previousMouseButtons & MOUSE_INPUT_RIGHT) != 0;
    input.absorbPress = rightHeld && !rightWasHeld;
    input.absorbRelease = !rightHeld && rightWasHeld;
    input.normalShotHeld = (mouseButtons & MOUSE_INPUT_LEFT) != 0;
    input.actionPress = pressed(KEY_INPUT_SPACE);
    input.actionRelease = released(KEY_INPUT_SPACE);
    input.skillPress = pressed(KEY_INPUT_I);
    return input;
}

} // namespace boa
