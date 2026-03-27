#pragma once

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>

struct OcrWord {
    QString text;
    QRectF boundingBox; // in original image pixel coordinates
};

struct OcrLine {
    QVector<OcrWord> words;
};

struct OcrResult {
    QVector<OcrLine> lines;
    bool success = false;
    QString errorMessage;
};

class WinOcrEngine
{
public:
    static OcrResult recognize(const QImage &image);
};
