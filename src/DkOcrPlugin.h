#pragma once

#include <QSet>

#include "DkPluginInterface.h"
#include "WinOcrEngine.h"

namespace nmo
{

class DkOcrViewPort;

class DkOcrPlugin : public QObject, nmc::DkViewPortInterface
{
    Q_OBJECT
    Q_INTERFACES(nmc::DkViewPortInterface)
    Q_PLUGIN_METADATA(IID "com.nomacs.ImageLounge.DkOcrPlugin/3.8" FILE "DkOcrPlugin.json")

public:
    DkOcrPlugin();
    ~DkOcrPlugin() override;

    QImage image() const override;
    bool hideHUD() const override;

    QSharedPointer<nmc::DkImageContainer> runPlugin(
        const QString &runID = QString(),
        QSharedPointer<nmc::DkImageContainer> imgC = QSharedPointer<nmc::DkImageContainer>()) const override;

    bool createViewPort(QWidget *parent) override;
    nmc::DkPluginViewPort *getViewPort() override;
    void setVisible(bool visible) override;

private:
    DkOcrViewPort *mViewPort = nullptr;
};

class DkOcrViewPort : public nmc::DkPluginViewPort
{
    Q_OBJECT

public:
    explicit DkOcrViewPort(QWidget *parent);
    ~DkOcrViewPort() override;

    void startOcr();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct WordInfo {
        QString text;
        QRectF imageRect; // bounding box in image pixel coordinates
        int lineIndex;
    };

    QTransform getTransform() const;
    int wordAtPoint(const QPointF &widgetPos) const;
    QVector<int> wordsInRect(const QRectF &widgetRect) const;
    QString buildSelectedText() const;
    void copySelectedText();
    void copyText(const QString &text);

    OcrResult mOcrResult;
    QVector<WordInfo> mWords; // flattened word list

    bool mOcrRunning = false;
    bool mOcrDone = false;

    // selection state
    QSet<int> mSelectedWords;
    bool mDragging = false;
    QPointF mDragStart;
    QPointF mDragCurrent;
};

} // namespace nmo
