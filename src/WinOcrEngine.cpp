#include "WinOcrEngine.h"

#ifdef Q_OS_WIN

#include <QDebug>

#include <Unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>

// IMemoryBufferByteAccess for raw pixel access to SoftwareBitmap
struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable)
    IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t **value, uint32_t *capacity) = 0;
};

using namespace winrt;
using namespace winrt::Windows::Graphics::Imaging;
namespace WinOcr = winrt::Windows::Media::Ocr;

namespace
{

// RAII guard for COM initialization on worker threads
struct ComGuard {
    bool initialized = false;

    ComGuard()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        initialized = SUCCEEDED(hr);
    }

    ~ComGuard()
    {
        if (initialized)
            CoUninitialize();
    }

    ComGuard(const ComGuard &) = delete;
    ComGuard &operator=(const ComGuard &) = delete;
};

// Increase contrast of the image to help OCR with faded/low-contrast text.
QImage enhanceContrast(const QImage &src)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    // Simple auto-levels: find min/max luminance, stretch to full range.
    int minL = 255, maxL = 0;
    for (int y = 0; y < img.height(); y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++) {
            int gray = qGray(line[x]);
            if (gray < minL) minL = gray;
            if (gray > maxL) maxL = gray;
        }
    }

    if (maxL - minL < 100 && maxL > minL) {
        // Only enhance if contrast is actually low
        double factor = 255.0 / (maxL - minL);
        for (int y = 0; y < img.height(); y++) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); x++) {
                int r = qBound(0, static_cast<int>((qRed(line[x]) - minL) * factor), 255);
                int g = qBound(0, static_cast<int>((qGreen(line[x]) - minL) * factor), 255);
                int b = qBound(0, static_cast<int>((qBlue(line[x]) - minL) * factor), 255);
                line[x] = qRgba(r, g, b, qAlpha(line[x]));
            }
        }
    }
    return img;
}

SoftwareBitmap qImageToSoftwareBitmap(const QImage &qimg, double &scale)
{
    QImage img = enhanceContrast(qimg);
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int origWidth = img.width();
    scale = 1.0;

    uint32_t maxDim = WinOcr::OcrEngine::MaxImageDimension();

    // Upscale small images so text is large enough for OCR to detect.
    // Target: shortest side >= 1500px (ensures ~20px+ character height for most text).
    int minSide = qMin(img.width(), img.height());
    if (minSide < 1500 && minSide > 0) {
        double upFactor = qMin(1500.0 / minSide, static_cast<double>(maxDim) / qMax(img.width(), img.height()));
        if (upFactor > 1.0) {
            img = img.scaled(static_cast<int>(img.width() * upFactor),
                             static_cast<int>(img.height() * upFactor),
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scale = static_cast<double>(img.width()) / static_cast<double>(origWidth);
        }
    }

    // Scale down if exceeding OCR engine's max dimension
    if (static_cast<uint32_t>(img.width()) > maxDim || static_cast<uint32_t>(img.height()) > maxDim) {
        img = img.scaled(static_cast<int>(maxDim), static_cast<int>(maxDim), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scale = static_cast<double>(img.width()) / static_cast<double>(origWidth);
    }

    int w = img.width();
    int h = img.height();

    // QImage Format_ARGB32 stores pixels as BGRA in memory (little-endian x86)
    // which matches BitmapPixelFormat::Bgra8
    SoftwareBitmap bitmap(BitmapPixelFormat::Bgra8, w, h, BitmapAlphaMode::Premultiplied);

    {
        auto buffer = bitmap.LockBuffer(BitmapBufferAccessMode::Write);
        auto ref = buffer.CreateReference();
        auto access = ref.as<IMemoryBufferByteAccess>();

        uint8_t *dstData = nullptr;
        uint32_t dstSize = 0;
        check_hresult(access->GetBuffer(&dstData, &dstSize));

        auto plane = buffer.GetPlaneDescription(0);
        int dstStride = plane.Stride;
        int rowBytes = w * 4;

        for (int y = 0; y < h; y++) {
            memcpy(dstData + y * dstStride, img.constScanLine(y), rowBytes);
        }
    }

    return bitmap;
}

} // anonymous namespace

OcrResult WinOcrEngine::recognize(const QImage &image)
{
    OcrResult result;

    if (image.isNull()) {
        result.errorMessage = "No image";
        return result;
    }

    ComGuard comGuard;

    try {
        auto engine = WinOcr::OcrEngine::TryCreateFromUserProfileLanguages();
        if (!engine) {
            result.errorMessage = "OCR not available. No language packs installed.";
            return result;
        }

        double scale = 1.0;
        auto bitmap = qImageToSoftwareBitmap(image, scale);

        auto ocrResult = engine.RecognizeAsync(bitmap).get();

        for (const auto &line : ocrResult.Lines()) {
            OcrLine ocrLine;
            for (const auto &word : line.Words()) {
                OcrWord ocrWord;
                ocrWord.text = QString::fromWCharArray(word.Text().c_str(), static_cast<int>(word.Text().size()));

                auto rect = word.BoundingRect();
                // Convert back to original image coordinates if image was scaled
                ocrWord.boundingBox = QRectF(rect.X / scale, rect.Y / scale, rect.Width / scale, rect.Height / scale);

                ocrLine.words.append(ocrWord);
            }
            result.lines.append(ocrLine);
        }

        result.success = true;

    } catch (const winrt::hresult_error &e) {
        result.errorMessage = QString::fromWCharArray(e.message().c_str());
    } catch (const std::exception &e) {
        result.errorMessage = QString::fromUtf8(e.what());
    }

    return result;
}

#else

OcrResult WinOcrEngine::recognize(const QImage &image)
{
    Q_UNUSED(image);
    OcrResult result;
    result.errorMessage = "Windows OCR is only available on Windows.";
    return result;
}

#endif
