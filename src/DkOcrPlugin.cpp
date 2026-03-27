#include "DkOcrPlugin.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtConcurrent>

#include "DkBaseViewPort.h"

namespace nmo
{

// ----------------------------------------------------------------
// DkOcrPlugin
// ----------------------------------------------------------------

DkOcrPlugin::DkOcrPlugin()
    : mViewPort(nullptr)
{
}

DkOcrPlugin::~DkOcrPlugin()
{
}

QImage DkOcrPlugin::image() const
{
    return QImage();
}

bool DkOcrPlugin::hideHUD() const
{
    return false;
}

QSharedPointer<nmc::DkImageContainer> DkOcrPlugin::runPlugin(
    const QString & /*runID*/,
    QSharedPointer<nmc::DkImageContainer> imgC) const
{
    return imgC; // viewport plugin — image processing not needed
}

bool DkOcrPlugin::createViewPort(QWidget *parent)
{
    if (!mViewPort) {
        mViewPort = new DkOcrViewPort(parent);
    }
    return true;
}

nmc::DkPluginViewPort *DkOcrPlugin::getViewPort()
{
    return mViewPort;
}

void DkOcrPlugin::setVisible(bool visible)
{
    if (mViewPort) {
        mViewPort->setVisible(visible);
        if (visible) {
            mViewPort->startOcr();
        }
    }
}

// ----------------------------------------------------------------
// DkOcrViewPort
// ----------------------------------------------------------------

DkOcrViewPort::DkOcrViewPort(QWidget *parent)
    : nmc::DkPluginViewPort(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);

    // Repaint overlay when the viewport is zoomed/panned
    auto *vp = dynamic_cast<nmc::DkBaseViewPort *>(parent);
    if (vp) {
        connect(vp, &nmc::DkBaseViewPort::imageUpdated, this, QOverload<>::of(&QWidget::update));
    }
}

DkOcrViewPort::~DkOcrViewPort()
{
}

void DkOcrViewPort::startOcr()
{
    if (mOcrRunning)
        return;

    // Reset previous results
    mOcrResult = OcrResult();
    mWords.clear();
    mSelectedWords.clear();
    mOcrDone = false;
    mDragging = false;

    auto *vp = dynamic_cast<nmc::DkBaseViewPort *>(parentWidget());
    if (!vp)
        return;

    QImage img = vp->getImage();
    if (img.isNull())
        return;

    mOcrRunning = true;
    update();
    setFocus();

    auto *watcher = new QFutureWatcher<OcrResult>(this);
    connect(watcher, &QFutureWatcher<OcrResult>::finished, this, [this, watcher]() {
        mOcrResult = watcher->result();
        mOcrRunning = false;
        mOcrDone = true;

        // Flatten word list for easy indexing
        mWords.clear();
        for (int i = 0; i < mOcrResult.lines.size(); i++) {
            const auto &line = mOcrResult.lines[i];
            for (const auto &word : line.words) {
                WordInfo wi;
                wi.text = word.text;
                wi.imageRect = word.boundingBox;
                wi.lineIndex = i;
                mWords.append(wi);
            }
        }

        update();
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([img]() -> OcrResult {
        return WinOcrEngine::recognize(img);
    }));
}

QTransform DkOcrViewPort::getTransform() const
{
    auto *vp = dynamic_cast<nmc::DkBaseViewPort *>(parentWidget());
    if (!vp)
        return QTransform();
    return vp->getImageMatrix() * vp->getWorldMatrix();
}

// ----------------------------------------------------------------
// Painting
// ----------------------------------------------------------------

void DkOcrViewPort::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // --- Loading indicator ---
    if (mOcrRunning) {
        QString msg = tr("Recognizing text...");
        QFont font("Segoe UI", 14);
        painter.setFont(font);
        QFontMetrics fm(font);
        QRect textBounds = fm.boundingRect(msg);
        textBounds.moveCenter(rect().center());
        textBounds.adjust(-16, -8, 16, 8);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRoundedRect(textBounds, 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, msg);
        return;
    }

    if (!mOcrDone)
        return;

    // --- No text / error ---
    if (mWords.isEmpty()) {
        QString msg = mOcrResult.success ? tr("No text found") : mOcrResult.errorMessage;
        QFont font("Segoe UI", 12);
        painter.setFont(font);
        QFontMetrics fm(font);
        QRect textBounds = fm.boundingRect(msg);
        textBounds.moveCenter(rect().center());
        textBounds.adjust(-16, -8, 16, 8);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRoundedRect(textBounds, 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, msg);
        return;
    }

    // --- Word bounding boxes ---
    QTransform transform = getTransform();

    for (int i = 0; i < mWords.size(); i++) {
        QRectF widgetRect = transform.mapRect(mWords[i].imageRect);
        bool selected = mSelectedWords.contains(i);

        if (selected) {
            painter.setPen(QPen(QColor(0, 120, 215, 150), 1.5));
            painter.setBrush(QColor(0, 120, 215, 100));
        } else {
            painter.setPen(QPen(QColor(0, 120, 215, 80), 1.0));
            painter.setBrush(QColor(0, 120, 215, 40));
        }

        painter.drawRect(widgetRect);
    }

    // --- Drag selection rectangle ---
    if (mDragging) {
        QRectF dragRect = QRectF(mDragStart, mDragCurrent).normalized();
        painter.setPen(QPen(QColor(0, 120, 215, 200), 1.5, Qt::DashLine));
        painter.setBrush(QColor(0, 120, 215, 20));
        painter.drawRect(dragRect);
    }
}

// ----------------------------------------------------------------
// Hit testing
// ----------------------------------------------------------------

int DkOcrViewPort::wordAtPoint(const QPointF &widgetPos) const
{
    QTransform transform = getTransform();
    for (int i = 0; i < mWords.size(); i++) {
        QRectF wr = transform.mapRect(mWords[i].imageRect);
        if (wr.contains(widgetPos))
            return i;
    }
    return -1;
}

QVector<int> DkOcrViewPort::wordsInRect(const QRectF &widgetRect) const
{
    QVector<int> result;
    QTransform transform = getTransform();
    for (int i = 0; i < mWords.size(); i++) {
        QRectF wr = transform.mapRect(mWords[i].imageRect);
        if (widgetRect.intersects(wr))
            result.append(i);
    }
    return result;
}

// ----------------------------------------------------------------
// Mouse events
// ----------------------------------------------------------------

void DkOcrViewPort::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    mDragStart = event->position();
    mDragCurrent = mDragStart;
    mDragging = false;
    event->accept();
}

void DkOcrViewPort::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        event->ignore();
        return;
    }

    mDragCurrent = event->position();

    if (!mDragging && (mDragCurrent - mDragStart).manhattanLength() > 5.0) {
        mDragging = true;
    }

    if (mDragging) {
        QRectF dragRect = QRectF(mDragStart, mDragCurrent).normalized();
        mSelectedWords.clear();
        for (int idx : wordsInRect(dragRect)) {
            mSelectedWords.insert(idx);
        }
        update();
    }

    event->accept();
}

void DkOcrViewPort::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    if (mDragging) {
        mDragging = false;
        if (!mSelectedWords.isEmpty()) {
            copySelectedText();
        }
    } else {
        // Single click
        int idx = wordAtPoint(event->position());
        if (idx >= 0) {
            mSelectedWords.clear();
            mSelectedWords.insert(idx);
            copyText(mWords[idx].text);
        } else {
            mSelectedWords.clear();
        }
    }

    update();
    event->accept();
}

// ----------------------------------------------------------------
// Keyboard events
// ----------------------------------------------------------------

void DkOcrViewPort::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closePlugin();
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        copySelectedText();
        return;
    }

    if (event->matches(QKeySequence::SelectAll)) {
        mSelectedWords.clear();
        for (int i = 0; i < mWords.size(); i++) {
            mSelectedWords.insert(i);
        }
        update();
        return;
    }

    DkPluginViewPort::keyPressEvent(event);
}

void DkOcrViewPort::wheelEvent(QWheelEvent *event)
{
    event->ignore(); // pass through to parent viewport for zoom
}

// ----------------------------------------------------------------
// Text copy
// ----------------------------------------------------------------

QString DkOcrViewPort::buildSelectedText() const
{
    if (mSelectedWords.isEmpty())
        return QString();

    // Group selected words by line, then sort by x position within each line
    QMap<int, QVector<int>> lineMap;
    for (int idx : mSelectedWords) {
        lineMap[mWords[idx].lineIndex].append(idx);
    }

    QStringList lines;
    for (auto it = lineMap.begin(); it != lineMap.end(); ++it) {
        QVector<int> wordIndices = it.value();
        std::sort(wordIndices.begin(), wordIndices.end(), [this](int a, int b) {
            return mWords[a].imageRect.left() < mWords[b].imageRect.left();
        });

        QStringList wordTexts;
        for (int idx : wordIndices) {
            wordTexts.append(mWords[idx].text);
        }
        lines.append(wordTexts.join(' '));
    }

    return lines.join('\n');
}

void DkOcrViewPort::copySelectedText()
{
    QString text = buildSelectedText();
    if (!text.isEmpty()) {
        copyText(text);
    }
}

void DkOcrViewPort::copyText(const QString &text)
{
    QApplication::clipboard()->setText(text);

    // Truncate long text for the info message
    QString preview = text.length() > 50 ? text.left(50) + "..." : text;
    preview.replace('\n', ' ');
    emit showInfo(tr("Copied: %1").arg(preview));
}

} // namespace nmo
