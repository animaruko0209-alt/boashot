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
            // 復帰時のキー状態を基準にし、押し直したときに押下を検出する。
            previousKeys = keys;
            focused = true;
        }

        running = updateFrame(dt);
        renderer.drawScene(game, menuTime, profileStore.failed());
        previousKeys = keys;
        ScreenFlip();
        WaitTimer(1);
    }
}

bool Application::updateFrame(float dt) {
    const bool enter = pressed(KEY_INPUT_RETURN);
    const bool escape = pressed(KEY_INPUT_ESCAPE);
    const bool pause = pressed(KEY_INPUT_P);

    // Enterの用途を画面ごとに分け、出撃や再開と同じフレームで攻撃しないようにする。
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
            game.update(dt, readCombatInput());
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

Input Application::readCombatInput() const {
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

    // 同じフレームに複数の選択キーを押した場合は、後のキーを優先する。
    if (pressed(KEY_INPUT_J))
        input.selection = Action::Charge;
    if (pressed(KEY_INPUT_K))
        input.selection = Action::Grenade;
    if (pressed(KEY_INPUT_L))
        input.selection = Action::Barrier;

    input.absorbPress = pressed(KEY_INPUT_SPACE);
    input.absorbRelease = released(KEY_INPUT_SPACE);
    input.actionPress = pressed(KEY_INPUT_RETURN);
    input.actionRelease = released(KEY_INPUT_RETURN);
    input.skillPress = pressed(KEY_INPUT_I);
    return input;
}

} // namespace boa
