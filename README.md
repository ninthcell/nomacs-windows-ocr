# nomacs Windows OCR Plugin

A [nomacs](https://nomacs.org) plugin that detects text in images using the Windows built-in OCR API (`Windows.Media.Ocr`), the same engine used by the Windows Photos app.

## Features

- Text detection with bounding box overlay
- Click a word to copy it to clipboard
- Drag to select multiple words, auto-copied on release
- Ctrl+A to select all, Ctrl+C to copy
- ESC to close, auto-closes on image change
- Zoom/scroll pass-through to nomacs viewport

## Requirements

- Windows 10/11
- Visual Studio 2019+ (MSVC with C++17)
- Windows SDK 10.0.17763+
- Qt 6.x
- OpenCV 4.x
- nomacs 3.x built from source

## Build

1. Build [nomacs](https://github.com/nomacs/nomacs) from source first.

2. Configure with CMake, pointing to your nomacs build:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 \
    -DNOMACS_BUILD_DIRECTORY="C:/path/to/nomacs/build" \
    -DQT_QMAKE_EXECUTABLE="C:/path/to/Qt/bin/qmake.exe"
```

3. Build:

```bash
cmake --build build --config Release
```

The resulting `OcrPlugin.dll` is automatically placed in `<nomacs-build>/Release/plugins/`.

## Install

Copy `OcrPlugin.dll` to nomacs's `plugins/` directory (next to `nomacs.exe`).
