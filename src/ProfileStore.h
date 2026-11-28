#pragma once

#include "Game.h"

#include <filesystem>

namespace boa {

// 実行ファイルの隣に、次に出撃するステージ番号を保存する。
class ProfileStore {
public:
    ProfileStore();

    void load(Profile& profile);
    void save(const Profile& profile);
    bool failed() const { return saveFailed; }

private:
    std::filesystem::path savePath;
    bool saveFailed = false;
};

} // namespace boa
