// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

// Which Qt platform theme to ask for, so that the app uses the desktop's
// file dialog and, on KDE Plasma, follows the user's colors, widget style
// and icon theme. Nothing here needs a GUI.
namespace PlatformTheme
{

struct Choice {
    // Value for QT_QPA_PLATFORMTHEME; empty: leave the environment alone.
    QByteArray theme;
    // Plasma's theme has its own file dialog, which uses KIO. Ask it to use
    // the desktop portal like the other theme does
    // (PLASMA_INTEGRATION_USE_PORTAL).
    bool plasmaUsesPortal = false;
};

// Whether `desktop` (XDG_CURRENT_DESKTOP, a colon-separated list) is KDE.
bool isKde(const QString &desktop);

// `current` is the QT_QPA_PLATFORMTHEME we got. Anything but "gtk2" (which
// linuxdeploy's AppImage hook sets on GNOME and XFCE, and isn't bundled) is
// the user's choice and is left alone. `hasPlugin` tells whether a platform
// theme plugin ("kde", "xdgdesktopportal") is available to us: in a Flatpak
// it only matters that we are on KDE, since the runtime has them all.
// Plasma's theme is only worth it with a desktop portal: without one its
// file dialog (which we ask to use the portal, as the KIO one can't be
// bundled) would not open at all, so `portalAvailable` must be true.
Choice choose(const QByteArray &current,
              const QString &desktop,
              bool inFlatpak,
              bool inAppImage,
              const std::function<bool(const QByteArray &)> &hasPlugin,
              bool portalAvailable);

} // namespace PlatformTheme
