# AGENTS.md — Developer & AI Agent Guide for Tokri

Welcome to **Tokri**! This document provides technical context, architecture details, build procedures, and coding guidelines for AI agents and human developers working on this codebase.

---

## 1. Project Overview

**Tokri** (Hindi for *"Basket"*) is a lightweight, cross-platform desktop staging shelf / basket built with **C++17** and **Qt 6**.

### Core Concept & User Experience
- **Shake-to-summon**: While dragging text, URLs, images, or files with the mouse, shaking the cursor summons Tokri next to the pointer so the user can drop items into the basket.
- **Drag-out**: Dragging items out moves them to the target destination; holding `Ctrl` (Windows/Linux) or `⌥ Option` (macOS) copies them instead.
- **Persistent Local Basket**: Dropped files, text snippets, and downloaded URLs are persisted in a local directory (`~/Tokri` by default).
- **Single Instance**: Running a second instance signals the active instance via local socket IPC to wake up and surface near the cursor.
- **Tray & Background Lifecycle**: Tokri stays in the system tray when closed or minimized (`setQuitOnLastWindowClosed(false)`). Pressing `Escape` hides the basket into sleep mode.

---

## 2. Technology Stack & Platform Requirements

| Component | Technology / Library | Notes |
| :--- | :--- | :--- |
| **Language** | C++17 | Required (`CMAKE_CXX_STANDARD 17`) |
| **Framework** | Qt 6 (Core, Gui, Widgets, Network, Concurrent) | Optional: `Qt6::DBus` on Linux; On Windows: Qt 6 MSVC 2022 64-bit |
| **Windows Toolchain** | MSVC 2022 (Visual Studio 2022 C++ x64) | Uses MSVC 2022 compiler, CMake, and `jom` or `ninja` |
| **Build System** | CMake >= 3.16 | Uses `CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, `CMAKE_AUTORCC` |
| **Windows Hooks** | Win32 API (`user32`) | Low-level mouse hook (`WH_MOUSE_LL`) for drag-shake detection |
| **macOS Hooks** | CoreGraphics Framework | Event tap (`CGEventTapCreate`) for drag-shake detection |
| **Linux Hooks** | Linux `evdev` (`/dev/input/*`) | Requires user in `input` group; enforces X11/XCB backend |
| **Packaging** | Inno Setup (Windows), `macdeployqt` / DMG (macOS), Flatpak (Linux) | See packaging scripts in root and `scripts/` |

---

## 3. Repository Structure

```text
tokri/
├── assets/                          # Demo assets and preview media
├── docs/
│   └── DistroTest.md                # QA matrix & test notes across Linux distributions
├── resources/                       # Icons, assets, Qt resource bundle (.qrc), app metadata
│   ├── icons.qrc                    # Embedded Qt resources (icons, tray icon)
│   ├── com.ysuraj.Tokri.png / .ico  # Application icons
│   ├── com.ysuraj.Tokri.desktop     # Linux desktop entry
│   ├── com.ysuraj.Tokri.metainfo.xml.in
│   └── tokri.rc                     # Windows resource script for executable icon
├── scripts/
│   ├── build-linux.sh               # Flatpak build script
│   └── com.ysuraj.Tokri.yml         # Flatpak manifest definition
├── src/                             # C++ application source code
│   ├── main.cpp                     # Application entry point, setup, wiring, event loop
│   ├── tokriwindow.h / .cpp / .ui   # Main window GUI, geometry, shortcuts, selection slots
│   ├── dropawarefilesystemmodel.h/.cpp # Filesystem model with MIME drop handling
│   ├── drophandler.h / .cpp         # Background worker processing drops (files, images, URLs, text)
│   ├── filereveal.h / .cpp          # Native file manager integration (Explorer, Finder, Nautilus)
│   ├── loghelpers.h, logwindow.h/.cpp # Thread-safe logging facility and log viewer window
│   ├── nointernaldraglistview.h/.cpp# QListView preventing self-internal drop reordering
│   ├── shakedetector.h / .cpp       # OS-agnostic math/logic for mouse movement shake recognition
│   ├── sortfilterproxy.h / .cpp     # Custom proxy model sorting files by recency
│   ├── standardnames.h              # Centralized names (lock file, IPC socket, directory name)
│   ├── standardpaths.h / .cpp       # Resolves storage paths (~/Tokri, config, application paths)
│   ├── textfile.h / .cpp            # Utilities for reading/saving text files
│   ├── themeprovider.h / .cpp       # Theme management & dark/light palette synchronization
│   ├── thumbnaildelegate.h / .cpp   # QStyledItemDelegate rendering custom item cards & previews
│   ├── thumbnaillayout.h            # Card dimensions, padding, margins, icon sizes
│   ├── thumbnailproxymodel.h / .cpp # Asynchronous thumbnail loading proxy model
│   ├── windowicons.h / .cpp         # Window icon and theme color utilities
│   ├── linuxdragshakedetector.h/.cpp   # Linux evdev shake detector
│   ├── macosdragshakedetector.h/.cpp   # macOS CoreGraphics shake detector
│   └── windowsdragshakedetector.h/.cpp # Windows WH_MOUSE_LL shake detector
├── build-macos.sh                   # macOS bundle & DMG packager using macdeployqt and hdiutil
├── build-windows.bat                # Windows Qt deployment script using windeployqt
├── windows-installer-script.iss     # Inno Setup script creating TokriSetup.exe
├── CMakeLists.txt                   # CMake configuration defining executable & dependencies
├── README.md                        # User-facing README
└── CHANGELOG.md                     # Release change history
```

---

## 4. Key Architecture & Design Patterns

### 4.1. Threading Model
- **Main / GUI Thread**:
  - Handles UI events, `TokriWindow`, views, delegates, keyboard shortcuts, and tray icons.
  - Hosts `DropAwareFileSystemModel` which monitors the disk basket directory using Qt's file system watcher.
- **Worker Thread (`QThread` + `DropHandler`)**:
  - Heavy file operations (copying large files/directories, downloading remote links, parsing and writing binary images) run inside `DropHandler` on a separate `QThread`.
  - Communication between UI and `DropHandler` uses **queued signals/slots**:
    ```cpp
    QObject::connect(fsModel, &DropAwareFileSystemModel::dropReceived,
                     dropHandler, &DropHandler::handleDrop,
                     Qt::QueuedConnection);
    ```
  - When writing code, **never perform blocking disk I/O or network requests on the main thread**.

### 4.2. Model-View Pipeline
The items in the basket flow through a multi-tiered Qt model-view architecture:
1. `DropAwareFileSystemModel` (subclasses `QFileSystemModel`): Observes `~/Tokri`, accepts drops, emits `dropReceived`.
2. `ThumbnailProxyModel` (subclasses `QIdentityProxyModel`): Intercepts `Qt::DecorationRole` and asynchronously computes or caches thumbnails for images and documents.
3. `FSSortFilterProxy` (subclasses `QSortFilterProxyModel`): Sorts items descending by creation/modification time so newest dropped items appear at the top.
4. `NoInternalDragListView` (subclasses `QListView`): Customized list view providing smooth drag-out behavior while suppressing accidental reordering within the basket itself. Styled with `ThumbnailDelegate`.

### 4.3. Single Instance & IPC
- Uses `QLockFile` located at `QStandardPaths::TempLocation / com.ysuraj.Tokri.lock`.
- If another instance is running, a `QLocalSocket` connects to `QLocalServer` named `com.ysuraj.Tokri.ipc`.
- On connection, the primary instance triggers `TokriWindow::onShakeDetect()` to position itself near the mouse cursor and wake up.

### 4.4. OS Drag-Shake Detection
Each platform implements a shake detector that feeds relative X movements into the common `ShakeDetector` logic (`src/shakedetector.h`):
- **Windows**: `WindowsDragShakeDetector` spawns a worker thread running a Win32 message pump with `SetWindowsHookEx(WH_MOUSE_LL)`. Links with `user32`.
- **macOS**: `MacOSDragShakeDetector` uses CoreGraphics `CGEventTapCreate` to track mouse drag moves. Links with `-framework CoreGraphics`.
- **Linux**: `LinuxDragShakeDetector` reads `/dev/input/event*` devices via `poll()`. Requires the user to belong to the `input` group. Runs Qt under X11 (`QT_QPA_PLATFORM=xcb`).

---

## 5. Build & Packaging Instructions

### 5.1. Building with CMake (All Platforms)

```bash
# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

### 5.2. Building on Windows with Qt 6 & MSVC 2022

Tokri on Windows is compiled using **Qt 6 (msvc2022_64)** and **Microsoft Visual Studio 2022 (MSVC)** toolchain.

#### Prerequisites & Standard Tool Paths
- **Visual Studio 2022**: Community or Professional with *Desktop development with C++* workload (`vcvars64.bat` at `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`).
- **Qt 6 MSVC 2022**: e.g., `C:\Qt\6.12.0\msvc2022_64` (contains `qmake.exe`, `windeployqt.exe`, and CMake configs).
- **CMake**: `C:\Qt\Tools\CMake_64\bin\cmake.exe` (or system CMake >= 3.16).
- **Build Tool**: `jom` (`C:\Qt\Tools\QtCreator\bin\jom\jom.exe`) or `ninja` (`C:\Qt\Tools\Ninja\ninja.exe`).

#### Option A: Building via Qt Creator (Recommended)
1. Open `CMakeLists.txt` in Qt Creator.
2. Select the **Desktop Qt 6.x.x MSVC2022 64bit** kit.
3. Choose the **Release** build configuration.
4. Click **Build** (Ctrl+B). Build output is generated in `build/Desktop_Qt_6_x_x_MSVC2022_64bit_Release/`.

#### Option B: Building via Command Line (PowerShell)
```powershell
# 1. Add Qt tools, Jom, and Qt bin directory to PATH:
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\QtCreator\bin\jom;C:\Qt\6.12.0\msvc2022_64\bin;$env:PATH"

# 2. Configure with CMake:
cmake -B build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64" `
      -DCMAKE_BUILD_TYPE=Release

# 3. Build:
cmake --build build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release
```

#### Option C: Building via Visual Studio Developer Command Prompt / vcvars64
```cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.12.0\msvc2022_64\bin;%PATH%

cmake -B build -G "Ninja" -DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 5.3. Platform-Specific Deployment

- **Windows**:
  1. Build Release binary (`Tokri.exe`).
  2. Deploy Qt dependencies using `windeployqt`:
     ```cmd
     windeployqt --dir dist\Windows build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release\Tokri.exe
     copy /Y build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release\Tokri.exe dist\Windows\Tokri.exe
     ```
  3. Compile installer using Inno Setup:
     ```cmd
     iscc windows-installer-script.iss
     ```
     Generates `dist\installer\TokriSetup.exe`.

- **macOS**:
  1. Build Release bundle (`build/Release/Tokri.app`).
  2. Run `build-macos.sh` (executes `macdeployqt Tokri.app -always-overwrite` and creates DMG using `hdiutil`).
  3. Note: Unsigned binaries require users to clear quarantine (`xattr -dr com.apple.quarantine /Applications/Tokri.app`).

- **Linux**:
  1. Use Flatpak builder script:
     ```bash
     bash scripts/build-linux.sh
     ```
  2. Produces `dist/com.ysuraj.Tokri.flatpak`.

---

## 6. Development Rules & Guidelines for Agents

When making changes to this codebase, adhere strictly to the following conventions:

### 6.1. File & CMake Maintenance
- Always update [CMakeLists.txt](file:///c:/Workspace/tokri/CMakeLists.txt) when adding or removing source files (`.cpp` or `.h`).
  - Add cross-platform sources to `PROJECT_SOURCES` or the `Tokri` executable target.
  - Add OS-specific files to `WINDOWS_SOURCES`, `LINUX_SOURCES`, or `MACOS_SOURCES`.
- Keep header includes alphabetical or logically grouped (local headers first, followed by Qt modules, followed by standard library / system headers).

### 6.2. Qt & C++ Idioms
- **Memory Management**: Always pass appropriate `QObject` parent pointers to ensure automatic lifecycle cleanup. For QObjects moved to other threads, use `deleteLater()` or connect to `QThread::finished`.
- **Signals and Slots**: Prefer modern pointer-to-member-function syntax (`connect(sender, &Sender::signal, receiver, &Receiver::slot)`).
- **MIME Data**: When handling or creating `QMimeData`, ensure all relevant data flavors (`text/uri-list`, `text/plain`, `image/png`) are handled gracefully.
- **Cross-Platform Conditionals**:
  - Use `Q_OS_WIN`, `Q_OS_MACOS`, and `Q_OS_LINUX` macros for platform-specific blocks.
  - Never introduce hardcoded file separators; use `QDir`, `QFileInfo`, or `/`.

### 6.3. Thread Safety & Logging
- **Thread Safety**: Never interact with UI widgets or invoke UI-bound methods from `DropHandler` or worker threads. Emit signals to communicate back to the main thread.
- **Logging**: Use the centralized thread-safe logger:
  ```cpp
  #include "loghelpers.h"
  Logger::instance().log() << "Action performed with param=" << value;
  ```
  Users can inspect logs at runtime via `TokriWindow` -> `Logs` action (`LogWindow`).

### 6.4. Testing Changes
- Because Tokri is a desktop GUI utility with low-level OS hooks, verify behavior against:
  - Dragging files into Tokri (both single and multiple files).
  - Dragging web links and text snippets from browsers into Tokri.
  - Dragging files out into desktop folders / file manager (verifying move vs copy with Ctrl/Option).
  - Window behavior: summoning via shake, minimizing with `Escape`, opening from system tray.
  - Refer to [docs/DistroTest.md](file:///c:/Workspace/tokri/docs/DistroTest.md) for existing test results and regression tracking.
