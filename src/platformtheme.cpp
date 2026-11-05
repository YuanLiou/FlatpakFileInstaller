// SPDX-License-Identifier: Apache-2.0
#include "platformtheme.h"

#include <QStringList>

namespace PlatformTheme
{

bool isKde(const QString &desktop)
{
    const QStringList names = desktop.split(u':', Qt::SkipEmptyParts);
    for (const QString &name : names) {
        if (name.compare(u"KDE", Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

Choice choose(const QByteArray &current,
              const QString &desktop,
              bool inFlatpak,
              bool inAppImage,
              const std::function<bool(const QByteArray &)> &hasPlugin,
              bool portalAvailable)
{
    if (!current.isEmpty() && !(inAppImage && current == "gtk2")) {
        return {};
    }
    // Qt would pick the portal theme in a Flatpak, which only knows light and
    // dark. On KDE, ask for Plasma's theme: it reads the user's settings.
    if ((inFlatpak || inAppImage) && isKde(desktop) && portalAvailable && (inFlatpak || hasPlugin("kde"))) {
        return {.theme = "kde", .plasmaUsesPortal = true};
    }
    if (inAppImage && hasPlugin("xdgdesktopportal")) {
        return {.theme = "xdgdesktopportal"};
    }
    return {};
}

} // namespace PlatformTheme
