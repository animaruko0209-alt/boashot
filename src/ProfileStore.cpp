#include "ProfileStore.h"

#include <Windows.h>
#include <fstream>
#include <string>

namespace boa {

ProfileStore::ProfileStore() {
    // 起動したフォルダに関係なく、実行ファイルの場所を基準にする。
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    savePath = std::filesystem::path(exe).parent_path() / L"boashot-save.txt";
}

void ProfileStore::save(const Profile& profile) {
    auto temp = savePath;
    temp += L".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << "BOASHOT_2\n" << profile.stage << '\n';
        out.close();
        if (!out) {
            saveFailed = true;
            return;
        }
    }

    // 書き込みに成功した一時ファイルでセーブ本体を置き換える。
    saveFailed = !MoveFileExW(
        temp.c_str(), savePath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

void ProfileStore::load(Profile& profile) {
    std::ifstream in(savePath);
    if (!in)
        return;

    std::string version;
    int stage = 0;
    in >> version;
    if (version == "BOASHOT_2") {
        in >> stage;
    } else if (version == "BOASHOT_1") {
        // 旧形式の項目を読み取り、ステージ番号だけを引き継ぐ。
        int xp = 0, rank = 0, branch = 0, toggle = 0;
        in >> xp >> stage >> rank >> branch >> toggle;
    }

    if (in && stage >= 1 && stage <= 99999)
        profile.stage = stage;
    else
        saveFailed = true;
}

} // namespace boa
