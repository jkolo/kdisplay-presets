# Głęboki review kodu — kdisplay-presets (v1.0.5)

**Data:** 2026-06-22 · **Zakres:** szukanie błędów + zgodność z Plasma 6.5 / 6.6 / 6.7
**Metoda:** lektura całego kodu (3116 linii) + research ze źródłami pierwotnymi (invent.kde.org, zainstalowane nagłówki KF6Screen 6.7.0, doc.qt.io, qmltypes KF6) + adwersaryjna weryfikacja każdego findingu (27 potwierdzonych, 5 obalonych).

---

## STATUS IMPLEMENTACJI (2026-06-22)

**Wszystkie 24 unikalne findingi (3 High + 5 Medium + 16 Low) zostały wdrożone.** Build (`ninja -C build`) przechodzi na zielono (0 błędów, 0 ostrzeżeń); `qmllint` na wszystkich plikach QML czysty.

Uwagi z implementacji:
- **L14 dało realny efekt uboczny:** podniesienie `KDE_COMPILERSETTINGS_LEVEL` do 6.5.0 włączyło `QT_NO_CAST_FROM_ASCII`, co **zepsuło build** na istniejących gołych literałach w `daemon/main.cpp:24` (`"p"`, `"presets-file"`, `"file"`). Weryfikator review błędnie twierdził, że L14 jest build-safe. Naprawione przez `QStringLiteral(...)` — kod jest teraz zgodny z nowoczesnym stylem KDE.
- **M4** zaimplementowano jako `Q_SCRIPTABLE` + wpis w XML **oraz** `KNotification` po stronie daemona (pokrywa też ścieżkę skrótu globalnego bez otwartego UI). Dodano zależność `KF6::Notifications`.
- **L1** rozwiązano przez śledzenie „ostatnio znanej zawartości" pliku (`m_lastKnownData`) — tłumi zarówno self-write feedback, jak i podwójny event przy atomowym rename (`fileChanged` + `directoryChanged`).
- **L5** zaadresowano pośrednio przez M1 (refresh jest teraz asynchroniczny, więc odrzucanie payloadu nie powoduje już blokady); inkrementalnej konsumpcji payloadu nie wdrożono (advisor: opcjonalne).
- Dodatkowo: usunięto martwy sygnał `outputConnect` w KCM (jego jedyny konsument w QML zniknął wraz z `_refreshTrigger`).

### Stan weryfikacji (uczciwie)

- ✅ **Kompilacja:** `ninja -C build` zielony, 0 błędów / 0 ostrzeżeń (daemon + KCM + plasmoid). `qmllint` czysty na wszystkich plikach QML.
- ✅ **Smoke-test headless** (świeży daemon na prywatnej szynie `dbus-run-session`, bez zakłócania żywej sesji usera): **H1 potwierdzone runtime** (daemon wystartowany bez pliku podchwytuje preset utworzony z zewnątrz), **M3 potwierdzone** (kolejne edycje zewnętrzne łapane), **ścieżka błędu M4** nie wywala daemona.
- ⚠️ **NIE zweryfikowane runtime:** **H2** (10 nowych setterów na realnym sprzęcie — jednostki/zakresy `setBrightness`/`setHdrEnabled`/`setSdrBrightness` wymagają sesji Plasma + monitorów), **M2** (`QSaveFile` — logika sprawdzona, ścieżka zapisu daemona nieuruchomiona w teście), **L1** (supresja self-write wymaga *udanego* apply, więc zweryfikowana logiką + kompilacją, nie runtime), oraz cały plasmoid/KCM w żywym UI.
- Zgodnie z [[no-test-suite]]: brak automatycznych testów — testy manualne na realnej sesji Plasma (zwłaszcza H2) pozostają do wykonania.
- **Nic nie jest zacommitowane** — 19 zmodyfikowanych plików + nowy `REVIEW-plasma-6.5-6.7.md` czekają w drzewie roboczym (commit tylko na wyraźną prośbę).

Pozostałe sekcje opisują znalezione problemy i zastosowane podejście.

---

## TL;DR — zgodność z Plasma 6.5/6.6/6.7

**Nie ma żadnych przełomowych zmian API uderzających w ten kod.** Zweryfikowano przeciw źródłom pierwotnym (tag `v6.5.0` libplasma, changelogi 6.6/6.6.90, zainstalowane nagłówki):

- `PlasmoidItem`, `Plasmoid.status`, `Plasmoid.setInternalAction`, `fullRepresentation`, `toolTipSubText` — **OK**, publiczne i stabilne 6.5–6.7.
- `PlasmaCore.PopupPlasmaWindow` (hover-preview) — **publiczne, udokumentowane qdoc, obecne w `v6.5.0`**; w 6.6 dostało nawet poprawki (`qmlWarning`, focus). **Nie jest API prywatnym.**
- `PlasmaCore.Action`, `PlasmaCore.Types.PassiveStatus/ActiveStatus`, metadata.json (`X-Plasma-API-Minimum-Version` itd.), `kcmutils` (`SimpleKCM`, `KCMLauncher`, `KQuickConfigModule`) — **OK**.
- **Kirigami** (`Dialog`, `PromptDialog` z `customFooterActions`, `Action`, `FormLayout`+`FormData`, `PlaceholderMessage`, `Units`, `Theme`, `Icon`) — **OK**, zweryfikowane na obu końcach zakresu (tagi `v6.16.0` i `v6.27.0` invent.kde.org/frameworks/kirigami); `customFooterActions` jest bajt-identyczny, zero markerów deprecation.
- Bezwersyjne importy QML (`import org.kde.plasma.plasmoid`) to nowoczesny, przyszłościowy styl (libplasma usuwa wersje importów w 6.6.90).

**Wniosek:** obawa o regresje 6.5/6.6/6.7 jest w dużej mierze bezpodstawna. Realne problemy poniżej to **istniejące bugi i dług techniczny**, nie skutki zmian wersji. Trzy elementy mają wymiar „wersyjny", ale żaden to regresja Plasmy:
- `Qt.DefaultLocaleShortDate` — usunięty w **Qt 6.0** (projekt celuje w 6.8), działa przez przypadek.
- `ShortcutType.Global` — nieprawidłowy enum (był błędny od początku, nie zmiana wersji).
- Brakujące settery libkscreen (F2) — wszystkie istnieją od KF6 6.0–6.x, więc to luka w użyciu API, nie niezgodność.

---

## Priorytet WYSOKI

### H1 — Daemon nie obserwuje `presets.json`, jeśli plik nie istnieje przy starcie
**Plik:** `common/presets.cpp:27-31`, `412-415` · **pewność: wysoka**

Konstruktor `Presets` wywołuje `m_fileWatcher->addPath(filePath)` tylko gdy `QFile::exists(filePath)`. `onPresetFileChanged` może odpalić wyłącznie, gdy watcher już coś obserwuje — nie potrafi „samoczynnie" rozpocząć obserwacji. Re-dodanie ścieżki w `savePresetsToDisk` dzieje się tylko w instancji, która pisze (KCM), **nigdy w daemonie**.

**Skutek:** na świeżej instalacji (brak `presets.json`) daemon startuje bez żadnej obserwacji. Pierwszy preset utworzony w KCM jest **niewidoczny dla działającego daemona (i plasmoidu, który czyta z daemona) aż do restartu daemona** — brak sygnału `presetsChanged`, brak rejestracji skrótów.

**Poprawka:** obserwować **katalog nadrzędny**, nie plik:
1. W konstruktorze: `QDir().mkpath(dirPath)` (katalog może nie istnieć na czystym systemie — `QFileSystemWatcher` nie obserwuje nieistniejących ścieżek), potem `addPath(dirPath)` i połączyć `QFileSystemWatcher::directoryChanged`.
2. W handlerze `directoryChanged`: jeśli `QFile::exists(filePath)` → (re-)dodać ścieżkę pliku, `loadPresetsFromDisk()`, `emit presetsChanged()`. Strzec `QFile::exists` (directoryChanged odpala też dla nieistotnej aktywności w katalogu).
3. Zachować dotychczasową ścieżkę `fileChanged` dla kolejnych modyfikacji.

---

### H2 — `applyPresetToOutput` gubi zapisane właściwości (HDR/WCG/jasność/overscan/VRR/RGB/ICC/auto-rotate/EDR)
**Plik:** `daemon/presetsservice.cpp:296-328` vs `kcm/preset_manager.cpp:177-188` · **pewność: wysoka**
**Źródło:** `/usr/include/KF6/KScreen/kscreen/output.h` (6.7.0): settery linie 464-610, enum `Capability` linie 117-133.

`configToVariantMap` **zapisuje** 10 zaawansowanych właściwości: `overscan`, `vrrPolicy`, `rgbRange`, `hdr`, `sdr_brightness`, `wide_color_gamut`, `icc_profile_path`, `brightness`, `auto_rotate_policy`, `edr_policy`. `applyPresetToOutput` **przywraca tylko** `enabled`/`currentModeId`/`scale`/`rotation`/`pos`/`priority`. Grep całego repo na `set(Overscan|VrrPolicy|...)` → **zero trafień**.

**Skutek:** przełączenie na preset zapisany z np. włączonym HDR / konkretną jasnością / overscanem — **żadna z tych właściwości nie zostaje przywrócona**. Preset nie odtwarza wiernie konfiguracji, mimo że dane są w pliku. Wszystkie 10 setterów istnieje i jest honorowanych przez `SetConfigOperation` (ta sama ścieżka, której używa natywny KCM ekranów KScreen).

**Poprawka:** w `applyPresetToOutput`, po bloku `enabled` (dla włączonych wyjść), dodać settery — każdy strzeżony bitem `Capability` z `output->capabilities()` i `presetOutputMap.contains(key)` (by stare presety bez tych kluczy nie nadpisywały stanu sprzętu wartościami domyślnymi):

| klucz w mapie | setter | typ | Capability guard |
|---|---|---|---|
| `overscan` | `setOverscan(toUInt())` | uint32 | `Overscan` |
| `vrrPolicy` | `setVrrPolicy(static_cast<...::VrrPolicy>(toInt()))` | enum | `Vrr` |
| `rgbRange` | `setRgbRange(static_cast<...::RgbRange>(toInt()))` | enum | `RgbRange` |
| `hdr` | `setHdrEnabled(toBool())` | bool | `HighDynamicRange` |
| `sdr_brightness` | `setSdrBrightness(toUInt())` | uint32 | `HighDynamicRange` |
| `wide_color_gamut` | `setWcgEnabled(toBool())` | bool | `WideColorGamut` |
| `icc_profile_path` | `setIccProfilePath(toString())` | QString | `IccProfile` |
| `brightness` | `setBrightness(toDouble())` | double | `BrightnessControl` |
| `auto_rotate_policy` | `setAutoRotatePolicy(static_cast<...::AutoRotatePolicy>(toInt()))` | enum | `AutoRotation` |
| `edr_policy` | `setEdrPolicy(static_cast<...::EdrPolicy>(toInt()))` | enum | brak bitu — gate przez `HighDynamicRange` |

**NIE** dodawać `setExplicitLogicalSize` — backend (`setWlConfig`) nigdy go nie wysyła (logical size wynika z mode×scale); `logicalSize`/`capabilities` zostają metadanymi tylko do podglądu.

---

### H3 — Auto-hide plasmoidu (`Plasmoid.status`) nie reaguje na zmiany — zamarza na stanie startowym
**Plik:** `plasmoid/main.qml:30-55` · **pewność: wysoka** (severity graniczne H/M)
**Źródło:** doc.qt.io/qt-6/qtqml-syntax-propertybinding.html

Binding `Plasmoid.status` liczy auto-hide iterując `Plasmoid.presetModel.rowCount()/index()/data()`. Wywołania metod (są `Q_INVOKABLE` w Qt6, więc same w sobie działają) **nie tworzą zależności bindingu**, a `presetModel` jest `Q_PROPERTY(... CONSTANT FINAL)` (`kdisplaypresets_applet.h:58`) — nigdy nie notyfikuje. Model jest odświeżany przez `beginResetModel/endResetModel` w `refreshPresets()` na każdy `presetsChanged`, ale nic w QML nie nasłuchuje `modelReset`.

**Skutek:** binding ewaluuje się **raz** (przy starcie, gdy model bywa pusty) i nigdy więcej. Auto-hide zamarza na stanie startowym — widget nie chowa się/pokazuje przy hotplugu monitora, zastosowaniu presetu, dodaniu/usunięciu. (Dla kontrastu `PresetList.qml:38-39` JEST reaktywny, bo czyta `model.isAvailable`/`model.isCurrent` jako role — to wzorzec poprawki.)

**Poprawka (zalecana, idiomatyczna):** dodać NOTIFY-owalne właściwości skalarne w C++ `PresetModel`:
```cpp
Q_PROPERTY(int availableCount READ availableCount NOTIFY countsChanged)
Q_PROPERTY(bool hasActiveCurrent READ hasActiveCurrent NOTIFY countsChanged)
```
liczone przez iterację `m_presets`, z `emit countsChanged()` zaraz po `endResetModel()` w `refreshPresets()`; związać `Plasmoid.status` z tymi właściwościami. (Alternatywa: `Connections { target: Plasmoid.presetModel; function onModelReset() {...} }` + `Component.onCompleted` dla stanu początkowego.)

---

## Priorytet ŚREDNI

### M1 — Blokujące synchroniczne `getPresets()` na wątku UI plasmashell
**Plik:** `plasmoid/kdisplaypresets_applet.cpp:146` · **pewność: wysoka**
**Źródło:** doc.qt.io — `QDBus::Block` ("block on network operations ... UI may not be updated"); domyślny timeout -1 ≈ 25 s.

`PresetModel::refreshPresets()` używa synchronicznego `m_presetsInterface->call("getPresets")` (blokujące `QDBus::Block`) na wątku głównym plasmashell, wywoływane z 3 miejsc: konstruktor `PresetModel` (l. 68), `init()` (l. 34), handler sygnału `onPresetAvailabilityChanged()` (l. 175). Jeśli daemon jest wolny/zawieszony/w trakcie startu — **cały panel Plasmy zamarza** (do 25 s). Niespójne z resztą kodu: `loadPreset` (l. 49) i KCM `loadPreset` (`kcm_displaypresets.cpp:90`) używają już `asyncCall`.

> W praktyce wywołanie trafia do lokalnego daemona w tej samej sesji — latencja sub-ms; zamrożenie materializuje się tylko gdy daemon naprawdę zawiśnie. Stąd severity średni, nie wysoki.

**Poprawka:** `asyncCall("getPresets")` + `QDBusPendingCallWatcher::finished`; przenieść deserializację i `begin/endResetModel` do slotu watchera; `QDBusPendingReply<QVariantList>` + `reply.isError()`; `watcher->deleteLater()`. (Uwaga adjacentna: konstruktor `QDBusInterface` też robi blokujący introspekcyjny round-trip przy tworzeniu apletu.)

### M2 — Nieatomowy zapis `presets.json` — utrata danych / korupcja
**Plik:** `common/presets.cpp:367-410` · **pewność: wysoka**

Dwie osobne instancje `Presets` (daemon przez `updateLastUsed`, KCM przez save/delete/rename) piszą ten sam plik pełnym, obcinającym `QFile::open(WriteOnly)` — bez `temp+rename` i bez blokady. `updateLastUsed` daemona ścigające edycję w KCM → last-writer-wins (utrata zmiany). Crash/zanik zasilania w trakcie zapisu → obcięty, niepoprawny JSON, którego `loadPresetsFromDisk` nie sparsuje → **utrata wszystkich presetów**.

**Poprawka:** `QSaveFile` (temp + atomowy rename + fsync) zamiast `QFile`; sprawdzać `commit()`, emitować `savingFailed` przy błędzie. **Musi iść w parze z poprawką watchera (H1/M3)** — rename podmienia inode i zrzuca file-level watch. `QSaveFile` rozwiązuje korupcję i atomowość pojedynczego zapisu, ale **nie** cross-process lost-update — jeśli to ważne, dodać `QLockFile` wokół load-modify-save.

### M3 — `onPresetFileChanged` przy chwilowym braku pliku gubi watch bezpowrotnie
**Plik:** `common/presets.cpp:428-438` · **pewność: wysoka**
**Źródło:** doc.qt.io/qt-6/qfilesystemwatcher.html#fileChanged

Gdy `onPresetFileChanged` odpala a `QFile::exists` jest false, handler czyści presety i **wraca (l. 437) bez re-dodania ścieżki**. Qt usuwa ścieżkę z watchera po skasowaniu/podmianie pliku. Dziś łagodne (zapisy są in-place, nie kasują inode), ale **staje się aktywnym zagrożeniem po wdrożeniu `QSaveFile`** (atomowy rename może objawić się jako event delete) → po nim watcher daemona jest pusty i nigdy się nie podnosi.

**Poprawka:** obserwować katalog nadrzędny (ta sama poprawka co H1) — watch na katalogu przeżywa kasowanie/podmianę/rename.

### M4 — `errorOccurred` nie jest eksportowany na D-Bus — błędy apply są ciche
**Plik:** `daemon/presetsservice.h:39` · **pewność: wysoka**

Daemon emituje `errorOccurred` na każdej ścieżce porażki apply (l. 124/141/151/160/193), ale sygnał jest zadeklarowany **bez `Q_SCRIPTABLE`**, a obiekt zarejestrowany tylko z `ExportScriptableSignals` → sygnał **nigdy nie trafia na bus** (nie ma go w `org.kde.kdisplaypresets.xml`). `applyPreset` jest `void` (brak reply). **Nieudane zastosowanie presetu (np. odrzucone przez KWin) jest całkowicie ciche dla plasmoidu i KCM.**

**Poprawka:** dodać `Q_SCRIPTABLE` do `errorOccurred` + wpis `<signal>` w XML + handler po stronie klienta (dodać arg `presetId` dla korelacji). Czystsza alternatywa: `applyPreset` zwraca status/error przez `QDBusPendingReply`.

### M5 — `checkForConflictsAgainst: ShortcutType.Global` to nieprawidłowy enum
**Plik:** `kcm/ui/main.qml:212` · **pewność: wysoka**
**Źródło:** `/usr/lib/qt6/qml/org/kde/kquickcontrols/kquickcontrols.qmltypes` — `ShortcutType` ma tylko `None`, `StandardShortcuts`, `GlobalShortcuts`.

Brak członka `Global` → wyrażenie to `undefined` → koercja do `0` (== `None`) → **sprawdzanie konfliktów ze skrótami globalnymi jest po cichu wyłączone**. A presety rejestrują realne skróty globalne przez `KGlobalAccel` (`presetsservice.cpp:344`), więc użytkownik może po cichu przypisać kolidujący skrót.

**Poprawka:** `checkForConflictsAgainst: ShortcutType.GlobalShortcuts` (zmiana jednego tokenu).

---

## Priorytet NISKI / dług techniczny

| # | Plik | Problem | Poprawka |
|---|---|---|---|
| L1 | `daemon/presetsservice.cpp:40,262-281` | Self-write feedback: każdy zapis (m.in. `updateLastUsed` po apply) → pełny `beginResetModel` + re-rejestracja **wszystkich** skrótów globalnych. (Skróty użytkownika przeżywają — `KGlobalAccel` Autoloading; to churn + reset stanu widoku QML, nie utrata danych.) | Odróżnić zapisy własne (hash/mtime, nie flaga — `fileChanged` jest async); dla `lastUsed` użyć `dataChanged` jednego wiersza zamiast resetu. |
| L2 | `common/presets.cpp:364,446-447` | `presetsChanged` emitowany **dwukrotnie** na zmianę pliku (`loadPresetsFromDisk` + `onPresetFileChanged`). | Usunąć zbędny `Q_EMIT` w l. 447. **Nie ruszać** emitu w gałęzi kasowania (433-437). |
| L3 | `kcm/ui/main.qml:19-29,99-165` | Hack `_refreshTrigger` — status jako metody `Q_INVOKABLE` zamiast ról; działa, ale kruchy (każdy nowy binding musi pamiętać o `_refreshTrigger;`). Ten sam root-cause co H3. | Dodać `IsAvailableRole`/`IsCurrentRole` do wspólnego `common/Presets` (enum+roleNames+data); `refreshPresetStatus()` już emituje `dataChanged`. Bindować `model.isAvailable/isCurrent`, usunąć `_refreshTrigger`. |
| L4 | `kcm/ui/main.qml:86` | `Qt.DefaultLocaleShortDate` usunięty w Qt 6.0 → `undefined` (działa przez przypadek: fallback do `Locale.ShortFormat`). | `Qt.formatDate(model.lastUsed, Locale.ShortFormat)`. |
| L5 | `plasmoid/kdisplaypresets_applet.cpp:60` | Payload sygnału `presetsChanged` (granularne `changedPresets` z flagą `deleted`) jest odrzucany — handler i tak robi pełny `getPresets()`. Nakłada się na M1. | Słot przyjmujący `QVariantList` + inkrementalna aktualizacja **albo** (min.) async refresh. |
| L6 | `plasmoid/kdisplaypresets_applet.cpp:237` | Ręczna deserializacja `QDBusArgument` jest konieczna (a{sv} demarshaluje 1 poziom), ale hardcodowana lista `{pos,mode,logicalSize}` → każde przyszłe zagnieżdżone pole przyjdzie jako `undefined`. Dwa `qDBusRegisterMetaType` (presetsservice.cpp:51-52) to no-opy. | Wysyłać `configuration` jako string JSON (typ `s`) i parsować `QJsonDocument` po stronie klienta — eliminuje 3 helpery i listę. Lub generyczna rekurencja po `canConvert<QDBusArgument>`. Usunąć zbędne `qDBusRegisterMetaType`. |
| L7 | `plasmoid/PresetOutput.qml:81`, `kcm/ui/PresetOutput.qml:80` | Kruchy `parent.parent.parent.height` (działa, ale wstawienie wrappera po cichu zmieni źródło). | `presetOutput.height / 8` (i `outline.height` dla tekstów w Column). |
| L8 | `plasmoid/PresetView.qml:8-9`, `PresetOutput.qml:8-9`, `kcm/ui/PresetView.qml:8-9`, `PresetOutput.qml:8-9` | Martwe importy `QtQuick.Layouts` i `QtQuick.Controls as QQC2` we wszystkich 4 plikach. | Usunąć obie linie z 4 plików. |
| L9 | `plasmoid/PresetList.qml:94`, `kcm/ui/main.qml:70` | `i18nc("...","%1 display(s)",n)` — `(s)` nie obsługuje liczby mnogiej. **Istotne dla polskiego** (3 formy liczby mnogiej). | `i18ncp("@info", "%1 display", "%1 displays", model.outputCount || 0)`. |
| L10 | `plasmoid/main.qml:7` | `pragma ComponentBehavior: Bound` jest bezpieczny dziś (file-scoped), ale dodanie go do `PresetList.qml`/`kcm/ui/main.qml` bez konwersji `model.*` na `required property` po cichu zepsuje delegaty. | Zostawić; przy modernizacji najpierw konwertować na `required property`. Dodać komentarz. |
| L11 | `plasmoid/kdisplaypresets_applet.cpp:58` | Brak `QDBusServiceWatcher` — jeśli daemon zrestartuje/crashnie, plasmoid nie podłączy ponownie sygnału. (D-Bus activation łagodzi pierwszy start → severity niski.) | Dodać `QDBusServiceWatcher` na `serviceRegistered` → reconnect + `refreshPresets`. |
| L12 | `CMakeLists.txt:56` | `find_package(Plasma ${KF_MIN_VERSION})` wiąże wersję Plasmy (6.7) z numerem KF (6.27) — różne osie wersji. Działa przypadkiem. | `set(PLASMA_MIN_VERSION 6.5.0)` + `find_package(Plasma ${PLASMA_MIN_VERSION})`. |
| L13 | `CMakeLists.txt:42` | `Qt6::Test` jest `REQUIRED`, ale nigdy nielinkowany/nieużywany (brak testów). | Usunąć `Test` z `find_package`; gdy testy — `if(BUILD_TESTING)` + `ecm_add_test`. Zaktualizować CLAUDE.md. |
| L14 | `CMakeLists.txt:10` | `KDE_COMPILERSETTINGS_LEVEL 5.82` **obniża** ustawienia poniżej domyślnego ECM 6.5 — wyłącza `-pedantic`, `-Wzero-as-null-pointer-constant`, `-Wmissing-include-dirs`, `QT_NO_KEYWORDS`. (Zweryfikowano: nie psuje builда — brak `-Werror`, kod jest `QT_NO_KEYWORDS`-zgodny.) | Usunąć `set()` (domyśli się 6.5.0) lub `set(KDE_COMPILERSETTINGS_LEVEL 6.5.0)`. |
| L15 | `plasmoid/metadata.json:18` | `Version: "1.0"` ≠ `PROJECT_VERSION 1.0.5` (driftuje od pierwszego commita). | Podbić do `1.0.5`; dodać do checklisty „Bump version" lub szablonować przez `configure_file`. |
| L16 | `CMakeLists.txt:60-63` | `ecm_set_disabled_deprecation_versions(QT 6.7.2 ...)` < `QT_MIN_VERSION 6.8.0`. | `ecm_set_disabled_deprecation_versions(QT ${QT_MIN_VERSION} KF 6.14.0)`. |

---

## Co sprawdzono i ODRZUCONO (po weryfikacji — nie są błędami)

- **`detectChangedPresets` pomija `configuration`** → nie-bug: `savePreset` zawsze ustawia `lastUsed = currentDateTime()`, a `lastUsed` JEST porównywany — zmiana configu zawsze współ-zmienia `lastUsed`.
- **`ConfigMonitor::addConfig` bez `removeConfig`** → nie-bug: libkscreen 6.7 trzyma `QWeakPointer`, deduplikuje i auto-czyści martwe wpisy. Brak wycieku.
- **Disabled output bez jawnego `priority`** → nie-bug: `setOutputPriority`/`adjustPriorities()` w libkscreen same ustawiają priorytet wyłączonych wyjść na 0.
- **Brak autostart `.desktop` dla daemona** → nie-bug: łańcuch D-Bus activation + systemd (`Type=dbus`, `BusName`) jest poprawnie okablowany.
- (`no-daemon-reconnect` zdegradowany do L11 powyżej.)

---

## Sugerowana kolejność wdrożenia

1. **H1 + M3** razem (obserwacja katalogu) — fundament dla M2.
2. **M2** (`QSaveFile`) — po H1/M3.
3. **H2** (settery apply) — największa wartość funkcjonalna.
4. **H3** + **L3** (reaktywność przez role — wspólny root-cause).
5. **M1** + **L5** (async D-Bus).
6. **M4**, **M5**, potem L-ki wg uznania (L9 ważny dla polskiego UI, L4/L8/L13/L14/L15/L16 to szybkie porządki).
