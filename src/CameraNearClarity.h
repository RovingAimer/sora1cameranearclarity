// Copyright (c) 2026 CameraNearClarity contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
// Error/initialization flags are cumulative. PatchActive reflects current confirmed state.
enum SoraCameraStatus : DWORD {
    InitializationFinished = 0x1, HostNameMatch = 0x2, HostHashMatch = 0x4,
    PatchActive = 0x8, OriginalContextMatch = 0x10,
    ForeignHost = 0x100, HostHashRejected = 0x200, PatchConflict = 0x400,
    MemoryFinalizeFailed = 0x800, PatchFailed = 0x1000,
    InitializationApiFailed = 0x2000, ControlUnavailable = 0x4000, CorePinFailed = 0x8000
};
extern "C" {
__declspec(dllexport) DWORD WINAPI SoraCameraInitialize() noexcept;
__declspec(dllexport) DWORD WINAPI SoraCameraGetStatus() noexcept;
}
