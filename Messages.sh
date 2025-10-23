#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>
# SPDX-License-Identifier: GPL-2.0-or-later

# Extract messages from source files for translation

$EXTRACTRC `find . -name \*.ui -o -name \*.rc -o -name \*.kcfg` >> rc.cpp

# Extract from C++ files for daemon
$XGETTEXT `find daemon -name \*.cpp -o -name \*.h` -o $podir/kdisplaypresets_daemon.pot

# Extract from C++ files for common library
$XGETTEXT `find common -name \*.cpp -o -name \*.h` -o $podir/kdisplaypresets_common.pot

# Extract from C++ and QML files for KCM
$XGETTEXT `find kcm -name \*.cpp -o -name \*.h -o -name \*.qml` -o $podir/kcm_displaypresets.pot

# Extract from QML and C++ files for plasmoid
$XGETTEXT `find plasmoid -name \*.qml -o -name \*.cpp -o -name \*.h` -o $podir/plasma_applet_org.kde.kdisplaypresets.pot

rm -f rc.cpp
