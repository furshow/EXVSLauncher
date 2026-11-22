// Replays on every machine (Settings > Gameplay > Replays), included from main.cpp after training.h.
//
// The game keeps a match replay only on the "elected" machine, which sends it to the Live Monitor. M1kou's
// "EXVS multi-function patch" (20261028, shipped unmodified with the author's permission, renamed from
// XINPUT9_1_0.dll) lets the other machines keep it too. Turning it on writes <game>\Replay\ (EXVSReplay.dll, its
// config xinputPatchConfig.ini with its input blocking and time rental bypass off, README.txt, MinHook-LICENSE.txt);
// the EXVS Overlay (REPLAY_OVERLAY_MIN or newer) loads the DLL when the game starts. Turning it off removes the
// DLL only (renamed to EXVSReplay.old.dll while a running game has it loaded; the overlay deletes that file).
#pragma once

static const char *REPLAY_OVERLAY_MIN = "1.2.4";

static std::wstring replay_dir() { return g_dir + L"Replay\\"; }

static json replay_get_state() {
    return { {"enabled", exists(replay_dir() + L"EXVSReplay.dll")}, {"overlayMin", REPLAY_OVERLAY_MIN} };
}

static json replay_set(bool on) {
    if (on) {
        CreateDirectoryW(replay_dir().c_str(), NULL);
        struct { const wchar_t *res, *file; bool keep; } files[] = {
            { L"REPLAY_DLL", L"EXVSReplay.dll", false }, { L"REPLAY_CONFIG", L"xinputPatchConfig.ini", true },
            { L"REPLAY_README", L"README.txt", false }, { L"REPLAY_MINHOOK", L"MinHook-LICENSE.txt", false } };
        for (auto &x : files) {
            std::wstring dst = replay_dir() + x.file;
            if (x.keep && exists(dst)) continue;   // the config may have been edited by hand
            if (!training_write_resource(x.res, dst)) {
                if (!FindResourceW(NULL, x.res, MAKEINTRESOURCEW(10))) return { {"ok", false}, {"error", "This launcher build doesn't include the replay patch"} };
                return { {"ok", false}, {"error", "Could not write Replay\\" + narrow(x.file)} };
            }
        }
        DeleteFileW((replay_dir() + L"EXVSReplay.old.dll").c_str());
        log_line("replays on every machine: on (Replay\\EXVSReplay.dll)");
    } else {
        std::wstring dll = replay_dir() + L"EXVSReplay.dll", old = replay_dir() + L"EXVSReplay.old.dll";
        if (exists(dll) && !DeleteFileW(dll.c_str())) {   // loaded by a running game: move it aside
            DeleteFileW(old.c_str());
            if (!MoveFileW(dll.c_str(), old.c_str())) return { {"ok", false}, {"error", "Could not remove Replay\\EXVSReplay.dll (close the game first)"} };
        }
        log_line("replays on every machine: off");
    }
    return { {"ok", true}, {"enabled", on} };
}

// --replay-test <game dir> <out.json>: on, edited config kept, off, on again; starts nothing
static int replay_selftest(const wchar_t *dir, const wchar_t *out) {
    init_dir(dir);
    json r;
    r["before"] = replay_get_state();
    r["on"] = replay_set(true);
    json files = json::object();
    for (const wchar_t *f : { L"EXVSReplay.dll", L"xinputPatchConfig.ini", L"README.txt", L"MinHook-LICENSE.txt" }) {
        WIN32_FILE_ATTRIBUTE_DATA fa = {};
        files[narrow(f)] = GetFileAttributesExW((replay_dir() + f).c_str(), GetFileExInfoStandard, &fa) ? (long long)fa.nFileSizeLow : -1;
    }
    r["files"] = files;
    r["inputFix"] = (int)GetPrivateProfileIntW(L"InputFix", L"Enabled", -1, (replay_dir() + L"xinputPatchConfig.ini").c_str());
    r["timeRental"] = (int)GetPrivateProfileIntW(L"TimeRental", L"BypassTimeRentalFreePlay", -1, (replay_dir() + L"xinputPatchConfig.ini").c_str());
    WritePrivateProfileStringW(L"Replay", L"ReplayDiagnostics", L"1", (replay_dir() + L"xinputPatchConfig.ini").c_str());
    r["off"] = replay_set(false);
    r["afterOff"] = replay_get_state();
    r["on2"] = replay_set(true);
    r["configKept"] = (int)GetPrivateProfileIntW(L"Replay", L"ReplayDiagnostics", -1, (replay_dir() + L"xinputPatchConfig.ini").c_str());
    r["afterOn2"] = replay_get_state();
    FILE *f = _wfopen(out, L"wb"); if (!f) return 1;
    std::string s = r.dump(2); fwrite(s.data(), 1, s.size(), f); fclose(f);
    return 0;
}
