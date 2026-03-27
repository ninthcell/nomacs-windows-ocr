# nomacs Windows OCR Plugin

A [nomacs](https://nomacs.org) plugin that detects and copies text from images using the built-in Windows OCR engine (`Windows.Media.Ocr`).

![demo image](assets/demo.png)

## Features

- Text detection with bounding box overlay
- Click a word to copy it to clipboard
- Drag to select multiple words, auto-copied on release
- Ctrl+A to select all, Ctrl+C to copy
- Zoom/pan pass-through while plugin is active
- Automatic image upscaling and contrast enhancement for better recognition
- Esc to close the plugin

## Install (prebuilt)

1. Download `OcrPlugin.dll` from the [latest release](../../releases/latest) or [Actions artifacts](../../actions).
2. Copy it into the nomacs `plugins/` folder:
   - Installed nomacs: `C:\Program Files\nomacs\bin\plugins\`
   - Portable/dev build: `<nomacs-build>\Release\plugins\`
3. Restart nomacs. The plugin appears under **Plugins > Windows OCR**.

## Usage

1. Open an image in nomacs.
2. Go to **Plugins > Windows OCR**.
3. Wait for OCR to finish (a "Recognizing text..." indicator is shown).
4. Blue boxes appear over detected words.
5. **Click** a word or **drag** to select multiple words — text is copied to clipboard automatically.
6. Press **Esc** to close the plugin.

## Build from source

### Requirements

- Windows 10/11 with OCR language packs installed
- Visual Studio 2022+ (MSVC, C++17)
- CMake 3.16+
- Qt 6.x
- OpenCV 4.x
- [nomacs](https://github.com/nomacs/nomacs) source + built `nomacsCore` library

### Steps

```bash
# 1. Clone nomacs and build nomacsCore
git clone https://github.com/nomacs/nomacs.git
cmake -S nomacs/ImageLounge -B nomacs-build -A x64 \
    -DCMAKE_PREFIX_PATH="<Qt-prefix>;<OpenCV-prefix>" \
    -DENABLE_RAW=OFF -DENABLE_TIFF=OFF -DENABLE_QUAZIP=OFF \
    -DENABLE_TRANSLATIONS=OFF -DENABLE_PLUGINS=OFF -DENABLE_TESTING=OFF
cmake --build nomacs-build --target nomacsCore --config Release

# 2. Clone and build the plugin
git clone https://github.com/<you>/nomacs-windows-ocr.git
cmake -S nomacs-windows-ocr -B plugin-build -A x64 \
    -DCMAKE_PREFIX_PATH="<Qt-prefix>;<OpenCV-prefix>" \
    -DNOMACS_BUILD_DIRECTORY="<path-to>/nomacs-build" \
    -DNOMACS_INCLUDE_DIRECTORY="<path-to>/nomacs/ImageLounge/src/DkCore" \
    -DNOMACS_LIBS="<path-to>/nomacs-build/libs/Release/nomacsCore.lib" \
    -DDLL_CORE_NAME=nomacsCore
cmake --build plugin-build --config Release
```

The resulting `OcrPlugin.dll` is placed in `<nomacs-build>/Release/plugins/`.

## CI

Pushes to `main` automatically build the plugin via GitHub Actions. Download the artifact from the **Actions** tab.

## License

Same license as [nomacs](https://github.com/nomacs/nomacs) (GPLv3).
