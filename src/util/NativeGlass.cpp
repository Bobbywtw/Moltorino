// SPDX-License-Identifier: MIT
#include "util/NativeGlass.hpp"

#include <QOperatingSystemVersion>
#include <QVariant>
#include <QWidget>

#ifdef Q_OS_WIN
#    include <dwmapi.h>
#    include <qt_windows.h>
#    pragma comment(lib, "Dwmapi.lib")
#    pragma comment(lib, "Advapi32.lib")
#endif

namespace chatterino::nativeglass {
namespace {
bool policyKnown = false;
bool allowed = true;
constexpr auto ACTIVE_PROPERTY = "_moltorino_nativeGlass";
}  // namespace

bool supported()
{
#ifdef Q_OS_WIN
    return QOperatingSystemVersion::current().microVersion() >= 22621;
#else
    return false;
#endif
}

void refreshPolicy()
{
    allowed = true;
#ifdef Q_OS_WIN
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast,
                              0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON))
    {
        allowed = false;
    }
    DWORD transparency = 1;
    DWORD bytes = sizeof(transparency);
    // Read the user's existing accessibility preference; never change it.
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Pe"
                     L"rsonalize",
                     L"EnableTransparency", RRF_RT_REG_DWORD, nullptr,
                     &transparency, &bytes) == ERROR_SUCCESS &&
        transparency == 0)
    {
        allowed = false;
    }
#endif
    policyKnown = true;
}

bool effectsAllowed()
{
    if (!policyKnown)
    {
        refreshPolicy();
    }
    return allowed;
}

void prepare(QWidget *window)
{
#ifdef Q_OS_WIN
    if (supported() && !window->windowFlags().testFlag(Qt::FramelessWindowHint))
    {
        // Keep Moltorino's existing native frame and hit testing. Qt requests an
        // alpha backing store, without making this normally framed HWND layered.
        window->setAttribute(Qt::WA_TranslucentBackground);
    }
#else
    (void)window;
#endif
}

bool apply(QWidget *window, bool transient, bool dark)
{
#ifdef Q_OS_WIN
    if (!window || !supported() ||
        window->windowFlags().testFlag(Qt::FramelessWindowHint))
    {
        return false;
    }
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED)
    {
        window->setProperty(ACTIVE_PROPERTY, false);
        return false;
    }
    const BOOL darkMode = dark;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode,
                          sizeof(darkMode));
    const DWM_SYSTEMBACKDROP_TYPE type = !effectsAllowed() ? DWMSBT_NONE
                                         : transient ? DWMSBT_TRANSIENTWINDOW
                                                     : DWMSBT_MAINWINDOW;
    const HRESULT result = DwmSetWindowAttribute(
        hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    window->setProperty("_moltorino_dwmResult", static_cast<qlonglong>(result));
    window->setProperty("_moltorino_dwmType", static_cast<int>(type));
    const bool enabled = SUCCEEDED(result) && effectsAllowed();
    if (enabled)
    {
        RECT bounds{};
        GetClientRect(hwnd, &bounds);
        // Extending the left edge across the client retains the hidden native
        // caption when Windows' accent-colored title-bar setting is enabled.
        const MARGINS margins{bounds.right + 1, 0, 0, 0};
        if (FAILED(DwmExtendFrameIntoClientArea(hwnd, &margins)))
        {
            const DWM_SYSTEMBACKDROP_TYPE none = DWMSBT_NONE;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &none,
                                  sizeof(none));
            window->setProperty(ACTIVE_PROPERTY, false);
            return false;
        }
    }
    window->setProperty(ACTIVE_PROPERTY, enabled);
    return enabled;
#else
    (void)window;
    (void)transient;
    (void)dark;
    return false;
#endif
}

bool active(const QWidget *window)
{
    return window && window->property(ACTIVE_PROPERTY).toBool() &&
           effectsAllowed();
}
}  // namespace chatterino::nativeglass
