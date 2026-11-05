// SPDX-License-Identifier: Apache-2.0
// Never loaded. Lists QML modules that are only used at runtime (the
// org.kde.desktop style is chosen in main.cpp, the About page is loaded by
// Kirigami Addons), so the AppImage build's import scanner bundles them too.
import QtQuick
import Qt.labs.platform
import org.kde.coreaddons
import org.kde.desktop
import org.kde.kirigami
import org.kde.kirigamiaddons.formcard

Item {}
