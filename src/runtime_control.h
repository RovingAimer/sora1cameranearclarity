// Copyright (c) 2026 CameraNearClarity contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>

namespace camera {
// Called only during initialization and from the serialized control timer.
bool SetEnabled(bool enabled) noexcept;
// The caller must pin the module before starting process-lifetime callbacks.
bool StartControls(HMODULE module) noexcept;
}
