// Card server status for the Play page chip: is the server in the card settings (local or remote) answering?
// Included by main.cpp, not a separate unit.
//
// A background thread asks the configured address (SnowLauncher.ini Game_Config Server, else config.ini) for its
// home page every 10 seconds; any HTTP answer counts as online (a card server's root may answer 404). Clicking the
// chip opens that page in the browser (Server-Infinite has its web panel there).

struct CardServerState { std::string addr, url; bool local = false; int state = 0; int ms = 0; };   // state: 0 not set, 1 checking, 2 online, 3 offline
static std::mutex g_card_mx;
static CardServerState g_card;
static HANDLE g_card_wake;

// "card.example.com/", "127.0.0.1:28080", "https://x" -> URL with a scheme; local = this PC
static std::string card_url(std::string a, bool *local) {
    a = trim(a);
    if (a.empty()) return "";
    std::string low = a; for (auto &c : low) c = (char)tolower((unsigned char)c);
    if (low.rfind("http://", 0) != 0 && low.rfind("https://", 0) != 0) a = "http://" + a, low = "http://" + low;
    size_t h = low.find("://") + 3, e = low.find_first_of(":/", h);
    std::string host = low.substr(h, e == std::string::npos ? std::string::npos : e - h);
    *local = host == "127.0.0.1" || host == "localhost" || host == "[::1]";
    if (a.back() != '/' && a.find('/', h) == std::string::npos) a += "/";
    return a;
}

// any HTTP answer within the timeout = online; returns the round trip in ms or -1
static int card_probe(const std::string &url_utf8) {
    std::wstring url = widen(url_utf8);
    wchar_t host[256] = L"", path[1024] = L"";
    URL_COMPONENTS uc = { sizeof(uc) };
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return -1;
    ULONGLONG t0 = GetTickCount64();
    HINTERNET s = WinHttpOpen((L"EXVS-Launcher/" + widen(VERSION)).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!s) s = WinHttpOpen(L"EXVS-Launcher", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!s) return -1;
    WinHttpSetTimeouts(s, 3000, 3000, 3000, 3000);
    int ms = -1;
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0), r = NULL;
    if (c) r = WinHttpOpenRequest(c, L"GET", path[0] ? path : L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (r) {
        // only "is it up?": a self-signed certificate of a group's server still counts
        DWORD f = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(r, WINHTTP_OPTION_SECURITY_FLAGS, &f, sizeof(f));
        if (WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, NULL))
            ms = (int)(GetTickCount64() - t0);
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ms;
}

static std::string card_server_setting() {
    Ini sl(L"SnowLauncher.ini"), cfg(L"config.ini");
    return sl.get("Game_Config", "Server", cfg.get("config", "Server").c_str());
}

static void card_thread() {
    std::string last_addr;
    int last_state = -1;
    for (;;) {
        std::string addr = card_server_setting();
        bool local = false;
        std::string url = card_url(addr, &local);
        if (addr != last_addr) {   // new address: show "checking" right away
            std::lock_guard<std::mutex> l(g_card_mx);
            g_card = { addr, url, local, url.empty() ? 0 : 1, 0 };
            last_addr = addr;
        }
        int state = 0, ms = 0;
        if (!url.empty()) { ms = card_probe(url); state = ms >= 0 ? 2 : 3; }
        {
            std::lock_guard<std::mutex> l(g_card_mx);
            if (g_card.addr == addr) { g_card.state = state; g_card.ms = ms; }
        }
        if (state != last_state) log_line("card server %s: %s", local ? "(local)" : "(remote)", state == 2 ? "online" : state == 3 ? "not answering" : "not set");
        last_state = state;
        WaitForSingleObject(g_card_wake, state == 3 ? 5000 : 10000);   // check again sooner while it's down
    }
}

static void card_start() {
    static bool started;
    if (started) return;
    started = true;
    g_card_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    std::thread(card_thread).detach();
}
static void card_check_now() { if (g_card_wake) SetEvent(g_card_wake); }

static json card_status() {
    std::lock_guard<std::mutex> l(g_card_mx);
    static const char *names[] = { "none", "checking", "online", "offline" };
    return { {"state", names[g_card.state]}, {"local", g_card.local}, {"ms", g_card.ms}, {"url", g_card.url} };
}

static json open_card_server() {
    std::string url;
    { std::lock_guard<std::mutex> l(g_card_mx); url = g_card.url; }
    if (url.empty()) return { {"ok", false}, {"error", "No card server address set (Settings › Account)"} };
    // called on a worker thread: ShellExecute needs COM there to hand a URL to the browser
    HRESULT co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INT_PTR r = (INT_PTR)ShellExecuteW(NULL, L"open", widen(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
    if (SUCCEEDED(co)) CoUninitialize();
    if (r <= 32) { log_line("card server page: ShellExecute failed (%d)", (int)r); return { {"ok", false}, {"error", "Couldn't open the browser"} }; }
    return { {"ok", true} };
}

// --card-test <game dir> <out.json>: the address from the card settings, the URL and one check (no window)
static int card_selftest(const wchar_t *dir, const wchar_t *out) {
    init_dir(dir);
    bool local = false;
    std::string addr = card_server_setting(), url = card_url(addr, &local);
    int ms = url.empty() ? -2 : card_probe(url);
    json r = { {"addr", addr}, {"url", url}, {"local", local}, {"ms", ms}, {"online", ms >= 0} };
    if (FILE *f = _wfopen(out, L"wb")) { std::string d = r.dump(2); fwrite(d.data(), 1, d.size(), f); fclose(f); }
    return 0;
}
