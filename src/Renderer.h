#pragma once

#include "Game.h"

#include <string>

namespace boa {

// Game の状態を読み取り、DxLib のバックバッファーに1フレーム分を描く。
class Renderer {
public:
    // フォントハンドルは DxLib の初期化後に作成し、終了処理の前に解放する。
    void createFonts();
    void deleteFonts();
    void drawScene(const Game& g, float menuTime, bool saveFailed) const;

private:
    int fontSmall = -1, fontBody = -1, fontTitle = -1, fontHero = -1;

    void text(int x, int y, const std::wstring& value, unsigned color, int font = -1) const;
    void centered(int y, const std::wstring& value, unsigned color, int font = -1) const;
    void title(const Game& g, float time) const;
    void drawGame(const Game& g) const;
    void drawHud(const Game& g) const;
    void enemySprite(const Enemy& e, const Game& g) const;
};

} // namespace boa
