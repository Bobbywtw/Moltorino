// SPDX-License-Identifier: MIT
#pragma once

class QWidget;
namespace chatterino::nativeglass {
// Documented DWM APIs on this application's HWND only. No private compositor
// entry points, DLL hooks, screen capture, telemetry or settings writes.
bool supported();
bool effectsAllowed();
void prepare(QWidget *window);
bool apply(QWidget *window, bool transient, bool dark);
bool active(const QWidget *window);
void refreshPolicy();
}  // namespace chatterino::nativeglass
