// Copyright (c) 2026 DialogueOutline contributors
// Copyright (c) 2026 CameraNearClarity contributors
// SPDX-License-Identifier: MIT
// Host hashing and explicit InitOnce pattern adapted from DialogueOutline core.cpp.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "CameraNearClarity.h"
#include "LicenseNotice.h"
#include "PatchContext.h"
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <intrin.h>

namespace {
constexpr std::uintptr_t kPatchRva = 0x1C9616;
constexpr std::uintptr_t kContextRva = 0x1C9613;
constexpr LONGLONG kExeSize = 11862528;
constexpr unsigned char kExeHash[] = {
    0x95,0x85,0xd3,0xd9,0x93,0x87,0x4d,0x65,0x15,0x6a,0xba,0x63,0x3c,0x00,0xb8,0xa2,
    0x5f,0xaa,0xb3,0xaf,0xde,0x89,0x53,0xa8,0xa1,0x83,0xc8,0x0f,0xb8,0xf6,0x57,0xac
};
volatile LONG gStatus = 0;
INIT_ONCE gOnce = INIT_ONCE_STATIC_INIT;
void status(DWORD value) noexcept { InterlockedOr(&gStatus, static_cast<LONG>(value)); }

bool host_path(wchar_t (&path)[32768]) noexcept {
    const DWORD count = GetModuleFileNameW(nullptr, path, 32768);
    if (!count || count >= 32768) return false;
    const wchar_t* name = std::wcsrchr(path, L'\\');
    return _wcsicmp(name ? name + 1 : path, L"sora_1st.exe") == 0;
}
bool verified_file_hash(const wchar_t* path) noexcept {
    // Reject concurrent writers while checking the exact executable on disk.
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    bool okay = GetFileSizeEx(file, &size) && size.QuadPart == kExeSize;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32] = {}, buffer[65536];
    if (okay) okay = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (okay) okay = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    while (okay) {
        DWORD bytes = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &bytes, nullptr)) { okay = false; break; }
        if (!bytes) break;
        okay = BCryptHashData(hash, buffer, bytes, 0) >= 0;
    }
    if (okay) okay = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return okay && std::memcmp(digest, kExeHash, sizeof(digest)) == 0;
}

bool install_patch() noexcept {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* context = base + kContextRva;
    auto* patch = base + kPatchRva;
    MEMORY_BASIC_INFORMATION region = {};
    if (reinterpret_cast<std::uintptr_t>(patch) % sizeof(SHORT) ||
        !VirtualQuery(context, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        region.Type != MEM_IMAGE || region.AllocationBase != base ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
        reinterpret_cast<std::uintptr_t>(context) + sizeof(kContext) >
            reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize ||
        std::memcmp(context, kContext, sizeof(kContext)) != 0) {
        status(PatchConflict); return false;
    }
    status(OriginalContextMatch);
    DWORD previous = 0;
    if (!VirtualProtect(patch, sizeof(SHORT), PAGE_EXECUTE_READWRITE, &previous)) return false;
    // Aligned 2-byte atomic compare/exchange preserves displacement and avoids a torn instruction.
    // A competing writer changing the original 72 37 causes refusal, never an overwrite.
    const bool installed = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(patch),
        static_cast<SHORT>(0x37EB), static_cast<SHORT>(0x3772)) == static_cast<SHORT>(0x3772);
    DWORD ignored = 0;
    const bool restored = VirtualProtect(patch, sizeof(SHORT), previous, &ignored) != FALSE;
    const bool flushed = FlushInstructionCache(GetCurrentProcess(), patch, sizeof(SHORT)) != FALSE;
    if (!restored || !flushed) status(MemoryFinalizeFailed);
    if (!installed) { status(PatchConflict); return false; }
    status(PatchActive);
    return true;
}
BOOL CALLBACK initialize(PINIT_ONCE, PVOID, PVOID*) noexcept {
    wchar_t path[32768] = {};
    if (!host_path(path)) { status(ForeignHost | InitializationFinished); return TRUE; }
    status(HostNameMatch);
    if (!verified_file_hash(path)) {
        status(HostHashRejected | PatchFailed | InitializationFinished); return TRUE;
    }
    status(HostHashMatch);
    if (!install_patch()) status(PatchFailed);
    status(InitializationFinished);
    return TRUE; // Cache both success and refusal; explicit initialization cannot race retries.
}
} // namespace
extern "C" {
DWORD WINAPI SoraCameraGetStatus() noexcept {
    return static_cast<DWORD>(InterlockedCompareExchange(&gStatus, 0, 0));
}
DWORD WINAPI SoraCameraInitialize() noexcept {
    if (!InitOnceExecuteOnce(&gOnce, initialize, nullptr, nullptr)) status(InitializationApiFailed);
    return SoraCameraGetStatus();
}
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
}
