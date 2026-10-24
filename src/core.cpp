// Copyright (c) 2026 DialogueOutline contributors
// Copyright (c) 2026 CameraNearClarity contributors
// SPDX-License-Identifier: MIT
// Explicit InitOnce pattern adapted from DialogueOutline core.cpp.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "CameraNearClarity.h"
#include "LicenseNotice.h"
#include "target_patterns.h"
#include "runtime_control.h"
#include <cstdint>
#include <cstring>
#include <intrin.h>

namespace {
constexpr std::size_t kPatchOffset = 3;
unsigned char gOriginalContext[sizeof(targets::kNearFade)]{};
volatile LONG gStatus = 0;
INIT_ONCE gOnce = INIT_ONCE_STATIC_INIT;
unsigned char* gPatchContext = nullptr;
bool gPatchEnabled = false;
bool gPatchReady = false;
void status(DWORD value) noexcept { InterlockedOr(&gStatus, static_cast<LONG>(value)); }

bool prepare_patch() noexcept {
    auto* context = targets::NearFade();
    if (!context) { status(PatternMissing); return false; }
    std::memcpy(gOriginalContext, context, sizeof(gOriginalContext));
    for (std::size_t i=0; i<sizeof(gOriginalContext); ++i) {
        if (targets::kNearFadeMask[i]!='?' && gOriginalContext[i]!=targets::kNearFade[i]) {
            status(PatchConflict); return false;
        }
    }
    status(TargetPatternFound | OriginalContextMatch);
    gPatchContext = context;
    gPatchReady = true;
    return true;
}
BOOL CALLBACK initialize(PINIT_ONCE, PVOID, PVOID*) noexcept;
} // namespace

namespace camera {
bool SetEnabled(bool enabled) noexcept {
    if (!gPatchReady) return false;
    unsigned char expected[sizeof(gOriginalContext)];
    std::memcpy(expected, gOriginalContext, sizeof(expected));
    if (gPatchEnabled) expected[kPatchOffset] = 0xEB;
    if (std::memcmp(gPatchContext, expected, sizeof(expected)) != 0) {
        status(PatchConflict);
        InterlockedAnd(&gStatus, ~static_cast<LONG>(PatchActive));
        gPatchReady = false; // Unknown writer: never overwrite or retry its bytes.
        return false;
    }
    if (enabled == gPatchEnabled) return true;
    auto* patch = gPatchContext + kPatchOffset;
    DWORD previous = 0;
    if (!VirtualProtect(patch, sizeof(CHAR), PAGE_EXECUTE_READWRITE, &previous)) {
        status(PatchFailed); return false;
    }
    const auto before = static_cast<CHAR>(gPatchEnabled ? 0xEB : gOriginalContext[kPatchOffset]);
    const auto after = static_cast<CHAR>(enabled ? 0xEB : gOriginalContext[kPatchOffset]);
    const bool changed = _InterlockedCompareExchange8(reinterpret_cast<volatile CHAR*>(patch), after, before) == before;
    DWORD ignored = 0;
    const bool restored = VirtualProtect(patch, sizeof(CHAR), previous, &ignored) != FALSE;
    const bool flushed = FlushInstructionCache(GetCurrentProcess(), patch, sizeof(CHAR)) != FALSE;
    if (!restored || !flushed) { status(MemoryFinalizeFailed); gPatchReady = false; }
    if (!changed) {
        status(PatchConflict);
        InterlockedAnd(&gStatus, ~static_cast<LONG>(PatchActive));
        gPatchReady = false;
        return false;
    }
    gPatchEnabled = enabled;
    if (enabled) status(PatchActive);
    else InterlockedAnd(&gStatus, ~static_cast<LONG>(PatchActive));
    return restored && flushed;
}
}
namespace {
BOOL CALLBACK initialize(PINIT_ONCE, PVOID, PVOID*) noexcept {
    if (!prepare_patch()) {
        status(PatchFailed | InitializationFinished); return TRUE;
    }
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&initialize), &pinned)) {
        status(CorePinFailed | InitializationFinished); return TRUE;
    }
    if (!camera::StartControls(pinned)) status(ControlUnavailable);
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
