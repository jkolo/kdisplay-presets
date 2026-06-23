# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Repositories

- **origin** (`git@git.kolosowscy.pl:jurek/kdisplay-presets.git`) - Main repository
- **github** (`git@github.com:jkolo/kdisplay-presets.git`) - Mirror, automatically synchronized from origin

**IMPORTANT**: Always push to `origin` repository. GitHub mirror is automatically synchronized and should not be pushed to directly.

## Project Overview

KDisplay Presets is a standalone KDE Plasma utility for managing display configuration presets. Users save display configurations and switch between them via system tray widget, keyboard shortcuts, or System Settings.

Extracted from KScreen to provide focused preset management that coexists with the standard KScreen configuration tool. Uses separate D-Bus namespace (`org.kde.kdisplaypresets`) and configuration directory (`~/.config/kdisplaypresets/`), but delegates actual display changes to KScreen backend (libkscreen).

## Build Commands

```bash
# Build (using existing build directory)
ninja -C build

# Clean build from scratch
cmake -B build -G Ninja && ninja -C build

# Install
sudo ninja -C build install

# Version is set in root CMakeLists.txt: set(PROJECT_VERSION "x.y.z")
```

The repo also carries a CLion `cmake-build-debug/` tree (`.idea/` is present). All commands above assume the CLI `build/` dir — run `cmake -B build -G Ninja` first if only `cmake-build-debug/` exists.

## Testing & Debugging

Only one `kdisplaypresets_daemon` can connect to D-Bus at a time. Kill existing instances before starting a new build.

```bash
# Full test cycle
ninja -C build
killall kdisplaypresets_daemon
QT_LOGGING_RULES="kdisplaypresets.*=true" ./build/bin/kdisplaypresets_daemon

# Test individual components
kcmshell6 kcm_displaypresets                     # KCM module
# Plasmoid: Add "Display Presets" to system tray panel

# Check D-Bus service status
dbus-send --print-reply --dest=org.kde.kdisplaypresets / org.kde.kdisplaypresets.getPresets

# Daemon supports custom presets file for testing
./build/bin/kdisplaypresets_daemon --presets-file /tmp/test-presets.json
```

### Logging Categories

Enable targeted debug logging with `QT_LOGGING_RULES`:

| Category | Component |
|----------|-----------|
| `kdisplaypresets.common` | Presets model, Utils |
| `kdisplaypresets.daemon` | D-Bus daemon service |
| `kdisplaypresets.kcm` | System Settings KCM |
| `kdisplaypresets.applet` | System tray plasmoid |

Example: `QT_LOGGING_RULES="kdisplaypresets.daemon=true"` for daemon-only logs.

## High-Level Architecture

```
┌──────────────────────┐    ┌─────────────────────┐
│  System Settings KCM │    │  System Tray Widget  │
│  (kcm/)              │    │  (plasmoid/)         │
└──────────┬───────────┘    └──────────┬──────────┘
           │  D-Bus calls              │  D-Bus calls
           │  + monitors signal        │  + monitors signal
           └───────────┬───────────────┘
                       ▼
              ┌────────────────┐
              │  D-Bus Daemon  │  org.kde.kdisplaypresets
              │  (daemon/)     │  Methods: applyPreset(), getPresets()
              └───────┬────────┘  Signal: presetsChanged(changedPresets)
                      │
         ┌────────────┼────────────────┐
         ▼            ▼                ▼
   Presets model   KScreen API   KGlobalAccel
   (common/)       (libkscreen)  (shortcuts)
         │
         ▼
   ~/.config/kdisplaypresets/presets.json
```

### Core Components

**Common Library (`common/`)** — OBJECT library linked into daemon and KCM:
- `Presets` (`presets.h`) — `QAbstractListModel` managing preset data, persistence (JSON), and availability/current-status checks against connected monitors. Watches config file with `QFileSystemWatcher` for cross-component sync.
- `DisplayPreset` struct — holds id (UUID), name, description, timestamps, outputIds (hashMd5 values), configuration (QVariantMap of full KScreen config), and shortcut (QKeySequence).
- `Utils` (`utils.h`) — display naming via `outputName()` (two overloads for `KScreen::Output*` / `OutputPtr`, with optional serial-number and connector suffixes).

**D-Bus Daemon (`daemon/`)** — `PresetsService` class:
- Applies presets by mapping saved output configs onto current KScreen config, disabling outputs not in the preset.
- Monitors display changes via `KScreen::ConfigMonitor` with 500ms debounce timer.
- Manages per-preset global keyboard shortcuts via KGlobalAccel.
- Emits `presetsChanged()` D-Bus signal when availability/status changes.

**KCM Module (`kcm/`)** — `KCMDisplayPresets` + `PresetManager`:
- `PresetManager` handles business logic: saving current KScreen config as preset (UUID generation, output serialization), delete/rename/update operations.
- Calls daemon's D-Bus `applyPreset()` to apply presets (does not apply directly).
- QML UI in `kcm/ui/`: `main.qml` (preset list with save/edit/delete/apply), `PresetView.qml` (monitor layout visualization), `PresetOutput.qml` (individual monitor rendering).

**Plasmoid Widget (`plasmoid/`)** — `KDisplayPresetsApplet` + `PresetModel`:
- `PresetModel` wraps D-Bus data (from `getPresets()`) into QML-accessible model. Roles include `IsCurrentRole`, `IsAvailableRole`.
- Auto-hide: `Plasmoid.status = PassiveStatus` when ≤1 available preset and it's already active.
- QML: `main.qml` (widget container), `PresetList.qml` (preset list with hover preview popup using `PopupPlasmaWindow`), `PresetView.qml` + `PresetOutput.qml` (monitor layout).

### Key Data Flows

**Saving a preset**: KCM `savePreset()` → PresetManager serializes current KScreen config → Presets model writes JSON → daemon detects file change via watcher → emits D-Bus `presetsChanged()` → all UIs refresh.

**Applying a preset**: UI calls D-Bus `applyPreset(id)` → daemon gets current config via `GetConfigOperation` → maps preset outputs onto connected monitors → `SetConfigOperation` → updates lastUsed → emits `presetsChanged()`.

**Display change detected**: KScreen `ConfigMonitor` fires → daemon debounces (500ms) → refreshes preset availability/current status → emits `presetsChanged()`.

## Development Guidelines

### Code Style
- KDE Frameworks Coding Style (`.clang-format` in repo)
- C++20 standard
- GPL-2.0-or-later with SPDX license headers

### Dependencies
- Qt 6.8.0+ (Core, Quick, QuickControls2, Gui, DBus)
- KDE Frameworks 6.5.0+ (I18n, CoreAddons, Config, DBusAddons, XmlGui, GlobalAccel, KCMUtils, Screen, Notifications)
- Plasma 6.5.0+, KF6Screen

### Translation Domains

| Component | Domain |
|-----------|--------|
| Daemon | `kdisplaypresets_daemon` |
| Common | `kdisplaypresets_common` |
| KCM | `kcm_displaypresets` |
| Plasmoid | `plasma_applet_org.kde.kdisplaypresets` |

### Important Files
- `daemon/org.kde.kdisplaypresets.xml` — D-Bus interface definition
- `daemon/plasma-kdisplaypresets.service` — systemd user service template
- `kcm/kcm_displaypresets.json` — KCM metadata (category: `display`)
- `plasmoid/metadata.json` — Plasmoid metadata
- Configuration: `~/.config/kdisplaypresets/presets.json`

### Common Issues

**Daemon not starting**: Check if another instance is running on D-Bus. Only one can register at a time.

**Presets not appearing in widget**: Verify daemon is running (`dbus-send` test above). Check preset availability — all required monitors must be connected.

**Widget not hiding when expected**: Check auto-hide logic in `plasmoid/main.qml` (`Plasmoid.status` property). Verify `isAvailable` and `isCurrent` flags with debug logging.
