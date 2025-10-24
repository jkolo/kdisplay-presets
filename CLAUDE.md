# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Repositories

- **origin** (`git@git.kolosowscy.pl:jurek/kdisplay-presets.git`) - Main repository
- **github** (`git@github.com:jkolo/kdisplay-presets.git`) - Mirror, automatically synchronized from origin

**IMPORTANT**: Always push to `origin` repository. GitHub mirror is automatically synchronized and should not be pushed to directly.

## Project Overview

KDisplay Presets is a standalone KDE Plasma utility for managing display configuration presets. It allows users to save multiple display configurations and quickly switch between them using a system tray widget, keyboard shortcuts, or the System Settings module.

This project was extracted from KScreen to provide a focused, lightweight solution for display preset management that can coexist with the standard KScreen configuration tool.

## Build Commands

```bash
# Build using existing build directory
ninja -C build

# Clean build from scratch
cmake -B build -G Ninja
ninja -C build

# Install
sudo ninja -C build install
```

## Testing Commands

```bash
# Test the daemon (D-Bus service)
QT_LOGGING_RULES="*.debug=true" ./build/bin/kdisplaypresets_daemon

# Test the KCM module
kcmshell6 kcm_displaypresets

# Check D-Bus service
dbus-send --print-reply --dest=org.kde.kdisplaypresets / org.kde.kdisplaypresets.getPresets

# Kill running daemon (if needed for testing new build)
ps aux | grep kdisplaypresets_daemon
kill <PID>
```

## High-Level Architecture

### Core Components

**D-Bus Daemon (`daemon/`)**: Background service that manages display presets via D-Bus. Main class is `PresetsService` which:
- Manages preset configuration via `org.kde.kdisplaypresets` D-Bus interface
- Monitors display configuration changes automatically
- Handles global keyboard shortcuts for preset switching (via KGlobalAccel)
- Applies presets by communicating with KScreen backend

**Common Library (`common/`)**: Shared code used across components:
- `Presets` - QAbstractListModel for preset data management
- `Utils` - Display output naming and utility functions
- Configuration file I/O (JSON format)

**KCM Module (`kcm/`)**: System Settings module for preset management. Uses:
- `KCMDisplayPresets` - Main KCM controller
- `PresetManager` - Business logic for preset operations
- QML UI in `kcm/ui/` for interface
- Calls D-Bus service to apply presets

**Plasmoid Widget (`plasmoid/`)**: System tray widget with auto-hide functionality. Features:
- `KDisplayPresetsApplet` - Applet backend with D-Bus integration
- `PresetModel` - QAbstractListModel bridging D-Bus data to QML
- **Auto-hide logic**: Widget automatically hides when ≤1 available preset exists and is currently active
- QML UI for quick preset switching

### Key Design Patterns

- **D-Bus Communication**: All components communicate via `org.kde.kdisplaypresets` service
- **Configuration Persistence**: Uses `~/.config/kdisplaypresets/presets.json` for preset storage
- **Auto-hide Widget**: Plasmoid uses `PlasmaCore.Types.PassiveStatus` when not needed
- **Separation from KScreen**: Independent D-Bus namespace allows coexistence with KScreen

## Development Guidelines

### Code Style
- Uses KDE Frameworks Coding Style with `.clang-format` configuration
- C++20 standard required
- License headers: GPL-2.0-or-later with SPDX identifiers

### Dependencies
- Qt 6.8.0+ (Gui, Quick, QuickControls2, DBus, Test)
- KDE Frameworks 6.5.0+ (I18n, CoreAddons, Config, DBusAddons, XmlGui, GlobalAccel, KCMUtils, Screen)
- Plasma 6.5.0+
- KF6Screen 6.5.0+ (core screen management library)

### Important Files
- `daemon/org.kde.kdisplaypresets.xml` - D-Bus interface definition
- `kcm/kcm_displaypresets.json` - KCM metadata (category: `display`)
- `plasmoid/metadata.json` - Plasmoid metadata
- Configuration: `~/.config/kdisplaypresets/presets.json`
- D-Bus service: `org.kde.kdisplaypresets.service`
- Systemd unit: `plasma-kdisplaypresets.service`

### Key Features

**Apply vs Load Button**:
- UI uses "Apply" instead of "Load" to better reflect the action
- Implemented in `kcm/ui/main.qml`

**Auto-hide Logic**:
- Widget hides when there's only one available preset and it's already active
- Prevents clutter in system tray when presets aren't useful
- Implemented in `plasmoid/main.qml` via `Plasmoid.status` property

**Keyboard Shortcuts**:
- Managed by daemon using KGlobalAccel
- Shortcuts stored per-preset in configuration
- Automatically registered/unregistered when presets change

### D-Bus Interface

Service: `org.kde.kdisplaypresets`
Path: `/`

Methods:
- `applyPreset(presetId: string)` - Apply a display preset
- `getPresets() -> array<variant>` - Get all presets with status

Signals:
- `presetsChanged(changedPresets: array<variant>)` - Emitted when presets change

### Daemon Management

Only one `kdisplaypresets_daemon` can connect to D-Bus at a time.

Before starting a new daemon:
```bash
# Check if daemon is running
ps aux | grep kdisplaypresets_daemon

# Kill if needed (when testing new build)
killall kdisplaypresets_daemon

# Start daemon with debug logging
QT_LOGGING_RULES="*.debug=true" ./build/bin/kdisplaypresets_daemon
```

Check daemon status:
```bash
dbus-send --print-reply --dest=org.kde.kdisplaypresets / org.kde.kdisplaypresets.getPresets
```

### Testing Workflow

1. Build project: `ninja -C build`
2. Kill existing daemon: `killall kdisplaypresets_daemon`
3. Start new daemon: `./build/bin/kdisplaypresets_daemon`
4. Test KCM: `kcmshell6 kcm_displaypresets`
5. Test widget: Add "Display Presets" to system tray

### Common Issues

**Daemon not starting**:
- Check if another instance is running
- Verify D-Bus session bus is available
- Check logging output for errors

**Presets not appearing in widget**:
- Verify daemon is running and registered on D-Bus
- Check preset availability (all required monitors must be connected)
- Review D-Bus signal connections in logs

**Widget not hiding**:
- Check auto-hide logic in `plasmoid/main.qml`
- Verify `isAvailable` and `isCurrent` flags are correct
- Enable debug logging to see status calculations

### Relation to KScreen

This project is **independent** from KScreen but works alongside it:
- Uses separate D-Bus namespace (`org.kde.kdisplaypresets` vs `org.kde.kscreen`)
- Uses separate configuration directory (`~/.config/kdisplaypresets/`)
- Can be installed and run simultaneously with KScreen
- Delegates actual display configuration to KScreen backend (libkscreen)
- Appears in same System Settings category as KScreen for user convenience

### Translation Domain

- Daemon: `kdisplaypresets_daemon`
- KCM: `kcm_displaypresets`
- Plasmoid: `plasma_applet_org.kde.kdisplaypresets`

Ensure proper `i18n()` calls with correct translation domains in each component.
