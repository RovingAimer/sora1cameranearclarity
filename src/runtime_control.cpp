// Copyright (c) 2026 CameraNearClarity contributors
// Copyright (c) 2026 DialogueOutline contributors
// SPDX-License-Identifier: MIT
// UTF decoder adapted from sora1dialogueoutline v0.2.1 runtime_config.cpp.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "runtime_control.h"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace camera {
namespace {
struct Settings {
    bool enabled = true;
    unsigned key = VK_F8;
};
struct KeyEdge {
    bool focused = false;
    bool down = true;
    bool Pressed(bool foreground, bool pressed) noexcept {
        if (!foreground) { focused = false; down = true; return false; }
        if (!focused) { focused = true; down = pressed; return false; }
        const bool trigger = pressed && !down;
        down = pressed;
        return trigger;
    }
};
std::wstring_view Trim(std::wstring_view text) noexcept {
    const auto first = text.find_first_not_of(L" \t\r");
    if (first == std::wstring_view::npos) return {};
    const auto last = text.find_last_not_of(L" \t\r");
    return text.substr(first, last-first+1);
}
bool Equal(std::wstring_view a, std::wstring_view b) noexcept {
    return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
        b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
bool Number(std::wstring_view value, unsigned& number) noexcept {
    value = Trim(value);
    unsigned base = 10;
    if (value.size() >= 2 && value[0] == L'0' && (value[1] == L'x' || value[1] == L'X')) {
        base = 16;
        value.remove_prefix(2);
    }
    if (value.empty()) return false;
    number = 0;
    for (wchar_t c : value) {
        unsigned digit;
        if (c >= L'0' && c <= L'9') digit = static_cast<unsigned>(c-L'0');
        else if (c >= L'a' && c <= L'f') digit = static_cast<unsigned>(c-L'a')+10;
        else if (c >= L'A' && c <= L'F') digit = static_cast<unsigned>(c-L'A')+10;
        else return false;
        if (digit >= base) return false;
        number = number*base+digit;
        if (number > 254) return false;
    }
    return true;
}
bool Decode(const std::string& bytes, std::wstring& text) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    if (bytes.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        if (bytes.size()%2) return false;
        text.resize((bytes.size()-2)/2);
        for (std::size_t i=0; i<text.size(); ++i)
            text[i] = static_cast<wchar_t>(data[2+2*i] | (data[3+2*i]<<8));
        if (!text.empty() && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr)) return false;
    } else {
        const std::size_t skip = bytes.size() >= 3 && data[0]==0xEF && data[1]==0xBB && data[2]==0xBF ? 3 : 0;
        const int size = static_cast<int>(bytes.size()-skip);
        if (!size) return false;
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data()+skip, size, nullptr, 0);
        if (!count) return false;
        text.resize(static_cast<std::size_t>(count));
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data()+skip, size, text.data(), count)) return false;
    }
    return !text.empty() && text.find(L'\0') == std::wstring::npos;
}
bool Parse(const std::string& bytes, Settings& settings) {
    std::wstring decoded;
    if (!Decode(bytes, decoded)) return false;
    std::wstring_view text = decoded;
    bool inside = false, section = false, haveEnabled = false, haveKey = false;
    while (!text.empty()) {
        const auto end = text.find(L'\n');
        auto line = text.substr(0, end);
        text = end == std::wstring_view::npos ? std::wstring_view{} : text.substr(end+1);
        line = Trim(line.substr(0, line.find_first_of(L";#")));
        if (line.empty()) continue;
        if (line.front() == L'[') {
            if (line.back() != L']') return false;
            inside = Equal(Trim(line.substr(1, line.size()-2)), L"CameraNearClarity");
            section = section || inside;
            continue;
        }
        if (!inside) continue;
        const auto equals = line.find(L'=');
        if (equals == std::wstring_view::npos) return false;
        const auto key = Trim(line.substr(0, equals));
        unsigned value = 0;
        if (Equal(key, L"Enabled")) {
            if (haveEnabled || !Number(line.substr(equals+1), value) || value>1) return false;
            settings.enabled = value != 0;
            haveEnabled = true;
        } else if (Equal(key, L"ToggleKey")) {
            if (haveKey || !Number(line.substr(equals+1), value)) return false;
            settings.key = value;
            haveKey = true;
        }
    }
    // Require both values: a partially saved file must not reset either one.
    return section && haveEnabled && haveKey;
}
bool Read(const std::wstring& path, std::string& bytes) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    char buffer[8193];
    DWORD count = 0;
    const bool ok = ReadFile(file, buffer, sizeof(buffer), &count, nullptr) && count && count <= 8192;
    CloseHandle(file);
    if (ok) bytes.assign(buffer, count);
    return ok;
}
struct Control {
    std::wstring path;
    std::string acceptedBytes;
    Settings settings;
    bool enabled = true;
    KeyEdge edge;
    ULONGLONG nextRead = 0;
    PTP_TIMER timer = nullptr;
    SRWLOCK lock = SRWLOCK_INIT;
    ~Control() { if (timer) CloseThreadpoolTimer(timer); }
};
void Reload(Control& state) {
    std::string bytes;
    if (!Read(state.path, bytes) || bytes == state.acceptedBytes) return;
    Settings value;
    if (!Parse(bytes, value) || !SetEnabled(value.enabled)) return;
    if (value.key != state.settings.key) state.edge = {};
    state.settings = value;
    state.enabled = value.enabled;
    state.acceptedBytes = std::move(bytes);
}
void Tick(Control& state, bool foreground, bool pressed) noexcept {
    if (!state.settings.key) { state.edge = {}; return; }
    if (state.edge.Pressed(foreground, pressed) && SetEnabled(!state.enabled)) state.enabled = !state.enabled;
}
void CALLBACK TimerReady(PTP_CALLBACK_INSTANCE, void* context, PTP_TIMER) noexcept {
    auto& state = *static_cast<Control*>(context);
    if (!TryAcquireSRWLockExclusive(&state.lock)) return;
    try {
        const auto now = GetTickCount64();
        if (now >= state.nextRead) { state.nextRead = now+500; Reload(state); }
        DWORD process = 0;
        const auto foreground = GetForegroundWindow();
        if (foreground) GetWindowThreadProcessId(foreground, &process);
        const bool focused = foreground && process == GetCurrentProcessId();
        // Only query the configured key while this game process is foreground.
        const bool down = focused && state.settings.key && (GetAsyncKeyState(static_cast<int>(state.settings.key)) & 0x8000);
        Tick(state, focused, down);
    } catch (...) {
        // A failed read/allocation preserves the previous settings and state.
    }
    ReleaseSRWLockExclusive(&state.lock);
}
}
bool StartControls(HMODULE module) noexcept {
    try {
        std::wstring path(32768, L'\0');
        const auto count = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (!count || count >= path.size()) return false;
        path.resize(count);
        const auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return false;
        auto state = std::make_unique<Control>();
        state->path = path.substr(0, slash+1) + L"CameraNearClarity.ini";
        std::string bytes;
        Settings initial;
        if (Read(state->path, bytes) && Parse(bytes, initial)) {
            state->settings = initial;
            state->acceptedBytes = std::move(bytes);
        }
        state->enabled = state->settings.enabled;
        if (!SetEnabled(state->enabled)) return false;
        state->timer = CreateThreadpoolTimer(TimerReady, state.get(), nullptr);
        if (!state->timer) return false;
        state->nextRead = GetTickCount64()+500;
        LARGE_INTEGER relative;
        relative.QuadPart = -250000; // 25 ms; key repeats are handled by KeyEdge.
        FILETIME due{relative.LowPart, static_cast<DWORD>(relative.HighPart)};
        SetThreadpoolTimer(state->timer, &due, 25, 0);
        state.release(); // Pinned module and callbacks live until process exit.
        return true;
    } catch (...) { return false; }
}
}
