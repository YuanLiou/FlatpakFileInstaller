// SPDX-License-Identifier: Apache-2.0
// Never loaded. Lists QML modules that are only used at runtime (the
// org.kde.desktop style is chosen in main.cpp), so the AppImage build's
// import scanner bundles them too.
import QtQuick
import org.kde.desktop
import org.kde.kirigami

Item {}
