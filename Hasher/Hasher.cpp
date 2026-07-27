#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#if defined(_M_X64)
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined(_M_IX86)
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// ---------------------------------------------------------------------------
// Constants & custom messages
// ---------------------------------------------------------------------------

constexpr UINT WM_APP_PROGRESS = WM_APP + 1;
constexpr UINT WM_APP_DONE     = WM_APP + 2;

constexpr int IDC_INPUT_EDIT   = 1001;
constexpr int IDC_INPUT_BROWSE = 1002;
constexpr int IDC_OUTPUT_EDIT  = 1003;
constexpr int IDC_OUTPUT_BROWSE= 1004;
constexpr int IDC_START        = 1005;
constexpr int IDC_CANCEL       = 1006;
constexpr int IDC_OPEN_FOLDER  = 1007;
constexpr int IDC_PROGRESS     = 1008;
constexpr int IDC_STATUS       = 1009;
constexpr int IDC_COUNTER      = 1010;
constexpr int IDC_LOG          = 1011;

constexpr DWORD HASH_BUFFER_SIZE  = 32;
constexpr DWORD READ_BUFFER_SIZE  = 1024 * 1024;

// Dark theme palette
constexpr COLORREF CLR_BG        = RGB(30,  30,  34);
constexpr COLORREF CLR_PANEL     = RGB(42,  42,  48);
constexpr COLORREF CLR_TEXT      = RGB(230, 230, 235);
constexpr COLORREF CLR_MUTED     = RGB(150, 150, 160);
constexpr COLORREF CLR_ACCENT    = RGB(72,  149, 239);
constexpr COLORREF CLR_EDIT_BG   = RGB(24,  24,  28);
constexpr COLORREF CLR_LOG_BG    = RGB(20,  20,  24);

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

HWND g_hWnd          = nullptr;
HWND g_hInputEdit    = nullptr;
HWND g_hOutputEdit   = nullptr;
HWND g_hProgress     = nullptr;
HWND g_hStatus       = nullptr;
HWND g_hCounter      = nullptr;
HWND g_hLog          = nullptr;
HWND g_hStartBtn     = nullptr;
HWND g_hCancelBtn    = nullptr;
HWND g_hOpenFolderBtn= nullptr;

HFONT g_hFont        = nullptr;
HFONT g_hFontBold    = nullptr;
HBRUSH g_hBgBrush    = nullptr;
HBRUSH g_hPanelBrush = nullptr;
HBRUSH g_hEditBrush  = nullptr;
HBRUSH g_hLogBrush   = nullptr;

std::atomic<bool> g_cancel{ false };
std::thread       g_worker;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    return out;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

std::wstring GetWindowTextStr(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring buf(static_cast<size_t>(len + 1), L'\0');
    GetWindowTextW(h, &buf[0], len + 1);
    buf.resize(len);
    return buf;
}

void SetDlgText(HWND h, const std::wstring& text) {
    SetWindowTextW(h, text.c_str());
}

void AppendLog(const std::wstring& line) {
    if (!g_hLog) return;
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    std::wstring msg = line + L"\r\n";
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(msg.c_str()));
    SendMessageW(g_hLog, EM_SCROLLCARET, 0, 0);
}

void EnableControls(bool working) {
    EnableWindow(g_hInputEdit,     !working);
    EnableWindow(g_hOutputEdit,    !working);
    EnableWindow(GetDlgItem(g_hWnd, IDC_INPUT_BROWSE),  !working);
    EnableWindow(GetDlgItem(g_hWnd, IDC_OUTPUT_BROWSE), !working);
    EnableWindow(g_hStartBtn,      !working);
    EnableWindow(g_hCancelBtn,     working);
    EnableWindow(g_hOpenFolderBtn, !working);
}

std::wstring BrowseForFolder(HWND owner, const std::wstring& title) {
    std::wstring result;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return result;

    DWORD options = 0;
    dlg->GetOptions(&options);
    dlg->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(title.c_str());

    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::wstring BrowseForSaveFile(HWND owner) {
    std::wstring result;
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return result;

    COMDLG_FILTERSPEC filter[] = { { L"JSON files", L"*.json" }, { L"All files", L"*.*" } };
    dlg->SetFileTypes(2, filter);
    dlg->SetDefaultExtension(L"json");
    dlg->SetTitle(L"Save hash manifest");

    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

// ---------------------------------------------------------------------------
// Hashing logic
// ---------------------------------------------------------------------------

struct CryptoHandle {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    ~CryptoHandle() {
        if (hHash) CryptDestroyHash(hHash);
        if (hProv) CryptReleaseContext(hProv, 0);
    }
};

struct FileInfo {
    std::wstring name;
    std::string  hash;
    DWORD        size = 0;
};

std::string CalculateFileHash(const std::wstring& filePath) {
    CryptoHandle crypto;
    BYTE rgbHash[HASH_BUFFER_SIZE];
    DWORD cbHash = HASH_BUFFER_SIZE;
    const char digits[] = "0123456789abcdef";
    std::ostringstream hashStream;

    if (!CryptAcquireContext(&crypto.hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return {};

    if (!CryptCreateHash(crypto.hProv, CALG_SHA_256, 0, 0, &crypto.hHash))
        return {};

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return {};

    std::vector<BYTE> buffer(READ_BUFFER_SIZE);
    DWORD bytesRead = 0;
    while (ReadFile(hFile, buffer.data(), READ_BUFFER_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
        if (!CryptHashData(crypto.hHash, buffer.data(), bytesRead, 0)) {
            CloseHandle(hFile);
            return {};
        }
    }
    CloseHandle(hFile);

    if (!CryptGetHashParam(crypto.hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
        return {};

    for (DWORD i = 0; i < cbHash; ++i) {
        hashStream << digits[rgbHash[i] >> 4];
        hashStream << digits[rgbHash[i] & 0xf];
    }
    return hashStream.str();
}

void CollectFiles(const std::wstring& directory, const std::wstring& baseDir,
                  std::vector<std::wstring>& paths, std::vector<DWORD>& sizes) {
    std::wstring search = directory + L"\\*";
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;

        const std::wstring fullPath = directory + L"\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CollectFiles(fullPath, baseDir, paths, sizes);
        } else {
            paths.push_back(fullPath);
            sizes.push_back(fd.nFileSizeLow);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                sprintf_s(buf, "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

bool GenerateJsonFile(const std::vector<FileInfo>& files, const std::wstring& outputPath) {
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) return false;

    out << "{\n  \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& f = files[i];
        out << "    {\n";
        out << "      \"name\": \"" << JsonEscape(WideToUtf8(f.name)) << "\",\n";
        out << "      \"size\": " << f.size << ",\n";
        out << "      \"hash\": \"" << f.hash << "\"\n";
        out << "    }";
        if (i + 1 < files.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return true;
}

struct ProgressPayload {
    int  current;
    int  total;
    wchar_t fileName[260];
};

struct DonePayload {
    bool  success;
    wchar_t message[512];
    int   fileCount;
};

void RunHashJob(std::wstring inputDir, std::wstring outputFile) {
    DonePayload done{};
    done.success = false;

    std::vector<std::wstring> paths;
    std::vector<DWORD> sizes;
    CollectFiles(inputDir, inputDir, paths, sizes);

    if (g_cancel) {
        wcscpy_s(done.message, L"Cancelled.");
        PostMessageW(g_hWnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(new DonePayload(done)));
        return;
    }

    if (paths.empty()) {
        wcscpy_s(done.message, L"No files found in the selected folder.");
        PostMessageW(g_hWnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(new DonePayload(done)));
        return;
    }

    const int total = static_cast<int>(paths.size());
    std::vector<FileInfo> results;
    results.reserve(total);

    for (int i = 0; i < total; ++i) {
        if (g_cancel) {
            wcscpy_s(done.message, L"Cancelled.");
            PostMessageW(g_hWnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(new DonePayload(done)));
            return;
        }

        const std::wstring& fullPath = paths[i];
        std::wstring relPath = fullPath.substr(inputDir.size());
        if (!relPath.empty() && (relPath[0] == L'\\' || relPath[0] == L'/'))
            relPath = relPath.substr(1);

        auto* prog = new ProgressPayload{};
        prog->current = i + 1;
        prog->total   = total;
        wcscpy_s(prog->fileName, relPath.c_str());
        PostMessageW(g_hWnd, WM_APP_PROGRESS, 0, reinterpret_cast<LPARAM>(prog));

        std::string hash = CalculateFileHash(fullPath);
        if (hash.empty()) {
            swprintf_s(done.message, L"Failed to hash: %s", relPath.c_str());
            PostMessageW(g_hWnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(new DonePayload(done)));
            return;
        }

        FileInfo info;
        info.name = relPath;
        info.hash = hash;
        info.size = sizes[i];
        results.push_back(std::move(info));
    }

    if (!GenerateJsonFile(results, outputFile)) {
        wcscpy_s(done.message, L"Failed to write output file.");
        PostMessageW(g_hWnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(new DonePayload(done)));
        return;
    }

    done.success   = true;
    done.fileCount = total;
    swprintf_s(done.message, L"Done! %d files hashed successfully.", total);
    PostMessageW(g_hWnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(new DonePayload(done)));
}

void StartJob() {
    std::wstring inputDir  = GetWindowTextStr(g_hInputEdit);
    std::wstring outputFile = GetWindowTextStr(g_hOutputEdit);

    if (inputDir.empty()) {
        MessageBoxW(g_hWnd, L"Please select an input folder.", L"Hasher", MB_ICONWARNING);
        return;
    }
    if (outputFile.empty()) {
        MessageBoxW(g_hWnd, L"Please specify an output JSON file.", L"Hasher", MB_ICONWARNING);
        return;
    }

    DWORD attr = GetFileAttributesW(inputDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(g_hWnd, L"Input folder does not exist.", L"Hasher", MB_ICONERROR);
        return;
    }

    SetDlgText(g_hLog, L"");
    AppendLog(L"Starting scan...");
    SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);
    SetDlgText(g_hStatus,  L"Preparing...");
    SetDlgText(g_hCounter, L"0 / 0");

    g_cancel = false;
    EnableControls(true);

    if (g_worker.joinable()) g_worker.join();
    g_worker = std::thread(RunHashJob, inputDir, outputFile);
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int height, bool bold = false) {
    HWND ctrl = CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? g_hFontBold : g_hFont), TRUE);
    return ctrl;
}

HWND CreateEdit(HWND parent, int id, int x, int y, int w, int height, const wchar_t* text = L"") {
    HWND ctrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, w, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), TRUE);
    return ctrl;
}

HWND CreateButton(HWND parent, int id, const wchar_t* text, int x, int y, int w, int height) {
    HWND ctrl = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), TRUE);
    return ctrl;
}

void CreateUi(HWND hwnd) {
    const int margin = 20;
    const int labelH = 18;
    const int editH  = 28;
    const int btnW   = 110;
    const int rowGap = 12;

    int y = margin;

    CreateLabel(hwnd, L"FILE HASHER", margin, y, 300, 24, true);
    y += 28;
    CreateLabel(hwnd, L"SHA-256 directory scanner", margin, y, 400, labelH);
    y += labelH + rowGap + 4;

    CreateLabel(hwnd, L"Input folder", margin, y, 200, labelH);
    y += labelH + 4;

    const int editW = 520 - btnW - 8;
    g_hInputEdit = CreateEdit(hwnd, IDC_INPUT_EDIT, margin, y, editW, editH, L"Client");
    CreateButton(hwnd, IDC_INPUT_BROWSE, L"Browse...", margin + editW + 8, y - 1, btnW, editH + 2);
    y += editH + rowGap;

    CreateLabel(hwnd, L"Output JSON", margin, y, 200, labelH);
    y += labelH + 4;

    g_hOutputEdit = CreateEdit(hwnd, IDC_OUTPUT_EDIT, margin, y, editW, editH, L"client.json");
    CreateButton(hwnd, IDC_OUTPUT_BROWSE, L"Browse...", margin + editW + 8, y - 1, btnW, editH + 2);
    y += editH + rowGap + 8;

    CreateLabel(hwnd, L"Progress", margin, y, 200, labelH);
    y += labelH + 4;

    g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        margin, y, 520 + btnW + 8, 22, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROGRESS)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);
    SendMessageW(g_hProgress, PBM_SETBARCOLOR, 0, CLR_ACCENT);
    SendMessageW(g_hProgress, PBM_SETBKCOLOR, 0, CLR_EDIT_BG);
    y += 30;

    g_hStatus  = CreateLabel(hwnd, L"Ready", margin, y, 400, labelH);
    g_hCounter = CreateLabel(hwnd, L"", margin + 400, y, 200, labelH);
    SendMessageW(g_hCounter, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFontBold), TRUE);
    y += labelH + rowGap;

    const int btnRowY = y;
    const int actionBtnW = 130;
    g_hStartBtn      = CreateButton(hwnd, IDC_START,       L"Start",          margin,                    btnRowY, actionBtnW, 34);
    g_hCancelBtn     = CreateButton(hwnd, IDC_CANCEL,      L"Cancel",         margin + actionBtnW + 10,  btnRowY, actionBtnW, 34);
    g_hOpenFolderBtn = CreateButton(hwnd, IDC_OPEN_FOLDER, L"Open Output",    margin + (actionBtnW + 10) * 2, btnRowY, actionBtnW, 34);
    EnableWindow(g_hCancelBtn, FALSE);
    y += 34 + rowGap;

    CreateLabel(hwnd, L"Log", margin, y, 200, labelH);
    y += labelH + 4;

    g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        margin, y, 520 + btnW + 8, 130, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_hLog, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), TRUE);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hWnd = hwnd;
        CreateUi(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG);
        return reinterpret_cast<LRESULT>(g_hBgBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctrl = reinterpret_cast<HWND>(lParam);
        if (ctrl == g_hLog) {
            SetTextColor(hdc, CLR_MUTED);
            SetBkColor(hdc, CLR_LOG_BG);
            return reinterpret_cast<LRESULT>(g_hLogBrush);
        }
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_EDIT_BG);
        return reinterpret_cast<LRESULT>(g_hEditBrush);
    }
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, g_hBgBrush);
        return 1;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_INPUT_BROWSE: {
            std::wstring path = BrowseForFolder(hwnd, L"Select folder to hash");
            if (!path.empty()) SetDlgText(g_hInputEdit, path);
            break;
        }
        case IDC_OUTPUT_BROWSE: {
            std::wstring path = BrowseForSaveFile(hwnd);
            if (!path.empty()) SetDlgText(g_hOutputEdit, path);
            break;
        }
        case IDC_START:
            StartJob();
            break;
        case IDC_CANCEL:
            g_cancel = true;
            SetDlgText(g_hStatus, L"Cancelling...");
            AppendLog(L"Cancelling...");
            break;
        case IDC_OPEN_FOLDER: {
            std::wstring out = GetWindowTextStr(g_hOutputEdit);
            if (!out.empty()) {
                std::wstring param = L"/select,\"" + out + L"\"";
                ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
            }
            break;
        }
        }
        return 0;

    case WM_APP_PROGRESS: {
        auto* p = reinterpret_cast<ProgressPayload*>(lParam);
        if (p) {
            int pct = p->total > 0 ? (p->current * 100) / p->total : 0;
            SendMessageW(g_hProgress, PBM_SETPOS, pct, 0);
            SetDlgText(g_hStatus, std::wstring(L"Hashing: ") + p->fileName);
            SetDlgText(g_hCounter, std::to_wstring(p->current) + L" / " + std::to_wstring(p->total));
            if (p->current == 1 || p->current == p->total || p->current % 10 == 0)
                AppendLog(std::wstring(L"[") + std::to_wstring(p->current) + L"/" +
                          std::to_wstring(p->total) + L"] " + p->fileName);
            delete p;
        }
        return 0;
    }
    case WM_APP_DONE: {
        auto* d = reinterpret_cast<DonePayload*>(lParam);
        if (d) {
            EnableControls(false);
            SendMessageW(g_hProgress, PBM_SETPOS, d->success ? 100 : 0, 0);
            SetDlgText(g_hStatus, d->message);
            AppendLog(d->message);

            if (d->success) {
                EnableWindow(g_hOpenFolderBtn, TRUE);
                MessageBoxW(hwnd, d->message, L"Hasher - Complete", MB_ICONINFORMATION);
            } else if (wcslen(d->message) > 0 && wcscmp(d->message, L"Cancelled.") != 0) {
                MessageBoxW(hwnd, d->message, L"Hasher - Error", MB_ICONERROR);
            }
            delete d;
        }
        if (g_worker.joinable()) g_worker.join();
        return 0;
    }
    case WM_DESTROY:
        g_cancel = true;
        if (g_worker.joinable()) g_worker.join();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    g_hFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hFontBold = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_hBgBrush    = CreateSolidBrush(CLR_BG);
    g_hPanelBrush = CreateSolidBrush(CLR_PANEL);
    g_hEditBrush  = CreateSolidBrush(CLR_EDIT_BG);
    g_hLogBrush   = CreateSolidBrush(CLR_LOG_BG);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_hBgBrush;
    wc.lpszClassName = L"HasherMainWnd";
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    const int winW = 680;
    const int winH = 480;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, L"HasherMainWnd", L"Hasher - SHA-256 Manifest Generator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        sx, sy, winW, winH, nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hFont)     DeleteObject(g_hFont);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hBgBrush)    DeleteObject(g_hBgBrush);
    if (g_hPanelBrush) DeleteObject(g_hPanelBrush);
    if (g_hEditBrush)  DeleteObject(g_hEditBrush);
    if (g_hLogBrush)   DeleteObject(g_hLogBrush);

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
