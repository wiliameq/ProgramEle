#include "CanvasWidget.h"
#include <unordered_map>
#include "Settings.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QtMath>

// std::max, std::min, std::sqrt
#include <cmath>
#include <algorithm>
#include <array>

#include <QtPdf/QPdfDocument>
#include <QSize>
#include <QImage>
#include <QTextEdit>
#include <QFontMetrics>
#include <QPolygonF>

namespace {
CanvasWidget::ResizeHandle hitResizeHandle(const QRectF &rect, const QPointF &pos, double threshold) {
    struct HandlePoint { CanvasWidget::ResizeHandle handle; QPointF point; };
    const std::array<HandlePoint, 4> points{{
        {CanvasWidget::ResizeHandle::TopLeft, rect.topLeft()},
        {CanvasWidget::ResizeHandle::TopRight, rect.topRight()},
        {CanvasWidget::ResizeHandle::BottomLeft, rect.bottomLeft()},
        {CanvasWidget::ResizeHandle::BottomRight, rect.bottomRight()},
    }};
    for (const auto &hp : points) {
        const double dx = pos.x() - hp.point.x();
        const double dy = pos.y() - hp.point.y();
        if (std::hypot(dx, dy) <= threshold) {
            return hp.handle;
        }
    }
    return CanvasWidget::ResizeHandle::None;
}

double safePixelsPerMeter(double pixelsPerMeter, double zoom) {
    double value = pixelsPerMeter * zoom;
    if (value <= 0.0) {
        return 1.0;
    }
    return value;
}

CalloutAnchor anchorFromPosition(const QRectF &bubbleRect, const QPointF &anchorPos) {
    QPointF center = bubbleRect.center();
    QPointF delta = anchorPos - center;
    if (std::abs(delta.x()) >= std::abs(delta.y())) {
        return (delta.x() < 0.0) ? CalloutAnchor::Left : CalloutAnchor::Right;
    }
    return (delta.y() < 0.0) ? CalloutAnchor::Top : CalloutAnchor::Bottom;
}
} // namespace

namespace {
QPointF clampAnchorOutsideBubble(const QRectF &bubbleRect, const QPointF &anchorPos,
                                 CalloutAnchor anchor, double gapWorld) {
    QPointF clamped = anchorPos;
    switch (anchor) {
    case CalloutAnchor::Bottom: {
        double minY = bubbleRect.bottom() + gapWorld;
        if (clamped.y() < minY) clamped.setY(minY);
        break;
    }
    case CalloutAnchor::Top: {
        double maxY = bubbleRect.top() - gapWorld;
        if (clamped.y() > maxY) clamped.setY(maxY);
        break;
    }
    case CalloutAnchor::Left: {
        double maxX = bubbleRect.left() - gapWorld;
        if (clamped.x() > maxX) clamped.setX(maxX);
        break;
    }
    case CalloutAnchor::Right: {
        double minX = bubbleRect.right() + gapWorld;
        if (clamped.x() < minX) clamped.setX(minX);
        break;
    }
    }
    return clamped;
}

QRectF bubbleRectForAnchor(const QPointF &anchorPos, const QSizeF &sizeWorld,
                           CalloutAnchor anchor, double gapWorld) {
    double w = sizeWorld.width();
    double h = sizeWorld.height();
    double x_m = 0.0;
    double y_m = 0.0;
    switch (anchor) {
    case CalloutAnchor::Bottom:
        x_m = anchorPos.x() - w / 2.0;
        y_m = anchorPos.y() - gapWorld - h;
        break;
    case CalloutAnchor::Top:
        x_m = anchorPos.x() - w / 2.0;
        y_m = anchorPos.y() + gapWorld;
        break;
    case CalloutAnchor::Left:
        x_m = anchorPos.x() + gapWorld;
        y_m = anchorPos.y() - h / 2.0;
        break;
    case CalloutAnchor::Right:
        x_m = anchorPos.x() - gapWorld - w;
        y_m = anchorPos.y() - h / 2.0;
        break;
    }
    return QRectF(x_m, y_m, w, h);
}
} // namespace

CanvasWidget::CanvasWidget(QWidget* parent, ProjectSettings* settings)
    : QWidget(parent)
    , m_settings(settings)
    , m_measurementsTool(this, [this]() { emit measurementFinished(); }) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Inicjuj domyślną widoczność warstw.  Wszystkie znane warstwy są
    // domyślnie ustawiane jako widoczne.  Niektóre warstwy (np. gniazda
    // teletechniczne) są inicjalizowane tutaj; nowe warstwy będą
    // dodawane dynamicznie w momencie tworzenia obiektów.
    m_layerVisibility[QStringLiteral("Ściany")]      = true;
    m_layerVisibility[QStringLiteral("Drzwi")]       = true;
    m_layerVisibility[QStringLiteral("Okna")]        = true;
    m_layerVisibility[QStringLiteral("Gniazda")]     = true;
    m_layerVisibility[QStringLiteral("Oświetlenie")] = true;
    m_layerVisibility[QStringLiteral("Gniazda RJ45")] = true;
    m_layerVisibility[QStringLiteral("Pomiary")]     = true;
    m_layerVisibility[QStringLiteral("Komentarze")]  = true;
    m_measurementsTool.setVisible(m_showMeasures);

    // Ustaw domyślne kolory dla nowo wstawianych dymków
    m_insertBubbleFillColor = QColor(255, 255, 255, 200);
    m_insertBubbleBorderColor = Qt::black;

    // Zainicjuj pola tymczasowego dymka
    m_hasTempTextItem = false;
    m_isDraggingTempBubble = false;
    m_isDraggingTempAnchor = false;
    m_isTempBubblePinned = false;
    m_isDraggingSelectedAnchor = false;
}

// --- modyfikacje bieżącego pomiaru ---
void CanvasWidget::cancelCurrentMeasure() {
    m_measurementsTool.cancelCurrentMeasure();
    m_activeTool = nullptr;
    m_mode = ToolMode::None;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    unsetCursor();
}

void CanvasWidget::undoCurrentMeasure() {
    m_measurementsTool.undoCurrentMeasure();
}

void CanvasWidget::redoCurrentMeasure() {
    m_measurementsTool.redoCurrentMeasure();
}

void CanvasWidget::confirmCurrentMeasure(QWidget* parentForAdvanced) {
    m_measurementsTool.confirmCurrentMeasure(parentForAdvanced);
    m_activeTool = nullptr;
    unsetCursor();
}

void CanvasWidget::insertPendingText(const QString& text) {
    // Wstaw tekst tylko, jeśli ustalono pozycję
    if (!m_hasTextInsertPos) return;
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return;
    // Wyznacz wymiary ramki dymka z marginesami i odstępem na grot.
    QFontMetricsF metrics(m_insertTextFont);
    qreal textW = metrics.horizontalAdvance(trimmed);
    qreal textH = metrics.height();
    // marginesy wokół tekstu (piksele)
    constexpr qreal marginX = 8.0;
    constexpr qreal marginY = 6.0;
    // odstęp na grot – minimalna odległość między ramką a punktem kotwiczenia (piksele)
    constexpr qreal tailGap = 12.0;
    // oblicz szerokość i wysokość dymka w pikselach
    qreal bubbleW = textW + 2 * marginX;
    qreal bubbleH = textH + 2 * marginY;
    // Oblicz przelicznik pikseli na metry.  Jeśli skala nie została
    // jeszcze zdefiniowana (pixelsPerMeter == 0), przyjmij tymczasowo 1, aby uniknąć dzielenia przez zero.
    qreal pixelsPerMeter = m_pixelsPerMeter * m_zoom;
    if (pixelsPerMeter <= 0.0) pixelsPerMeter = 1.0;
    qreal bubbleWWorld = bubbleW / pixelsPerMeter;
    qreal bubbleHWorld = bubbleH / pixelsPerMeter;
    qreal gapWorld = tailGap / pixelsPerMeter;
    // Zawsze zaczynaj z grotem skierowanym w dół
    CalloutAnchor anchor = CalloutAnchor::Bottom;
    QRectF worldRect = bubbleRectForAnchor(m_textInsertPos, QSizeF(bubbleWWorld, bubbleHWorld),
                                           anchor, gapWorld);
    // Ustaw kotwicę tak, aby nie znajdowała się wewnątrz dymka
    QPointF anchorPos = clampAnchorOutsideBubble(worldRect, m_textInsertPos, anchor, gapWorld);
    TextItem item;
    item.pos = anchorPos;
    item.text = trimmed;
    item.color = m_insertTextColor;
    item.font = m_insertTextFont;
    item.anchor = anchor;
    item.boundingRect = worldRect;
    // Ustaw kolory wypełnienia i obramowania z bieżących domyślnych
    item.bgColor = m_insertBubbleFillColor;
    item.borderColor = m_insertBubbleBorderColor;
    // Warstwa dla komentarzy
    item.layer = QStringLiteral("Komentarze");
    m_textItems.push_back(item);
    // Wyczyść stan
    m_hasTextInsertPos = false;
    m_pendingText.clear();
    // Zakończ tryb
    m_mode = ToolMode::None;
    update();
    emit measurementFinished();
}

void CanvasWidget::updateAllMeasureColors() {
    if (!m_settings) return;
    m_measurementsTool.updateAllMeasureColors(m_settings->defaultMeasureColor);
}

// Aktualizuje grubość linii wszystkich istniejących pomiarów na wartość
// zdefiniowaną w ustawieniach projektu.  Nie dotyka linii aktualnie
// rysowanego pomiaru ani wartości m_currentLineWidth.
void CanvasWidget::updateAllMeasureLineWidths() {
    if (!m_settings) return;
    m_measurementsTool.updateAllMeasureLineWidths(m_settings->lineWidthPx);
}

// Rozpoczyna tryb zaznaczania istniejących pomiarów.  Czyści bieżące
// punkty i stos cofnięć, ustawia m_mode i resetuje zaznaczenie.
void CanvasWidget::startSelect() {
    m_mode = ToolMode::Select;
    m_pendingText.clear();
    m_measurementsTool.deactivate();
    m_activeTool = nullptr;
    // Clear text selection and dragging state when entering selection mode
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    // Use arrow cursor for selection
    setCursor(Qt::ArrowCursor);
    update();
}

// Rozpoczyna tryb wstawiania tekstu.  Jeśli parent jest nullptr,
// przyjmujemy, że tekst został ustawiony wcześniej w m_pendingText.
void CanvasWidget::startInsertText(QWidget* parent) {
    Q_UNUSED(parent);
    // Anuluj dotychczasową edycję tekstu (jeśli istnieje) i tymczasowy dymek
    if (m_textEdit) {
        cancelTextEdit();
    }
    if (m_hasTempTextItem) {
        cancelTempTextItem();
    }
    // W trybie InsertText użytkownik najpierw klika w płótno, aby
    // ustawić pozycję kotwicy dymka (koniec strzałki).  Dymek oraz
    // pole edycyjne zostaną utworzone po tym kliknięciu.  Czyścimy
    // punkty pomiaru i zaznaczenie pomiarów/tekstów.
    m_measurementsTool.deactivate();
    m_activeTool = nullptr;
    m_selectedTextIndex = -1;
    // Resetuj flagi wstawiania tekstu
    m_hasTextInsertPos = false;
    m_pendingText.clear();
    m_hasTempTextItem = false;
    m_isDraggingTempBubble = false;
    m_isDraggingTempAnchor = false;
    m_isTempBubblePinned = false;
    m_editingTextIndex = -1;
    m_mode = ToolMode::InsertText;
    // Ustaw kursor krzyża
    setCursor(Qt::CrossCursor);
    update();
}

// Rozpoczyna tryb usuwania pomiarów.  Czyści bieżące punkty i stos cofnięć
// oraz zaznaczenie.
void CanvasWidget::startDelete() {
    m_mode = ToolMode::Delete;
    m_pendingText.clear();
    m_measurementsTool.deactivate();
    m_activeTool = nullptr;
    // Clear text selection and stop any dragging
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    // Use cross cursor to indicate deletion (eraser-like)
    setCursor(Qt::CrossCursor);
    update();
}

// Ustawia kolor zaznaczonego pomiaru
void CanvasWidget::setSelectedMeasureColor(const QColor &c) {
    m_measurementsTool.setSelectedMeasureColor(c);
}

// Ustawia grubość linii zaznaczonego pomiaru
void CanvasWidget::setSelectedMeasureLineWidth(int w) {
    m_measurementsTool.setSelectedMeasureLineWidth(w);
}

// --------- Operacje na tekście ---------

void CanvasWidget::setSelectedTextColor(const QColor &c) {
    if (!hasSelectedText()) return;
    m_textItems[m_selectedTextIndex].color = c;
    update();
}

// Ustawia kolor wypełnienia dymka zaznaczonego tekstu
void CanvasWidget::setSelectedTextBgColor(const QColor &c) {
    if (!hasSelectedText()) return;
    m_textItems[m_selectedTextIndex].bgColor = c;
    update();
}

// Ustawia kolor obramowania dymka zaznaczonego tekstu
void CanvasWidget::setSelectedTextBorderColor(const QColor &c) {
    if (!hasSelectedText()) return;
    m_textItems[m_selectedTextIndex].borderColor = c;
    update();
}

void CanvasWidget::setSelectedTextFont(const QFont &f) {
    if (!hasSelectedText()) return;
    m_textItems[m_selectedTextIndex].font = f;
    // Zaktualizuj boundingRect w oparciu o nową czcionkę i istniejący tekst
    const QString &text = m_textItems[m_selectedTextIndex].text;
    QFontMetrics fm(f);
    int textW = fm.horizontalAdvance(text);
    int textH = fm.height();
    double pixPerM = m_pixelsPerMeter * m_zoom;
    if (pixPerM <= 0.0) pixPerM = 1.0;
    double w_m = textW / pixPerM;
    double h_m = textH / pixPerM;
    TextItem &ti = m_textItems[m_selectedTextIndex];
    if (!ti.boundingRect.isNull()) {
        ti.boundingRect.setSize(QSizeF(w_m, h_m));
    } else {
        const double gapWorld = 12.0 / pixPerM;
        ti.boundingRect = bubbleRectForAnchor(ti.pos, QSizeF(w_m, h_m), ti.anchor, gapWorld);
    }
    const double gapWorld = 12.0 / pixPerM;
    ti.pos = clampAnchorOutsideBubble(ti.boundingRect, ti.pos, ti.anchor, gapWorld);
    update();
}

void CanvasWidget::updateSelectedText(const QString &text, const QColor &color, const QFont &font) {
    if (!hasSelectedText()) return;
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        // Jeśli usunięto treść, usuń element tekstowy
        deleteSelectedText();
        return;
    }
    int idx = m_selectedTextIndex;
    TextItem &ti = m_textItems[idx];
    ti.text = trimmed;
    ti.color = color;
    ti.font = font;
    // Aktualizuj boundingRect na podstawie nowego tekstu i czcionki
    const double marginX = 8.0;
    const double marginY = 6.0;
    QFontMetrics fm(font);
    double widthPx = ti.boundingRect.width() * m_pixelsPerMeter * m_zoom - marginX * 2;
    if (widthPx <= 0.0) {
        widthPx = fm.horizontalAdvance(trimmed);
    }
    QRect textBounds = fm.boundingRect(0, 0, (int)std::ceil(widthPx), 10000,
                                       Qt::TextWordWrap, trimmed);
    double pixPerM = m_pixelsPerMeter * m_zoom;
    if (pixPerM <= 0.0) pixPerM = 1.0;
    double w_m = (textBounds.width() + marginX * 2) / pixPerM;
    double h_m = (textBounds.height() + marginY * 2) / pixPerM;
    double x_m = ti.boundingRect.left();
    double y_m = ti.boundingRect.top();
    if (ti.boundingRect.isNull()) {
        const double gapWorld = 12.0 / pixPerM;
        QRectF rect = bubbleRectForAnchor(ti.pos, QSizeF(w_m, h_m), ti.anchor, gapWorld);
        x_m = rect.left();
        y_m = rect.top();
    }
    ti.boundingRect = QRectF(x_m, y_m, w_m, h_m);
    const double gapWorld = 12.0 / pixPerM;
    ti.pos = clampAnchorOutsideBubble(ti.boundingRect, ti.pos, ti.anchor, gapWorld);
    update();
}

void CanvasWidget::deleteSelectedText() {
    if (!hasSelectedText()) return;
    m_textItems.erase(m_textItems.begin() + m_selectedTextIndex);
    m_selectedTextIndex = -1;
    update();
}

/**
 * Ustawia kierunek strzałki dla zaznaczonego elementu tekstowego i
 * przelicza jego boundingRect, aby pasował do nowej orientacji.
 */
void CanvasWidget::setSelectedTextAnchor(CalloutAnchor a) {
    if (!hasSelectedText()) return;
    int idx = m_selectedTextIndex;
    if (idx < 0 || idx >= (int)m_textItems.size()) return;
    TextItem &ti = m_textItems[idx];
    if (ti.anchor == a) {
        // Nic nie zmienia się, ale mimo to odśwież widok
        update();
        return;
    }
    ti.anchor = a;
    if (ti.boundingRect.isNull()) {
        // Oblicz wymiary tekstu w pikselach na podstawie bieżącej czcionki
        QFontMetrics fm(ti.font);
        int textW = fm.horizontalAdvance(ti.text);
        int textH = fm.height();
        double pixPerM = m_pixelsPerMeter * m_zoom;
        if (pixPerM <= 0.0) pixPerM = 1.0;
        double w_m = textW / pixPerM;
        double h_m = textH / pixPerM;
        const double gapWorld = 12.0 / pixPerM;
        ti.boundingRect = bubbleRectForAnchor(ti.pos, QSizeF(w_m, h_m), ti.anchor, gapWorld);
    }
    double pixPerM = m_pixelsPerMeter * m_zoom;
    if (pixPerM <= 0.0) pixPerM = 1.0;
    const double gapWorld = 12.0 / pixPerM;
    ti.pos = clampAnchorOutsideBubble(ti.boundingRect, ti.pos, ti.anchor, gapWorld);
    update();
}

void CanvasWidget::startEditExistingText(int index) {
    if (index < 0 || index >= (int)m_textItems.size()) return;
    // Anuluj bieżącą edycję lub tymczasowy dymek
    if (m_textEdit) {
        cancelTextEdit();
    }
    if (m_hasTempTextItem) {
        cancelTempTextItem();
    }
    // Ustaw indeks edytowanego elementu
    m_editingTextIndex = index;
    // Wyciągnij istniejący element
    TextItem &ti = m_textItems[index];
    // Utwórz pole edycyjne nad tekstem
    m_textEdit = new QTextEdit(this);
    m_textEdit->setFrameStyle(QFrame::NoFrame);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setAutoFillBackground(false);
    m_textEdit->setAttribute(Qt::WA_TranslucentBackground);
    m_textEdit->document()->setDocumentMargin(0.0);
    m_textEdit->setContentsMargins(0, 0, 0, 0);
    m_textEdit->installEventFilter(this);
    // Ustaw kolor tekstu i czcionkę zgodnie z istniejącym elementem
    QPalette pal = m_textEdit->palette();
    pal.setColor(QPalette::Text, ti.color);
    pal.setColor(QPalette::Base, Qt::transparent);
    m_textEdit->setPalette(pal);
    m_textEdit->setStyleSheet(QString("color: %1; background: transparent;").arg(ti.color.name()));
    m_textEdit->setFont(ti.font);
    // Ustaw istniejący tekst
    m_textEdit->setPlainText(ti.text);
    // Oblicz pozycję na ekranie na podstawie boundingRect górnego lewego rogu
    QPointF topLeftScreen = toScreen(ti.boundingRect.topLeft());
    QSizeF sizePx(ti.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                  ti.boundingRect.height() * m_pixelsPerMeter * m_zoom);
    int width = std::max(40, (int)std::round(sizePx.width()));
    int height = std::max(20, (int)std::round(sizePx.height()));
    m_textEdit->move(topLeftScreen.toPoint());
    m_textEdit->resize(width, height);
    m_textEdit->show();
    m_textEdit->setFocus();
    // Podczas edycji aktualizuj boundingRect w czasie rzeczywistym
    connect(m_textEdit, &QTextEdit::textChanged, this, [this, index](){
        if (index < 0 || index >= (int)m_textItems.size()) return;
        TextItem &t = m_textItems[index];
        t.text = m_textEdit ? m_textEdit->toPlainText() : t.text;
        const double marginX = 8.0;
        const double marginY = 6.0;
        // Determine document size and convert to world units including margins.
        QSizeF docSize = m_textEdit ? m_textEdit->document()->size() : QSizeF();
        double pixPerM = m_pixelsPerMeter * m_zoom;
        if (pixPerM <= 0.0) pixPerM = 1.0;
        double w_m = (docSize.width() + marginX * 2) / pixPerM;
        double h_m = (docSize.height() + marginY * 2) / pixPerM;
        // Use a fixed pixel gap for the arrow tail and convert it to world units.
        const double tailGapPx = 12.0;
        double gapWorld = tailGapPx / pixPerM;
        // Compute new bubble position relative to the anchor.
        double x_m = 0.0;
        double y_m = 0.0;
        switch (t.anchor) {
        case CalloutAnchor::Bottom:
            // Bubble above anchor
            x_m = t.pos.x() - w_m / 2.0;
            y_m = t.pos.y() - gapWorld - h_m;
            break;
        case CalloutAnchor::Top:
            // Bubble below anchor
            x_m = t.pos.x() - w_m / 2.0;
            y_m = t.pos.y() + gapWorld;
            break;
        case CalloutAnchor::Left:
            // Bubble to the right
            x_m = t.pos.x() + gapWorld;
            y_m = t.pos.y() - h_m / 2.0;
            break;
        case CalloutAnchor::Right:
            // Bubble to the left
            x_m = t.pos.x() - gapWorld - w_m;
            y_m = t.pos.y() - h_m / 2.0;
            break;
        }
        t.boundingRect = QRectF(x_m, y_m, w_m, h_m);
        // Move the editor widget on screen to follow the bubble's top-left.
        QPointF tl = toScreen(t.boundingRect.topLeft());
        int width2 = std::max(40, (int)std::round(docSize.width() + marginX * 2));
        int height2 = std::max(20, (int)std::round(docSize.height() + marginY * 2));
        if (m_textEdit) {
            m_textEdit->move(tl.toPoint());
            m_textEdit->resize(width2, height2);
        }
        update();
    });
    // Przejdź do trybu InsertText, aby obsłużyć zatwierdzenie/Anuluj
    m_mode = ToolMode::InsertText;
    // Zaktualizuj widok
    update();
}

// Usuwa zaznaczony pomiar
void CanvasWidget::deleteSelectedMeasure() {
    m_measurementsTool.deleteSelectedMeasure();
}

/**
 * Ustawia kolor bieżącego pomiaru.  Nie modyfikuje domyślnego koloru w
 * m_settings. Zmiana jest przekazywana do aktywnego modułu pomiarów.
 */
void CanvasWidget::setCurrentColor(const QColor &c) {
    m_measurementsTool.setCurrentColor(c);
}

/**
 * Ustawia grubość linii bieżącego pomiaru. Nie modyfikuje ustawień globalnych.
 */
void CanvasWidget::setCurrentLineWidth(int w) {
    m_measurementsTool.setCurrentLineWidth(w);
}

bool CanvasWidget::loadBackgroundFile(const QString& file) {
    QImage img;
    if (!loadBackgroundImage(file, img)) {
        return false;
    }
    m_bgImage = img;
    m_showBackground = true;
    update();
    return true;
}

bool CanvasWidget::loadBackgroundImage(const QString& file, QImage& image) const {
    QFileInfo fi(file);
    const QString ext = fi.suffix().toLower();
    if (ext == "pdf") {
        QPdfDocument pdf;
        auto err = pdf.load(file);
        if (err != QPdfDocument::Error::None) {
            return false;
        }
        if (pdf.pageCount() < 1) {
            return false;
        }

        const int pageIndex = 0;
        const QSizeF pt = pdf.pagePointSize(pageIndex);
        if (pt.isEmpty()) {
            return false;
        }

        const double targetWidth = 2000.0;
        const double scale = targetWidth / pt.width();
        const QSize imgSize(qMax(1, int(pt.width() * scale)),
                            qMax(1, int(pt.height() * scale)));

        QImage rendered = pdf.render(pageIndex, imgSize);
        if (rendered.isNull()) {
            return false;
        }

        image = rendered.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        return true;
    }
    QImageReader reader(file);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        return false;
    }
    image = img;
    return true;
}

// --- Zarządzanie widocznością warstw ---

void CanvasWidget::toggleLayerVisibility(const QString& layer) {
    // Jeśli warstwa nie istnieje w mapie, dodaj ją i ustaw na widoczną
    auto it = m_layerVisibility.find(layer);
    if (it == m_layerVisibility.end()) {
        m_layerVisibility[layer] = true;
        it = m_layerVisibility.find(layer);
    }
    // Przełącz wartość logiczną
    it->second = !it->second;
    update();
}

bool CanvasWidget::isLayerVisible(const QString& layer) const {
    auto it = m_layerVisibility.find(layer);
    if (it == m_layerVisibility.end()) {
        return true;
    }
    return it->second;
}

void CanvasWidget::toggleBackgroundVisibility() { m_showBackground = !m_showBackground; update(); }
void CanvasWidget::setBackgroundVisible(bool visible) {
    m_showBackground = visible;
    update();
}

bool CanvasWidget::hasBackground() const { return !m_bgImage.isNull(); }

bool CanvasWidget::isBackgroundVisible() const { return m_showBackground; }

void CanvasWidget::clearBackground() {
    m_bgImage = QImage();
    update();
}

void CanvasWidget::setBackgroundImage(const QImage& image) {
    m_bgImage = image;
    update();
}

const QImage& CanvasWidget::backgroundImage() const { return m_bgImage; }
void CanvasWidget::toggleMeasuresVisibility() {
    m_showMeasures = !m_showMeasures;
    m_measurementsTool.setVisible(m_showMeasures);
    update();
}
void CanvasWidget::startScaleDefinition(double) {
    m_scaleStep = ScaleStep::FirstPending;
    m_scaleHasFirst = false;
    m_scaleHasSecond = false;
    m_scaleDragPoint = 0;
    m_mode = ToolMode::DefineScale;
    m_measurementsTool.deactivate();
    m_activeTool = nullptr;
    emitScaleStateChanged();
}

void CanvasWidget::confirmScaleStep(QWidget* parent) {
    if (m_mode != ToolMode::DefineScale) {
        return;
    }
    if (m_scaleStep == ScaleStep::FirstPending && m_scaleHasFirst) {
        m_scaleStep = ScaleStep::SecondPending;
        emitScaleStateChanged();
        update();
        return;
    }
    if (m_scaleStep == ScaleStep::SecondPending && m_scaleHasSecond) {
        m_scaleStep = ScaleStep::Adjusting;
        emitScaleStateChanged();
        update();
        return;
    }
    if (m_scaleStep == ScaleStep::Adjusting) {
        applyScaleFromPoints(parent);
        m_mode = ToolMode::None;
        m_scaleStep = ScaleStep::None;
        m_scaleHasFirst = false;
        m_scaleHasSecond = false;
        m_scaleDragPoint = 0;
        emitScaleStateChanged();
        emit scaleFinished();
        update();
    }
}

void CanvasWidget::removeScalePoint() {
    if (m_mode != ToolMode::DefineScale) {
        return;
    }
    if (m_scaleHasSecond) {
        m_scaleHasSecond = false;
        m_scaleStep = ScaleStep::SecondPending;
    } else if (m_scaleHasFirst) {
        m_scaleHasFirst = false;
        m_scaleStep = ScaleStep::FirstPending;
    }
    m_scaleDragPoint = 0;
    emitScaleStateChanged();
    update();
}

bool CanvasWidget::scaleHasFirstPoint() const { return m_scaleHasFirst; }

bool CanvasWidget::scaleHasSecondPoint() const { return m_scaleHasSecond; }

int CanvasWidget::scaleStep() const { return static_cast<int>(m_scaleStep); }

void CanvasWidget::emitScaleStateChanged() {
    emit scaleStateChanged(static_cast<int>(m_scaleStep), m_scaleHasFirst, m_scaleHasSecond);
}

void CanvasWidget::startMeasureLinear() {
    m_mode = ToolMode::None;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    m_measurementsTool.startLinear();
    m_activeTool = &m_measurementsTool;
    setCursor(Qt::CrossCursor);
}
void CanvasWidget::startMeasurePolyline() {
    m_mode = ToolMode::None;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    m_measurementsTool.startPolyline();
    m_activeTool = &m_measurementsTool;
    setCursor(Qt::CrossCursor);
}
void CanvasWidget::startMeasureAdvanced(QWidget* parent) {
    m_measurementsTool.startAdvanced(parent);
    if (!m_measurementsTool.isActive()) {
        return;
    }
    m_mode = ToolMode::None;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    m_activeTool = &m_measurementsTool;
    setCursor(Qt::CrossCursor);
}

QPointF CanvasWidget::toWorld(const QPointF& screen) const { return (screen - m_viewOffset) / m_zoom; }
QPointF CanvasWidget::toScreen(const QPointF& world) const { return world * m_zoom + m_viewOffset; }

void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    p.translate(m_viewOffset);
    p.scale(m_zoom, m_zoom);

    if (m_showBackground && !m_bgImage.isNull()) {
        p.drawImage(QPointF(0,0), m_bgImage);
    }

    if (m_showMeasures) {
        m_measurementsTool.draw(p);
    }
    drawTextItems(p);
    drawOverlay(p); // overlay is also in world coords

    p.resetTransform();
    p.setPen(Qt::gray);
    p.drawText(10, height()-10, "PPM: pan, kółko/+/-: zoom; Pomiary: menu; Enter kończy; Ctrl+Enter zatwierdza komentarz; Backspace cofa; Esc anuluje");
}

void CanvasWidget::drawTextItems(QPainter& p) {
    p.setRenderHint(QPainter::Antialiasing, true);
    for (size_t ti = 0; ti < m_textItems.size(); ++ti) {
        const auto &txt = m_textItems[ti];
        if (txt.text.isEmpty()) continue;
        // Jeżeli warstwa tekstu jest wyłączona, pomiń rysowanie
        if (!isLayerVisible(txt.layer)) {
            continue;
        }
        const double marginX = 8.0;
        const double marginY = 6.0;
        // Przelicz górny lewy narożnik oraz rozmiar dymka w pikselach
        QPointF topLeftScreen = toScreen(txt.boundingRect.topLeft());
        QSizeF sizePx(txt.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                      txt.boundingRect.height() * m_pixelsPerMeter * m_zoom);
        QRectF bubbleRect(topLeftScreen, sizePx);
        // Wyznacz anchor strzałki w pikselach
        QPointF anchorScreen = toScreen(txt.pos);
        // Zapamiętaj aktualne ustawienia pędzla, czcionki i koloru
        QFont oldFont = p.font();
        QPen oldPen = p.pen();
        // Ustaw czcionkę tekstu
        if (txt.font != QFont()) {
            p.setFont(txt.font);
        }
        // Stwórz ścieżkę dymka z zaokrąglonymi rogami i strzałką
        QPainterPath calloutPath;
        const double radius = 8.0;
        calloutPath.addRoundedRect(bubbleRect, radius, radius);
        // Dodaj strzałkę w zależności od ustawionej kotwicy
        const double halfBase = 9.0;
        if (txt.anchor == CalloutAnchor::Bottom) {
            double baseX = std::clamp(anchorScreen.x(), bubbleRect.left() + radius, bubbleRect.right() - radius);
            QPointF baseLeft(baseX - halfBase, bubbleRect.bottom());
            QPointF baseRight(baseX + halfBase, bubbleRect.bottom());
            QPolygonF tail;
            tail << baseLeft << anchorScreen << baseRight;
            calloutPath.addPolygon(tail);
        } else if (txt.anchor == CalloutAnchor::Top) {
            double baseX = std::clamp(anchorScreen.x(), bubbleRect.left() + radius, bubbleRect.right() - radius);
            QPointF baseLeft(baseX - halfBase, bubbleRect.top());
            QPointF baseRight(baseX + halfBase, bubbleRect.top());
            QPolygonF tail;
            tail << baseLeft << anchorScreen << baseRight;
            calloutPath.addPolygon(tail);
        } else if (txt.anchor == CalloutAnchor::Left) {
            double baseY = std::clamp(anchorScreen.y(), bubbleRect.top() + radius, bubbleRect.bottom() - radius);
            QPointF baseTop(bubbleRect.left(), baseY - halfBase);
            QPointF baseBottom(bubbleRect.left(), baseY + halfBase);
            QPolygonF tail;
            tail << baseTop << anchorScreen << baseBottom;
            calloutPath.addPolygon(tail);
        } else if (txt.anchor == CalloutAnchor::Right) {
            double baseY = std::clamp(anchorScreen.y(), bubbleRect.top() + radius, bubbleRect.bottom() - radius);
            QPointF baseTop(bubbleRect.right(), baseY - halfBase);
            QPointF baseBottom(bubbleRect.right(), baseY + halfBase);
            QPolygonF tail;
            tail << baseTop << anchorScreen << baseBottom;
            calloutPath.addPolygon(tail);
        }
        // Wypełnij tło dymka określonym kolorem tła z kanałem alfa
        p.setPen(Qt::NoPen);
        p.fillPath(calloutPath, txt.bgColor);
        // Narysuj obramowanie dymka w kolorze borderColor
        QPen bubblePen(txt.borderColor);
        bubblePen.setWidthF(1.2);
        bubblePen.setCosmetic(true);
        p.setPen(bubblePen);
        p.drawPath(calloutPath);
        // Wypisz tekst wewnątrz dymka z odpowiednim marginesem w kolorze tekstu
        QRectF textRect = bubbleRect.adjusted(marginX, marginY, -marginX, -marginY);
        p.setPen(txt.color);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, txt.text);
        // Jeśli element jest zaznaczony, narysuj czerwone przerywane obramowanie wokół dymka
        if ((int)ti == m_selectedTextIndex && m_debugDrawTextHandles) {
            QPen selPen(QColor(255,0,0));
            selPen.setStyle(Qt::DashLine);
            selPen.setWidth(1);
            selPen.setCosmetic(true);
            p.setPen(selPen);
            p.drawPath(calloutPath);
            // Uchwytowe kropki na rogach i kotwicy
            QPen handlePen(Qt::black);
            handlePen.setCosmetic(true);
            p.setPen(handlePen);
            p.setBrush(Qt::white);
            const double handleRadius = 4.0;
            p.drawEllipse(bubbleRect.topLeft(), handleRadius, handleRadius);
            p.drawEllipse(bubbleRect.topRight(), handleRadius, handleRadius);
            p.drawEllipse(bubbleRect.bottomLeft(), handleRadius, handleRadius);
            p.drawEllipse(bubbleRect.bottomRight(), handleRadius, handleRadius);
            p.drawEllipse(anchorScreen, handleRadius, handleRadius);
        }
        // Przywróć stan
        p.setFont(oldFont);
        p.setPen(oldPen);
    }
    // Narysuj tymczasowy dymek podczas wstawiania
    if (m_mode == ToolMode::InsertText && m_hasTempTextItem) {
        const auto &txt = m_tempTextItem;
        const double marginX = 8.0;
        const double marginY = 6.0;
        // Przelicz górny lewy narożnik oraz rozmiar dymka w pikselach
        QPointF topLeftScreen = toScreen(txt.boundingRect.topLeft());
        QSizeF sizePx(txt.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                      txt.boundingRect.height() * m_pixelsPerMeter * m_zoom);
        QRectF bubbleRect(topLeftScreen, sizePx);
        // Wyznacz anchor strzałki w pikselach
        QPointF anchorScreen = toScreen(txt.pos);
        QFont oldFont = p.font();
        QPen oldPen = p.pen();
        if (txt.font != QFont()) {
            p.setFont(txt.font);
        }
        QPainterPath calloutPath;
        const double radius = 8.0;
        calloutPath.addRoundedRect(bubbleRect, radius, radius);
        const double halfBase = 9.0;
        if (txt.anchor == CalloutAnchor::Bottom) {
            double baseX = std::clamp(anchorScreen.x(), bubbleRect.left() + radius, bubbleRect.right() - radius);
            QPointF baseLeft(baseX - halfBase, bubbleRect.bottom());
            QPointF baseRight(baseX + halfBase, bubbleRect.bottom());
            QPolygonF tail;
            tail << baseLeft << anchorScreen << baseRight;
            calloutPath.addPolygon(tail);
        } else if (txt.anchor == CalloutAnchor::Top) {
            double baseX = std::clamp(anchorScreen.x(), bubbleRect.left() + radius, bubbleRect.right() - radius);
            QPointF baseLeft(baseX - halfBase, bubbleRect.top());
            QPointF baseRight(baseX + halfBase, bubbleRect.top());
            QPolygonF tail;
            tail << baseLeft << anchorScreen << baseRight;
            calloutPath.addPolygon(tail);
        } else if (txt.anchor == CalloutAnchor::Left) {
            double baseY = std::clamp(anchorScreen.y(), bubbleRect.top() + radius, bubbleRect.bottom() - radius);
            QPointF baseTop(bubbleRect.left(), baseY - halfBase);
            QPointF baseBottom(bubbleRect.left(), baseY + halfBase);
            QPolygonF tail;
            tail << baseTop << anchorScreen << baseBottom;
            calloutPath.addPolygon(tail);
        } else if (txt.anchor == CalloutAnchor::Right) {
            double baseY = std::clamp(anchorScreen.y(), bubbleRect.top() + radius, bubbleRect.bottom() - radius);
            QPointF baseTop(bubbleRect.right(), baseY - halfBase);
            QPointF baseBottom(bubbleRect.right(), baseY + halfBase);
            QPolygonF tail;
            tail << baseTop << anchorScreen << baseBottom;
            calloutPath.addPolygon(tail);
        }
        p.setPen(Qt::NoPen);
        p.fillPath(calloutPath, txt.bgColor);
        QPen bubblePen(txt.borderColor);
        bubblePen.setWidthF(1.2);
        bubblePen.setCosmetic(true);
        p.setPen(bubblePen);
        p.drawPath(calloutPath);
        QRectF textRect = bubbleRect.adjusted(marginX, marginY, -marginX, -marginY);
        p.setPen(txt.color);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, txt.text);
        if (m_debugDrawTextHandles) {
            // Uchwytowe kropki na rogach i kotwicy
            QPen handlePen(Qt::black);
            handlePen.setCosmetic(true);
            p.setPen(handlePen);
            p.setBrush(Qt::white);
            const double handleRadius = 5.0;
            p.drawEllipse(bubbleRect.topLeft(), handleRadius, handleRadius);
            p.drawEllipse(bubbleRect.topRight(), handleRadius, handleRadius);
            p.drawEllipse(bubbleRect.bottomLeft(), handleRadius, handleRadius);
            p.drawEllipse(bubbleRect.bottomRight(), handleRadius, handleRadius);
            p.drawEllipse(anchorScreen, handleRadius, handleRadius);
        }
        p.setFont(oldFont);
        p.setPen(oldPen);
    }
}

void CanvasWidget::drawOverlay(QPainter& p) {
    m_measurementsTool.drawOverlay(p, m_hasMouseWorld, m_mouseWorld);
    if (m_mode == ToolMode::DefineScale) {
        QPen linePen(QColor(30, 144, 255));
        linePen.setWidthF(2.0);
        linePen.setCosmetic(true);
        QPen pointPen(Qt::black);
        pointPen.setWidthF(1.2);
        pointPen.setCosmetic(true);
        QBrush pointBrush(Qt::white);
        if (m_scaleHasFirst) {
            p.setPen(pointPen);
            p.setBrush(pointBrush);
            p.drawEllipse(m_scaleFirstPoint, 5.0 / m_zoom, 5.0 / m_zoom);
        }
        if (m_scaleHasSecond) {
            p.setPen(pointPen);
            p.setBrush(pointBrush);
            p.drawEllipse(m_scaleSecondPoint, 5.0 / m_zoom, 5.0 / m_zoom);
        }
        if (m_scaleStep == ScaleStep::Adjusting && m_scaleHasFirst && m_scaleHasSecond) {
            p.setPen(linePen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(m_scaleFirstPoint, m_scaleSecondPoint);
        }
    }
}

void CanvasWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton) {
        QPointF pos = toWorld(ev->position());
        if (m_mode == ToolMode::Select) {
            double marginWorldX = 0.0;
            double marginWorldY = 0.0;
            if (m_pixelsPerMeter * m_zoom != 0.0) {
                marginWorldX = 8.0 / (m_pixelsPerMeter * m_zoom);
                marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
            }
            for (int i = 0; i < (int)m_textItems.size(); ++i) {
                const auto &ti = m_textItems[i];
                QRectF hitRect = ti.boundingRect.adjusted(-marginWorldX, -marginWorldY, marginWorldX, marginWorldY);
                if (hitRect.contains(pos)) {
                    startEditExistingText(i);
                    return;
                }
            }
        }
        m_isPanning = true; m_lastMouseScreen = ev->position(); return;
    }
    QPointF pos = toWorld(ev->position());
    if (m_mode == ToolMode::DefineScale) {
        if (ev->button() != Qt::LeftButton) {
            return;
        }
        auto screenPos = ev->position();
        auto hitPoint = [this, screenPos](const QPointF& pt) {
            QPointF screen = toScreen(pt);
            const double dx = screen.x() - screenPos.x();
            const double dy = screen.y() - screenPos.y();
            return std::hypot(dx, dy) <= 8.0;
        };
        if (m_scaleHasFirst && hitPoint(m_scaleFirstPoint)) {
            m_scaleDragPoint = 1;
            return;
        }
        if (m_scaleHasSecond && hitPoint(m_scaleSecondPoint)) {
            m_scaleDragPoint = 2;
            return;
        }
        if (m_scaleStep == ScaleStep::FirstPending) {
            m_scaleFirstPoint = pos;
            m_scaleHasFirst = true;
            emitScaleStateChanged();
        } else if (m_scaleStep == ScaleStep::SecondPending) {
            m_scaleSecondPoint = pos;
            m_scaleHasSecond = true;
            emitScaleStateChanged();
        }
        update();
        return;
    }
    if (m_activeTool && m_activeTool->mousePress(ev)) {
        return;
    }
    // Tryb zaznaczania: wybierz pomiar lub tekst
    if (m_mode == ToolMode::Select) {
        QPointF wpos = pos;
        // W pierwszej kolejności sprawdź, czy kliknięto w kotwicę (strzałkę) któregoś
        // z dymków.  Jeśli tak, ustaw zaznaczenie na ten dymek i rozpocznij
        // przeciąganie kotwicy.
        int anchorIdx = -1;
        double threshold = 8.0 / safePixelsPerMeter(m_pixelsPerMeter, m_zoom);
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            double dx = wpos.x() - ti.pos.x();
            double dy = wpos.y() - ti.pos.y();
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist <= threshold) {
                anchorIdx = i;
                break;
            }
        }
        if (anchorIdx >= 0) {
            m_selectedTextIndex = anchorIdx;
            m_measurementsTool.clearSelection();
            m_isDraggingSelectedAnchor = true;
            m_isDraggingSelectedText = false;
            update();
            return;
        }
        // Sprawdź, czy kliknięto w uchwyt rozmiaru któregoś dymka
        int resizeIdx = -1;
        ResizeHandle handle = ResizeHandle::None;
        double handleThreshold = 10.0;
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            QPointF topLeftScreen = toScreen(ti.boundingRect.topLeft());
            QSizeF sizePx(ti.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                          ti.boundingRect.height() * m_pixelsPerMeter * m_zoom);
            QRectF bubbleRect(topLeftScreen, sizePx);
            handle = hitResizeHandle(bubbleRect, ev->position(), handleThreshold);
            if (handle != ResizeHandle::None) {
                resizeIdx = i;
                break;
            }
        }
        if (resizeIdx >= 0) {
            m_selectedTextIndex = resizeIdx;
            m_measurementsTool.clearSelection();
            m_resizeHandle = handle;
            m_isResizingSelectedBubble = true;
            QPointF topLeftScreen = toScreen(m_textItems[resizeIdx].boundingRect.topLeft());
            QSizeF sizePx(m_textItems[resizeIdx].boundingRect.width() * m_pixelsPerMeter * m_zoom,
                          m_textItems[resizeIdx].boundingRect.height() * m_pixelsPerMeter * m_zoom);
            QRectF bubbleRect(topLeftScreen, sizePx);
            m_resizeStartRect = bubbleRect;
            m_resizeStartPos = ev->position();
            m_isDraggingSelectedText = false;
            m_isDraggingSelectedAnchor = false;
            grabMouse();
            update();
            return;
        }
        // Jeśli kliknięto wewnątrz dymka, rozpocznij przeciąganie całego dymka
        int bubbleIdx = -1;
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            QPointF topLeftScreen = toScreen(ti.boundingRect.topLeft());
            QSizeF sizePx(ti.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                          ti.boundingRect.height() * m_pixelsPerMeter * m_zoom);
            QRectF hitRect(topLeftScreen, sizePx);
            if (hitRect.contains(ev->position())) {
                bubbleIdx = i;
                break;
            }
        }
        if (bubbleIdx >= 0) {
            m_selectedTextIndex = bubbleIdx;
            m_measurementsTool.clearSelection();
            m_isDraggingSelectedText = true;
            m_isDraggingSelectedAnchor = false;
            // Offset między kliknięciem a lewym górnym rogiem dymka
            m_dragStartOffset = wpos - m_textItems[bubbleIdx].boundingRect.topLeft();
            grabMouse();
            update();
            return;
        }
        // W przeciwnym razie szukaj najbliższego pomiaru
        double bestDist = 5.0 / m_zoom; // próg w jednostkach world (przybliżony)
        m_measurementsTool.selectMeasureAt(wpos, bestDist);
        m_selectedTextIndex = -1;
        update();
        return;
    }
    // Tryb wstawiania tekstu: pierwsze kliknięcie ustawia pozycję kotwicy,
    // kolejne kliknięcia pozwalają przeciągać dymek lub końcówkę strzałki.
    if (m_mode == ToolMode::InsertText) {
        // Przelicz pozycję kliknięcia do współrzędnych świata
        QPointF wpos = pos;
        // Jeżeli tymczasowy dymek jest już aktywny
        if (m_hasTempTextItem) {
            QPointF topLeftScreen = toScreen(m_tempTextItem.boundingRect.topLeft());
            QSizeF sizePx(m_tempTextItem.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                          m_tempTextItem.boundingRect.height() * m_pixelsPerMeter * m_zoom);
            QRectF bubbleRect(topLeftScreen, sizePx);
            // Sprawdź, czy kliknięto w uchwyt rozmiaru dymka
            double handleThreshold = 10.0;
            ResizeHandle handle = hitResizeHandle(bubbleRect, ev->position(), handleThreshold);
            if (handle != ResizeHandle::None) {
                m_resizeHandle = handle;
                m_isResizingTempBubble = true;
                m_resizeStartRect = bubbleRect;
                m_resizeStartPos = ev->position();
                m_isDraggingTempBubble = false;
                m_isDraggingTempAnchor = false;
                grabMouse();
                return;
            }
            // Sprawdź, czy kliknięcie znajduje się w pobliżu kotwicy
            double threshold = 8.0 / safePixelsPerMeter(m_pixelsPerMeter, m_zoom);
            double dx = wpos.x() - m_tempTextItem.pos.x();
            double dy = wpos.y() - m_tempTextItem.pos.y();
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist <= threshold) {
                // Rozpocznij przeciąganie kotwicy
                m_isDraggingTempAnchor = true;
                m_isDraggingTempBubble = false;
                m_isResizingTempBubble = false;
                grabMouse();
                return;
            }
            // Sprawdź, czy kliknięto wewnątrz obszaru dymka (boundingRect)
            if (bubbleRect.contains(ev->position())) {
                // Rozpocznij przeciąganie dymka; zapamiętaj offset
                m_isDraggingTempBubble = true;
                m_isDraggingTempAnchor = false;
                m_isResizingTempBubble = false;
                m_tempDragOffset = wpos - m_tempTextItem.boundingRect.topLeft();
                grabMouse();
                return;
            }
            // Kliknięcie poza dymkiem i kotwicą podczas wstawiania: potraktuj
            // to jako kliknięcie poza dymkiem; nie zmieniaj położenia.
            return;
        }
        // Jeżeli aktywne jest pole edycyjne (użytkownik wpisuje tekst)
        // i kliknięcie znajduje się wewnątrz tego pola, pozwól mu
        // obsłużyć zdarzenie bezpośrednio. Kliknięcie poza polem
        // zatwierdza bieżącą edycję (jak w edytorach tekstu).
        if (m_textEdit) {
            QRect geom = m_textEdit->geometry();
            if (geom.contains(ev->position().toPoint())) {
                QWidget::mousePressEvent(ev);
                return;
            }
            commitActiveTextEdit();
        }
        // Jeśli tymczasowy dymek jeszcze nie istnieje, to klik
        // ustawia pozycję kotwicy i rozpoczyna edycję.  Utwórz
        // m_tempTextItem i pole QTextEdit.
        m_hasTempTextItem = true;
        m_tempTextItem.pos = wpos;
        m_tempTextItem.text.clear();
        m_tempTextItem.color = m_insertTextColor;
        m_tempTextItem.font = m_insertTextFont;
        m_tempTextItem.layer = QStringLiteral("Komentarze");
        m_tempTextItem.anchor = m_insertTextAnchor;
        // Ustaw kolory wypełnienia i obramowania
        m_tempTextItem.bgColor = m_insertBubbleFillColor;
        m_tempTextItem.borderColor = m_insertBubbleBorderColor;
        // Oblicz początkowy boundingRect dla pustego tekstu
        m_isTempBubblePinned = false;
        updateTempBoundingRect();
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            const double offsetPxX = 40.0;
            const double offsetPxY = -40.0;
            QPointF offsetWorld(offsetPxX / (m_pixelsPerMeter * m_zoom),
                                offsetPxY / (m_pixelsPerMeter * m_zoom));
            m_tempTextItem.boundingRect.translate(offsetWorld.x(), offsetWorld.y());
            const double marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
            double bubbleBottom = m_tempTextItem.boundingRect.bottom() + marginWorldY;
            if (bubbleBottom >= m_tempTextItem.pos.y() - marginWorldY) {
                double shift = bubbleBottom - (m_tempTextItem.pos.y() - marginWorldY);
                m_tempTextItem.boundingRect.translate(0.0, -shift);
            }
        }
        m_isTempBubblePinned = false;
        // Utwórz pole edycyjne na płótnie
        if (m_textEdit) {
            // Nie powinno mieć miejsca, ale dla pewności usuń stare pole
            cancelTextEdit();
        }
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            double gapWorld = 12.0 / (m_pixelsPerMeter * m_zoom);
            m_tempTextItem.pos = clampAnchorOutsideBubble(m_tempTextItem.boundingRect,
                                                          m_tempTextItem.pos,
                                                          m_tempTextItem.anchor,
                                                          gapWorld);
        }
        m_textEdit = new QTextEdit(this);
        m_textEdit->setFrameStyle(QFrame::NoFrame);
        m_textEdit->setAcceptRichText(false);
        m_textEdit->setAutoFillBackground(false);
        m_textEdit->setAttribute(Qt::WA_TranslucentBackground);
        m_textEdit->document()->setDocumentMargin(0.0);
        m_textEdit->setContentsMargins(0, 0, 0, 0);
        m_textEdit->installEventFilter(this);
        // Ustaw kolor i czcionkę dla edycji
        QPalette pal = m_textEdit->palette();
        pal.setColor(QPalette::Text, m_tempTextItem.color);
        pal.setColor(QPalette::Base, Qt::transparent);
        m_textEdit->setPalette(pal);
        m_textEdit->setStyleSheet(QString("color: %1; background: transparent;").arg(m_tempTextItem.color.name()));
        m_textEdit->setFont(m_tempTextItem.font);
        m_textEdit->setPlainText(QString());
        // Dopasuj rozmiar pola do tekstu i ustaw je w odpowiedniej pozycji
        repositionTempTextEdit();
        m_textEdit->show();
        m_textEdit->setFocus();
        // Po każdej zmianie tekstu aktualizuj boundingRect i dymek
        connect(m_textEdit, &QTextEdit::textChanged, this, [this](){
            if (m_hasTempTextItem) {
                m_tempTextItem.text = m_textEdit ? m_textEdit->toPlainText() : m_tempTextItem.text;
                updateTempBoundingRect();
                repositionTempTextEdit();
                update();
            }
        });
        // Zachowaj, że to nowy dymek; m_editingTextIndex = -1
        m_editingTextIndex = -1;
        update();
        return;
    }
    // Tryb usuwania: usuń element w miejscu kliknięcia (tekst lub pomiar)
    if (m_mode == ToolMode::Delete) {
        QPointF wpos = pos;
        // Najpierw sprawdź, czy kliknięto w element tekstowy
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            if (ti.boundingRect.contains(wpos)) {
                // Usuń tekst i zakończ
                m_textItems.erase(m_textItems.begin() + i);
                if (m_selectedTextIndex == i) m_selectedTextIndex = -1;
                else if (m_selectedTextIndex > i) m_selectedTextIndex--;
                update();
                return;
            }
        }
        // W przeciwnym razie usuń najbliższy pomiar
        double bestDist = 5.0 / m_zoom;
        if (m_measurementsTool.selectMeasureAt(wpos, bestDist)) {
            m_measurementsTool.deleteSelectedMeasure();
        }
        return;
    }
    QWidget::mousePressEvent(ev);
}

void CanvasWidget::mouseDoubleClickEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(ev);
        return;
    }
    if (m_activeTool && m_activeTool->mouseDoubleClick(ev)) {
        return;
    }
    if (m_mode == ToolMode::Select) {
        QPointF wpos = toWorld(ev->position());
        double marginWorldX = 0.0;
        double marginWorldY = 0.0;
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            marginWorldX = 8.0 / (m_pixelsPerMeter * m_zoom);
            marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
        }
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            QRectF hitRect = ti.boundingRect.adjusted(-marginWorldX, -marginWorldY, marginWorldX, marginWorldY);
            if (hitRect.contains(wpos)) {
                startEditExistingText(i);
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(ev);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (m_isPanning) {
        QPointF now = ev->position();
        m_viewOffset += (now - m_lastMouseScreen);
        m_lastMouseScreen = now;
        if (m_textEdit && m_hasTempTextItem) {
            repositionTempTextEdit();
        } else if (m_textEdit && m_editingTextIndex >= 0 && m_editingTextIndex < (int)m_textItems.size()) {
            QPointF tl = toScreen(m_textItems[m_editingTextIndex].boundingRect.topLeft());
            QSizeF sizePx(m_textItems[m_editingTextIndex].boundingRect.width() * m_pixelsPerMeter * m_zoom,
                          m_textItems[m_editingTextIndex].boundingRect.height() * m_pixelsPerMeter * m_zoom);
            m_textEdit->move(tl.toPoint());
            m_textEdit->resize(std::max(40, (int)std::round(sizePx.width())),
                               std::max(20, (int)std::round(sizePx.height())));
        }
        update();
        return;
    }
    if (m_mode == ToolMode::DefineScale && m_scaleDragPoint != 0) {
        QPointF wpos = toWorld(ev->position());
        if (m_scaleDragPoint == 1) {
            if (m_scaleHasSecond && ev->modifiers().testFlag(Qt::ShiftModifier)) {
                QPointF delta = wpos - m_scaleSecondPoint;
                if (std::abs(delta.x()) >= std::abs(delta.y())) {
                    wpos.setY(m_scaleSecondPoint.y());
                } else {
                    wpos.setX(m_scaleSecondPoint.x());
                }
            }
            m_scaleFirstPoint = wpos;
        } else if (m_scaleDragPoint == 2) {
            if (m_scaleHasFirst && ev->modifiers().testFlag(Qt::ShiftModifier)) {
                QPointF delta = wpos - m_scaleFirstPoint;
                if (std::abs(delta.x()) >= std::abs(delta.y())) {
                    wpos.setY(m_scaleFirstPoint.y());
                } else {
                    wpos.setX(m_scaleFirstPoint.x());
                }
            }
            m_scaleSecondPoint = wpos;
        }
        update();
        return;
    }
    if (m_isResizingTempBubble) {
        QPointF spos = ev->position();
        QRectF rect = m_resizeStartRect;
        const double minW = 40.0;
        const double minH = 20.0;
        switch (m_resizeHandle) {
        case ResizeHandle::TopLeft:
            rect.setTopLeft(spos);
            break;
        case ResizeHandle::TopRight:
            rect.setTopRight(spos);
            break;
        case ResizeHandle::BottomLeft:
            rect.setBottomLeft(spos);
            break;
        case ResizeHandle::BottomRight:
            rect.setBottomRight(spos);
            break;
        default:
            break;
        }
        rect = rect.normalized();
        if (rect.width() < minW) rect.setWidth(minW);
        if (rect.height() < minH) rect.setHeight(minH);
        QPointF topLeftWorld = toWorld(rect.topLeft());
        QPointF bottomRightWorld = toWorld(rect.bottomRight());
        m_tempTextItem.boundingRect = QRectF(topLeftWorld, bottomRightWorld).normalized();
        m_isTempBubblePinned = true;
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            double gapWorld = 12.0 / (m_pixelsPerMeter * m_zoom);
            m_tempTextItem.pos = clampAnchorOutsideBubble(m_tempTextItem.boundingRect,
                                                          m_tempTextItem.pos,
                                                          m_tempTextItem.anchor,
                                                          gapWorld);
        }
        repositionTempTextEdit();
        update();
        return;
    }
    if (m_isResizingSelectedBubble && hasSelectedText()) {
        QPointF spos = ev->position();
        QRectF rect = m_resizeStartRect;
        const double minW = 40.0;
        const double minH = 20.0;
        switch (m_resizeHandle) {
        case ResizeHandle::TopLeft:
            rect.setTopLeft(spos);
            break;
        case ResizeHandle::TopRight:
            rect.setTopRight(spos);
            break;
        case ResizeHandle::BottomLeft:
            rect.setBottomLeft(spos);
            break;
        case ResizeHandle::BottomRight:
            rect.setBottomRight(spos);
            break;
        default:
            break;
        }
        rect = rect.normalized();
        if (rect.width() < minW) rect.setWidth(minW);
        if (rect.height() < minH) rect.setHeight(minH);
        QPointF topLeftWorld = toWorld(rect.topLeft());
        QPointF bottomRightWorld = toWorld(rect.bottomRight());
        m_textItems[m_selectedTextIndex].boundingRect = QRectF(topLeftWorld, bottomRightWorld).normalized();
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            double gapWorld = 12.0 / (m_pixelsPerMeter * m_zoom);
            TextItem &ti = m_textItems[m_selectedTextIndex];
            ti.pos = clampAnchorOutsideBubble(ti.boundingRect, ti.pos, ti.anchor, gapWorld);
        }
        if (m_textEdit && m_editingTextIndex == m_selectedTextIndex) {
            m_textEdit->move(rect.topLeft().toPoint());
            m_textEdit->resize(std::max(40, (int)std::round(rect.width())),
                               std::max(20, (int)std::round(rect.height())));
        }
        update();
        return;
    }
    // Przeciąganie tymczasowego dymka w trybie InsertText
    if (m_mode == ToolMode::InsertText && m_hasTempTextItem) {
        if (m_isDraggingTempBubble) {
            // Przesuwamy boundingRect względem kotwicy i korygujemy kotwicę
            // tak, aby pozostała poza dymkiem.
            QPointF wpos = toWorld(ev->position());
            QPointF newTopLeft = wpos - m_tempDragOffset;
            QPointF delta = newTopLeft - m_tempTextItem.boundingRect.topLeft();
            m_tempTextItem.boundingRect.translate(delta.x(), delta.y());
            if (m_pixelsPerMeter * m_zoom != 0.0) {
                double gapWorld = 12.0 / (m_pixelsPerMeter * m_zoom);
                m_tempTextItem.pos = clampAnchorOutsideBubble(m_tempTextItem.boundingRect,
                                                              m_tempTextItem.pos,
                                                              m_tempTextItem.anchor,
                                                              gapWorld);
            }
            // Przesuń pole edycji
            repositionTempTextEdit();
            update();
            return;
        }
        if (m_isDraggingTempAnchor) {
            // Zmień pozycję kotwicy bez przesuwania dymka
            QPointF wpos = toWorld(ev->position());
            m_tempTextItem.anchor = anchorFromPosition(m_tempTextItem.boundingRect, wpos);
            double gapWorld = (m_pixelsPerMeter * m_zoom != 0.0)
                ? 12.0 / (m_pixelsPerMeter * m_zoom)
                : 0.0;
            m_tempTextItem.pos = clampAnchorOutsideBubble(m_tempTextItem.boundingRect,
                                                          wpos,
                                                          m_tempTextItem.anchor,
                                                          gapWorld);
            update();
            return;
        }
    }
    // Jeśli przeciągamy zaznaczony tekst w trybie zaznaczania lub jego kotwicę
    if (m_mode == ToolMode::Select && hasSelectedText()) {
        if (m_isDraggingSelectedText) {
            // Przeciąganie całego dymka – przesuwamy boundingRect i pilnujemy
            // aby kotwica nie znalazła się wewnątrz dymka.
            QPointF wpos = toWorld(ev->position());
            TextItem &ti = m_textItems[m_selectedTextIndex];
            QPointF newTopLeft = wpos - m_dragStartOffset;
            QPointF delta = newTopLeft - ti.boundingRect.topLeft();
            ti.boundingRect.translate(delta.x(), delta.y());
            if (m_pixelsPerMeter * m_zoom != 0.0) {
                double gapWorld = 12.0 / (m_pixelsPerMeter * m_zoom);
                ti.pos = clampAnchorOutsideBubble(ti.boundingRect, ti.pos, ti.anchor, gapWorld);
            }
            update();
            return;
        }
        if (m_isDraggingSelectedAnchor) {
            // Przeciąganie kotwicy bez przesuwania dymka
            QPointF wpos = toWorld(ev->position());
            TextItem &ti = m_textItems[m_selectedTextIndex];
            ti.anchor = anchorFromPosition(ti.boundingRect, wpos);
            double gapWorld = (m_pixelsPerMeter * m_zoom != 0.0)
                ? 12.0 / (m_pixelsPerMeter * m_zoom)
                : 0.0;
            ti.pos = clampAnchorOutsideBubble(ti.boundingRect, wpos, ti.anchor, gapWorld);
            update();
            return;
        }
    }
    m_mouseWorld = toWorld(ev->position());
    m_hasMouseWorld = true;
    if (m_activeTool) {
        m_activeTool->mouseMove(ev, m_mouseWorld);
    }
    update();
    QWidget::mouseMoveEvent(ev);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton) {
        m_isPanning = false;
        return;
    }
    if (m_mode == ToolMode::DefineScale && ev->button() == Qt::LeftButton) {
        m_scaleDragPoint = 0;
    }
    if (m_activeTool && m_activeTool->mouseRelease(ev)) {
        return;
    }
    // Zakończ przeciąganie dymków i kotwic
    if (ev->button() == Qt::LeftButton) {
        if (m_mode == ToolMode::InsertText) {
            m_isDraggingTempBubble = false;
            m_isDraggingTempAnchor = false;
        }
        if (m_mode == ToolMode::Select) {
            if (m_isDraggingSelectedText) {
                m_isDraggingSelectedText = false;
            }
            if (m_isDraggingSelectedAnchor) {
                m_isDraggingSelectedAnchor = false;
            }
        }
        m_isResizingTempBubble = false;
        m_isResizingSelectedBubble = false;
        m_resizeHandle = ResizeHandle::None;
        releaseMouse();
    }
    QWidget::mouseReleaseEvent(ev);
}

void CanvasWidget::wheelEvent(QWheelEvent* ev) {
    const int delta = ev->angleDelta().y();
    if (delta == 0) { ev->accept(); return; }
    const double factor = (delta > 0) ? 1.1 : (1.0/1.1);
    QPointF screenPos = ev->position();
    QPointF worldPos = toWorld(screenPos);
    m_zoom *= factor;
    if (m_zoom < 0.1) m_zoom = 0.1;
    if (m_zoom > 50.0) m_zoom = 50.0;
    m_viewOffset = screenPos - worldPos * m_zoom;
    if (m_textEdit && m_hasTempTextItem) {
        repositionTempTextEdit();
    } else if (m_textEdit && m_editingTextIndex >= 0 && m_editingTextIndex < (int)m_textItems.size()) {
        QPointF tl = toScreen(m_textItems[m_editingTextIndex].boundingRect.topLeft());
        QSizeF sizePx(m_textItems[m_editingTextIndex].boundingRect.width() * m_pixelsPerMeter * m_zoom,
                      m_textItems[m_editingTextIndex].boundingRect.height() * m_pixelsPerMeter * m_zoom);
        m_textEdit->move(tl.toPoint());
        m_textEdit->resize(std::max(40, (int)std::round(sizePx.width())),
                           std::max(20, (int)std::round(sizePx.height())));
    }
    update();
    ev->accept();
}

// --- Obsługa edycji tekstu w stylu Paint ---
void CanvasWidget::startTextEdit(const QPointF &worldPos, const QPointF &screenPos) {
    // Jeśli trwa inna edycja, zatwierdź ją przed rozpoczęciem nowej
    if (m_textEdit) {
        commitTextEdit();
    }
    // Utwórz nowe pole edycyjne
    m_textEdit = new QTextEdit(this);
    m_textEdit->setFrameStyle(QFrame::NoFrame);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setAutoFillBackground(false);
    m_textEdit->setAttribute(Qt::WA_TranslucentBackground);
    m_textEdit->document()->setDocumentMargin(0.0);
    m_textEdit->setContentsMargins(0, 0, 0, 0);
    m_textEdit->installEventFilter(this);
    // Zastosuj kolor tekstu poprzez paletę i CSS (CSS zapewnia bardziej
    // niezawodne ustawienie koloru w niektórych motywach)
    QPalette pal = m_textEdit->palette();
    pal.setColor(QPalette::Text, m_insertTextColor);
    pal.setColor(QPalette::Base, Qt::transparent);
    m_textEdit->setPalette(pal);
    m_textEdit->setStyleSheet(QString("color: %1; background: transparent;").arg(m_insertTextColor.name()));
    // Ustaw czcionkę
    m_textEdit->setFont(m_insertTextFont);
    // Pusty tekst początkowy
    m_textEdit->setPlainText(QString());
    // Ustaw pozycję na ekranie (współrzędne lokalne)
    QPoint pt = screenPos.toPoint();
    m_textEdit->move(pt);
    // Ustaw minimalny rozmiar, aby pole było widoczne
    m_textEdit->resize(120, 40);
    m_textEdit->show();
    m_textEdit->setFocus();
    // Zapisz pozycję świata i wyczyść wskaźniki
    m_textInsertPos = worldPos;
    m_hasTextInsertPos = true;
    m_editingTextIndex = -1;
    // Ustaw tryb InsertText, aby eventy myszy nie były interpretowane jako wybór
    m_mode = ToolMode::InsertText;
}

void CanvasWidget::commitTextEdit() {
    if (!m_textEdit) {
        return;
    }
    // Pobierz tekst i kolor czcionki z pola
    QString text = m_textEdit->toPlainText().trimmed();
    const double marginX = 8.0;
    const double marginY = 6.0;
    QSizeF docSize = m_textEdit->document()->size();
    QTextEdit* edit = m_textEdit;
    m_textEdit = nullptr;
    edit->deleteLater();
    // Jeżeli wstawiany jest tymczasowy dymek (m_hasTempTextItem), to
    // commitTempTextItem() obsłuży finalizację.  Wywołaj je i zakończ.
    if (m_hasTempTextItem) {
        if (text.isEmpty()) {
            cancelTempTextItem();
            return;
        }
        // Ustaw treść tymczasowego dymka i finalizuj
        m_tempTextItem.text = text;
        commitTempTextItem();
        return;
    }
    // Edycja istniejącego tekstu
    if (m_editingTextIndex >= 0 && m_editingTextIndex < (int)m_textItems.size()) {
        // Jeśli tekst jest pusty, usuń element
        if (text.isEmpty()) {
            m_textItems.erase(m_textItems.begin() + m_editingTextIndex);
            m_selectedTextIndex = -1;
        } else {
            TextItem &ti = m_textItems[m_editingTextIndex];
            ti.text = text;
            // Kolor i czcionka nie są zmieniane tutaj – użytkownik
            // ustawia je z panelu poprzez setSelectedTextColor/Font.
            double pixPerM = m_pixelsPerMeter * m_zoom;
            if (pixPerM <= 0.0) pixPerM = 1.0;
            double w_m = (docSize.width() + marginX * 2) / pixPerM;
            double h_m = (docSize.height() + marginY * 2) / pixPerM;
            double x_m = ti.boundingRect.left();
            double y_m = ti.boundingRect.top();
            if (ti.boundingRect.isNull()) {
                x_m = ti.pos.x() - w_m / 2.0;
                y_m = ti.pos.y() - h_m;
            }
            ti.boundingRect = QRectF(x_m, y_m, w_m, h_m);
            // Ustaw zaznaczenie na edytowany element
            m_selectedTextIndex = m_editingTextIndex;
            m_measurementsTool.clearSelection();
        }
        m_editingTextIndex = -1;
        m_mode = ToolMode::None;
        update();
        emit measurementFinished();
        return;
    }
    // Jeśli brak edycji i brak tymczasowego dymka, to znaczy, że
    // nastąpiła próba zatwierdzenia bez wstawiania – po prostu
    // zakończ tryb
    m_mode = ToolMode::None;
    update();
    emit measurementFinished();
}

// --- Nowe funkcje do obsługi tymczasowego dymka tekstowego ---

void CanvasWidget::updateTempBoundingRect() {
    if (!m_hasTempTextItem) return;
    // Stałe marginesu i odstępu na grot w pikselach
    const double marginX = 8.0;
    const double marginY = 6.0;
    const double tailGapPx = 12.0;

    // Ustal wymiary tekstu: jeśli istnieje QTextEdit, korzystamy z jego dokumentu;
    // w przeciwnym razie obliczamy wielkość w oparciu o czcionkę m_tempTextItem.
    QSizeF docSize;
    if (m_textEdit) {
        docSize = m_textEdit->document()->size();
    } else {
        QString txt = m_tempTextItem.text;
        if (txt.isEmpty()) txt = QStringLiteral(" ");
        QFont f = m_tempTextItem.font;
        QFontMetrics fm(f);
        docSize = QSizeF(fm.horizontalAdvance(txt), fm.height());
    }

    // Szerokość i wysokość dymka w jednostkach świata (metry) z uwzględnieniem marginesów
    double w_m = 0.0;
    double h_m = 0.0;
    if (m_pixelsPerMeter * m_zoom != 0.0) {
        w_m = (docSize.width() + marginX * 2) / (m_pixelsPerMeter * m_zoom);
        h_m = (docSize.height() + marginY * 2) / (m_pixelsPerMeter * m_zoom);
    }

    // Jeśli dymek był przypięty, nie zmniejszaj jego rozmiaru poniżej poprzedniego
    if (m_isTempBubblePinned && !m_tempTextItem.boundingRect.isNull()) {
        w_m = std::max(w_m, m_tempTextItem.boundingRect.width());
        h_m = std::max(h_m, m_tempTextItem.boundingRect.height());
    }

    // Odstęp na grot w metrach; jeśli skala nie została zdefiniowana, użyj zastępczej wartości, aby uniknąć dzielenia przez zero
    double pixPerM = m_pixelsPerMeter * m_zoom;
    if (pixPerM <= 0.0) pixPerM = 1.0;
    const double gapWorld = tailGapPx / pixPerM;

    // Wyznacz nową ramkę dymka względem kotwicy za pomocą bubbleRectForAnchor()
    QRectF newRect = bubbleRectForAnchor(m_tempTextItem.pos,
                                         QSizeF(w_m, h_m),
                                         m_tempTextItem.anchor,
                                         gapWorld);

    // Jeśli dymek jest przypięty, zachowaj co najmniej poprzedni rozmiar
    if (m_isTempBubblePinned && !m_tempTextItem.boundingRect.isNull()) {
        newRect.setSize(QSizeF(std::max(w_m, m_tempTextItem.boundingRect.width()),
                               std::max(h_m, m_tempTextItem.boundingRect.height())));
    }

    // Zapisz nowy prostokąt jako boundingRect
    m_tempTextItem.boundingRect = newRect;

    // Kotwica musi zawsze pozostać poza dymkiem, niezależnie od przypięcia
    m_tempTextItem.pos = clampAnchorOutsideBubble(m_tempTextItem.boundingRect,
                                                  m_tempTextItem.pos,
                                                  m_tempTextItem.anchor,
                                                  gapWorld);
}

void CanvasWidget::repositionTempTextEdit() {
    if (!m_hasTempTextItem || !m_textEdit) return;
    // Oblicz pozycję lewego górnego rogu prostokąta tekstu w ekranie
    QPointF topLeftScreen = toScreen(m_tempTextItem.boundingRect.topLeft());
    QSizeF sizePx(m_tempTextItem.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                  m_tempTextItem.boundingRect.height() * m_pixelsPerMeter * m_zoom);
    int width = std::max(40, (int)std::round(sizePx.width()));
    int height = std::max(20, (int)std::round(sizePx.height()));
    // Ustaw pozycję edytora bezpośrednio w lewym górnym rogu boundingRect
    m_textEdit->move(topLeftScreen.toPoint());
    m_textEdit->resize(width, height);
}

void CanvasWidget::commitTempTextItem() {
    if (!m_hasTempTextItem) return;
    // Usuń pole edycyjne
    if (m_textEdit) {
        QString txt = m_textEdit->toPlainText().trimmed();
        // Usuń QTextEdit
        QTextEdit* edit = m_textEdit;
        m_textEdit = nullptr;
        edit->deleteLater();
        // Jeśli tekst jest pusty, anuluj wstawianie
        if (txt.isEmpty()) {
            cancelTempTextItem();
            return;
        }
        m_tempTextItem.text = txt;
    }
    // Finalizuj boundingRect dla bieżącej treści
    updateTempBoundingRect();
    // Dodaj element do listy tekstów
    m_textItems.push_back(m_tempTextItem);
    // Ustaw zaznaczenie na nowo dodany element
    m_selectedTextIndex = (int)m_textItems.size() - 1;
    m_measurementsTool.clearSelection();
    // Resetuj stan tymczasowego dymka
    m_hasTempTextItem = false;
    m_isDraggingTempBubble = false;
    m_isDraggingTempAnchor = false;
    m_isTempBubblePinned = false;
    // Wróć do trybu None
    m_mode = ToolMode::None;
    update();
    emit measurementFinished();
}

void CanvasWidget::cancelTempTextItem() {
    if (!m_hasTempTextItem) return;
    // Usuń pole edycyjne
    if (m_textEdit) {
        QTextEdit* edit = m_textEdit;
        m_textEdit = nullptr;
        edit->deleteLater();
    }
    // Resetuj flagi i tryb
    m_hasTempTextItem = false;
    m_isDraggingTempBubble = false;
    m_isDraggingTempAnchor = false;
    m_isTempBubblePinned = false;
    m_mode = ToolMode::None;
    update();
    emit measurementFinished();
}

void CanvasWidget::commitActiveTextEdit() {
    if (m_isCommittingText) return;
    if (!m_textEdit) return;
    m_isCommittingText = true;
    if (m_hasTempTextItem) {
        commitTempTextItem();
    } else if (m_editingTextIndex >= 0) {
        commitTextEdit();
    }
    m_isCommittingText = false;
}

void CanvasWidget::cancelTextEdit() {
    // Anuluj edycję bez zapisu
    if (m_textEdit) {
        QTextEdit* edit = m_textEdit;
        m_textEdit = nullptr;
        edit->deleteLater();
    }
    // Jeśli trwa wstawianie tymczasowego dymka, po prostu usuń dymek
    if (m_hasTempTextItem) {
        cancelTempTextItem();
        return;
    }
    // Jeśli edytowany był istniejący element tekstowy, przywróć tryb
    if (m_editingTextIndex >= 0) {
        m_editingTextIndex = -1;
        m_mode = ToolMode::None;
        update();
        emit measurementFinished();
        return;
    }
    // Ogólne anulowanie: resetuj pozycję wstawiania i tryb
    m_hasTextInsertPos = false;
    m_mode = ToolMode::None;
    update();
    emit measurementFinished();
}

void CanvasWidget::keyPressEvent(QKeyEvent* ev) {
    if (m_activeTool && m_activeTool->keyPress(ev, this)) {
        return;
    }
    if (m_mode == ToolMode::DefineScale) {
        if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
            confirmScaleStep(this);
            return;
        }
        if (ev->key() == Qt::Key_Backspace) {
            removeScalePoint();
            return;
        }
    }
    switch (ev->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            break;
        case Qt::Key_Backspace:
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal: {
            QPointF center(width()/2.0, height()/2.0);
            QPointF worldCenter = toWorld(center);
            m_zoom *= 1.1; if (m_zoom > 50.0) m_zoom = 50.0;
            m_viewOffset = center - worldCenter * m_zoom; update(); break;
        }
        case Qt::Key_Minus: {
            QPointF center(width()/2.0, height()/2.0);
            QPointF worldCenter = toWorld(center);
            m_zoom /= 1.1; if (m_zoom < 0.1) m_zoom = 0.1;
            m_viewOffset = center - worldCenter * m_zoom; update(); break;
        }
        case Qt::Key_Escape:
            // Anuluj edycję dymka lub aktualny tryb
            if (m_mode == ToolMode::InsertText) {
                // Jeśli wstawiany jest tymczasowy dymek, usuń go
                if (m_hasTempTextItem) {
                    cancelTempTextItem();
                } else if (m_textEdit) {
                    // Jeżeli edytujemy istniejący tekst, anuluj edycję
                    cancelTextEdit();
                } else {
                    // W trybie InsertText bez aktywnego dymka po prostu wyjdź
                    m_mode = ToolMode::None;
                    unsetCursor();
                    update();
                }
            } else if (m_textEdit) {
                // Jeżeli trwa edycja tekstu w innych trybach (np. Select), anuluj
                cancelTextEdit();
            } else if (m_mode == ToolMode::DefineScale) {
                m_mode = ToolMode::None;
                m_scaleStep = ScaleStep::None;
                m_scaleHasFirst = false;
                m_scaleHasSecond = false;
                m_scaleDragPoint = 0;
                emitScaleStateChanged();
                emit scaleFinished();
                unsetCursor();
                update();
            } else {
                // Ogólne anulowanie: wyczyść punkty i zaznaczenia
                m_mode = ToolMode::None;
                m_measurementsTool.deactivate();
                m_activeTool = nullptr;
                m_measurementsTool.clearSelection();
                m_selectedTextIndex = -1;
                m_isDraggingSelectedText = false;
                m_isDraggingSelectedAnchor = false;
                unsetCursor();
                update();
            }
            break;
        default: QWidget::keyPressEvent(ev);
    }
}

bool CanvasWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_textEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEv = static_cast<QKeyEvent*>(event);
            if ((keyEv->key() == Qt::Key_Return || keyEv->key() == Qt::Key_Enter)
                && keyEv->modifiers().testFlag(Qt::ControlModifier)) {
                commitActiveTextEdit();
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut) {
            commitActiveTextEdit();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CanvasWidget::applyScaleFromPoints(QWidget* parent) {
    if (!m_scaleHasFirst || !m_scaleHasSecond) {
        return;
    }
    const double dx = m_scaleSecondPoint.x() - m_scaleFirstPoint.x();
    const double dy = m_scaleSecondPoint.y() - m_scaleFirstPoint.y();
    const double distPx = std::hypot(dx, dy);
    if (distPx <= 0.0) {
        return;
    }

    bool ok = false;
    double defaultVal = 300.0;
    int decimals = m_settings->decimals;
    double val = QInputDialog::getDouble(parent ? parent : this,
                                         QString::fromUtf8("Skalowanie"),
                                         QString::fromUtf8("Podaj wartość odległości [cm]:"),
                                         defaultVal,
                                         0.001,
                                         100000.0,
                                         decimals,
                                         &ok);
    if (!ok) {
        return;
    }

    double oldPixelsPerMeter = m_pixelsPerMeter;
    m_pixelsPerMeter = distPx / val;
    m_measurementsTool.recalculateLengths();
    update();
}

void CanvasWidget::scaleCanvasContents(double factor) {
    if (factor == 1.0) {
        return;
    }
    QPointF screenCenter(width() / 2.0, height() / 2.0);
    QPointF worldCenter = toWorld(screenCenter);
    m_measurementsTool.scaleContent(factor);
    for (auto &txt : m_textItems) {
        txt.pos.setX(txt.pos.x() * factor);
        txt.pos.setY(txt.pos.y() * factor);
        QPointF topLeft = txt.boundingRect.topLeft() * factor;
        QPointF bottomRight = txt.boundingRect.bottomRight() * factor;
        txt.boundingRect = QRectF(topLeft, bottomRight).normalized();
    }
    if (m_hasTempTextItem) {
        m_tempTextItem.pos.setX(m_tempTextItem.pos.x() * factor);
        m_tempTextItem.pos.setY(m_tempTextItem.pos.y() * factor);
        QPointF topLeft = m_tempTextItem.boundingRect.topLeft() * factor;
        QPointF bottomRight = m_tempTextItem.boundingRect.bottomRight() * factor;
        m_tempTextItem.boundingRect = QRectF(topLeft, bottomRight).normalized();
        repositionTempTextEdit();
    }
    if (m_textEdit && m_editingTextIndex >= 0 && m_editingTextIndex < (int)m_textItems.size()) {
        QPointF tl = toScreen(m_textItems[m_editingTextIndex].boundingRect.topLeft());
        QSizeF sizePx(m_textItems[m_editingTextIndex].boundingRect.width() * m_pixelsPerMeter * m_zoom,
                      m_textItems[m_editingTextIndex].boundingRect.height() * m_pixelsPerMeter * m_zoom);
        m_textEdit->move(tl.toPoint());
        m_textEdit->resize(std::max(40, (int)std::round(sizePx.width())),
                           std::max(20, (int)std::round(sizePx.height())));
    }
    m_viewOffset = screenCenter - (worldCenter * factor) * m_zoom;
}

void CanvasWidget::openReportDialog(QWidget* parent)
{
    m_measurementsTool.openReportDialog(parent);
}
