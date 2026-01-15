#include "CanvasWidget.h"
#include <unordered_map>
#include "Dialogs.h"
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
} // namespace

CanvasWidget::CanvasWidget(QWidget* parent, ProjectSettings* settings)
    : QWidget(parent), m_settings(settings) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    // Inicjalizuj lokalne wartości koloru i grubości linii z globalnych ustawień.
    if (m_settings) {
        m_currentColor     = m_settings->defaultMeasureColor;
        m_currentLineWidth = m_settings->lineWidthPx;
    }

    // Inicjuj dodatkowe pola narzędzi
    m_selectedMeasureIndex = -1;

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
    m_layerVisibility[QStringLiteral("Tekst")]       = true;

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
    // Anulowanie: czyść aktualne punkty, stos redo i wróć do trybu None
    m_currentPts.clear();
    m_redoPts.clear();
    m_mode = ToolMode::None;
    // Usuń zaznaczone elementy i zatrzymaj przeciąganie tekstu
    m_selectedMeasureIndex = -1;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    // Przywróć domyślny kursor
    unsetCursor();
    update();
}

void CanvasWidget::undoCurrentMeasure() {
    if (!m_currentPts.empty()) {
        // Przenieś ostatni punkt z m_currentPts do m_redoPts
        m_redoPts.push_back(m_currentPts.back());
        m_currentPts.pop_back();
        update();
    }
}

void CanvasWidget::redoCurrentMeasure() {
    if (!m_redoPts.empty()) {
        // Przywróć ostatnio cofnięty punkt
        m_currentPts.push_back(m_redoPts.back());
        m_redoPts.pop_back();
        update();
    }
}

void CanvasWidget::confirmCurrentMeasure(QWidget* parentForAdvanced) {
    // Zatwierdzenie aktualnego pomiaru jest tożsame z zakończeniem pomiaru
    finishCurrentMeasure(parentForAdvanced);
}

void CanvasWidget::insertPendingText(const QString& text) {
    // Wstaw tekst tylko, jeśli ustalono pozycję
    if (!m_hasTextInsertPos) return;
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return;
    // Oblicz boundingRect w jednostkach świata oraz ustaw kierunek strzałki
    QFont f = m_insertTextFont;
    QFontMetrics fm(f);
    int textW = fm.horizontalAdvance(trimmed);
    int textH = fm.height();
    double w_m = textW / (m_pixelsPerMeter * m_zoom);
    double h_m = textH / (m_pixelsPerMeter * m_zoom);
    // Określ domyślny kierunek dla nowych tekstów
    CalloutAnchor anchor = m_insertTextAnchor;
    // Oblicz górny lewy róg prostokąta w świecie w zależności od kierunku
    double x_m, y_m;
    switch (anchor) {
    case CalloutAnchor::Bottom:
        x_m = m_textInsertPos.x() - w_m / 2.0;
        y_m = m_textInsertPos.y() - h_m;
        break;
    case CalloutAnchor::Top:
        x_m = m_textInsertPos.x() - w_m / 2.0;
        y_m = m_textInsertPos.y();
        break;
    case CalloutAnchor::Left:
        x_m = m_textInsertPos.x();
        y_m = m_textInsertPos.y() - h_m / 2.0;
        break;
    case CalloutAnchor::Right:
        x_m = m_textInsertPos.x() - w_m;
        y_m = m_textInsertPos.y() - h_m / 2.0;
        break;
    }
    QRectF worldRect(x_m, y_m, w_m, h_m);
    TextItem item;
    item.pos = m_textInsertPos;
    item.text = trimmed;
    item.color = m_insertTextColor;
    item.font = m_insertTextFont;
    item.anchor = anchor;
    item.boundingRect = worldRect;
    // Ustaw kolory wypełnienia i obramowania z bieżących domyślnych
    item.bgColor = m_insertBubbleFillColor;
    item.borderColor = m_insertBubbleBorderColor;
    // Warstwa dla tekstu
    item.layer = QStringLiteral("Tekst");
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
    for (auto &m : m_measures) {
        m.color = m_settings->defaultMeasureColor;
    }
    update();
}

// Aktualizuje grubość linii wszystkich istniejących pomiarów na wartość
// zdefiniowaną w ustawieniach projektu.  Nie dotyka linii aktualnie
// rysowanego pomiaru ani wartości m_currentLineWidth.
void CanvasWidget::updateAllMeasureLineWidths() {
    if (!m_settings) return;
    for (auto &m : m_measures) {
        m.lineWidthPx = qBound(1, m_settings->lineWidthPx, 8);
    }
    update();
}

// Rozpoczyna tryb zaznaczania istniejących pomiarów.  Czyści bieżące
// punkty i stos cofnięć, ustawia m_mode i resetuje zaznaczenie.
void CanvasWidget::startSelect() {
    m_mode = ToolMode::Select;
    m_currentPts.clear();
    m_redoPts.clear();
    m_pendingText.clear();
    m_selectedMeasureIndex = -1;
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
    m_currentPts.clear();
    m_redoPts.clear();
    m_selectedMeasureIndex = -1;
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
    m_currentPts.clear();
    m_redoPts.clear();
    m_pendingText.clear();
    m_selectedMeasureIndex = -1;
    // Clear text selection and stop any dragging
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    // Use cross cursor to indicate deletion (eraser-like)
    setCursor(Qt::CrossCursor);
    update();
}

// Ustawia kolor zaznaczonego pomiaru
void CanvasWidget::setSelectedMeasureColor(const QColor &c) {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        m_measures[m_selectedMeasureIndex].color = c;
        update();
    }
}

// Ustawia grubość linii zaznaczonego pomiaru
void CanvasWidget::setSelectedMeasureLineWidth(int w) {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        int bounded = qBound(1, w, 8);
        m_measures[m_selectedMeasureIndex].lineWidthPx = bounded;
        update();
    }
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
    double w_m = textW / (m_pixelsPerMeter * m_zoom);
    double h_m = textH / (m_pixelsPerMeter * m_zoom);
    // Zachowaj bieżącą pozycję (punkt bazowy tekstu)
    QPointF pos = m_textItems[m_selectedTextIndex].pos;
    m_textItems[m_selectedTextIndex].boundingRect = QRectF(pos.x(), pos.y() - h_m, w_m, h_m);
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
    QFontMetrics fm(font);
    double widthPx = ti.boundingRect.width() * m_pixelsPerMeter * m_zoom;
    if (widthPx <= 0.0) {
        widthPx = fm.horizontalAdvance(trimmed);
    }
    QRect textBounds = fm.boundingRect(0, 0, (int)std::ceil(widthPx), 10000,
                                       Qt::TextWordWrap, trimmed);
    double w_m = 0.0;
    double h_m = 0.0;
    if (m_pixelsPerMeter * m_zoom != 0.0) {
        w_m = textBounds.width() / (m_pixelsPerMeter * m_zoom);
        h_m = textBounds.height() / (m_pixelsPerMeter * m_zoom);
    }
    double x_m = ti.boundingRect.left();
    double y_m = ti.boundingRect.top();
    if (ti.boundingRect.isNull()) {
        x_m = ti.pos.x() - w_m / 2.0;
        y_m = ti.pos.y() - h_m;
    }
    ti.boundingRect = QRectF(x_m, y_m, w_m, h_m);
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
    // Oblicz wymiary tekstu w pikselach na podstawie bieżącej czcionki
    QFontMetrics fm(ti.font);
    int textW = fm.horizontalAdvance(ti.text);
    int textH = fm.height();
    double w_m = textW / (m_pixelsPerMeter * m_zoom);
    double h_m = textH / (m_pixelsPerMeter * m_zoom);
    double x_m, y_m;
    switch (a) {
    case CalloutAnchor::Bottom:
        x_m = ti.pos.x() - w_m / 2.0;
        y_m = ti.pos.y() - h_m;
        break;
    case CalloutAnchor::Top:
        x_m = ti.pos.x() - w_m / 2.0;
        y_m = ti.pos.y();
        break;
    case CalloutAnchor::Left:
        x_m = ti.pos.x();
        y_m = ti.pos.y() - h_m / 2.0;
        break;
    case CalloutAnchor::Right:
        x_m = ti.pos.x() - w_m;
        y_m = ti.pos.y() - h_m / 2.0;
        break;
    }
    ti.boundingRect = QRectF(x_m, y_m, w_m, h_m);
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
    m_textEdit->installEventFilter(this);
    // Ustaw kolor tekstu i czcionkę zgodnie z istniejącym elementem
    QPalette pal = m_textEdit->palette();
    pal.setColor(QPalette::Text, ti.color);
    m_textEdit->setPalette(pal);
    m_textEdit->setStyleSheet(QString("color: %1;").arg(ti.color.name()));
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
        QSizeF docSize = m_textEdit ? m_textEdit->document()->size() : QSizeF();
        double w_m = 0.0;
        double h_m = 0.0;
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            w_m = docSize.width() / (m_pixelsPerMeter * m_zoom);
            h_m = docSize.height() / (m_pixelsPerMeter * m_zoom);
        }
        double x_m = t.boundingRect.left();
        double y_m = t.boundingRect.top();
        if (t.boundingRect.isNull()) {
            x_m = t.pos.x() - w_m / 2.0;
            y_m = t.pos.y() - h_m;
        }
        t.boundingRect = QRectF(x_m, y_m, w_m, h_m);
        // Przenieś pole edycyjne do nowej pozycji i rozmiaru
        QPointF tl = toScreen(t.boundingRect.topLeft());
        int width2 = std::max(40, (int)std::round(docSize.width()));
        int height2 = std::max(20, (int)std::round(docSize.height()));
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
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        m_measures.erase(m_measures.begin() + m_selectedMeasureIndex);
        m_selectedMeasureIndex = -1;
        update();
    }
}

/**
 * Ustawia kolor bieżącego pomiaru.  Nie modyfikuje domyślnego koloru w
 * m_settings. Jeśli pomiar jest typu MeasureAdvanced, aktualizuje
 * równocześnie kolor w szablonie, aby zmiana została zapisana w
 * zmienionej kopii m_advTemplate w finishCurrentMeasure.
 */
void CanvasWidget::setCurrentColor(const QColor &c) {
    m_currentColor = c;
    // Jeśli pomiar zaawansowany, zmień kolor w szablonie
    if (m_mode == ToolMode::MeasureAdvanced) {
        m_advTemplate.color = c;
    }
    update();
}

/**
 * Ustawia grubość linii bieżącego pomiaru. Nie modyfikuje ustawień globalnych.
 * Jeśli pomiar jest zaawansowany, grubość jest też przypisywana do
 * szablonu, aby była zapisana wraz z pomiarem.
 */
void CanvasWidget::setCurrentLineWidth(int w) {
    m_currentLineWidth = qBound(1, w, 8);
    if (m_mode == ToolMode::MeasureAdvanced) {
        m_advTemplate.lineWidthPx = m_currentLineWidth;
    }
    update();
}

/**
 * Aktualizuje globalny zapas wszystkich istniejących pomiarów.
 *
 * Wartość m_settings->defaultBuffer jest interpretowana w aktualnej
 * jednostce projektu (cm lub m) i konwertowana do metrów.  Następnie
 * każdemu zapisowi pomiaru przypisywana jest ta wartość w polu
 * Measure::bufferGlobalMeters.  Po aktualizacji przeliczana jest
 * całkowita długość z zapasami (length + bufferGlobal + bufferDefault + bufferFinal).
 */
void CanvasWidget::updateAllMeasureGlobalBuffers() {
    // Aktualizuj wartość globalnego zapasu w każdym pomiarze.  Domyślny
    // zapas z ustawień jest interpretowany w bieżącej jednostce (cm lub m),
    // a następnie zapisywany w polu bufferGlobalMeters każdego pomiaru.
    double defBufM;
    if (m_settings->defaultUnit == ProjectSettings::Unit::Cm) {
        defBufM = m_settings->defaultBuffer / 100.0;
    } else {
        defBufM = m_settings->defaultBuffer;
    }
    for (auto &m : m_measures) {
        // Ustaw globalny zapas dla pomiaru na nową wartość
        m.bufferGlobalMeters = defBufM;
        // Oblicz ponownie łączną długość z zapasami: zawiera
        // długość, globalny zapas, zapas początkowy oraz zapas końcowy.
        m.totalWithBufferMeters = m.lengthMeters + m.bufferGlobalMeters + m.bufferDefaultMeters + m.bufferFinalMeters;
    }
    update();
}

bool CanvasWidget::loadBackgroundFile(const QString& file) {
    QFileInfo fi(file);
    const QString ext = fi.suffix().toLower();
    if (ext == "pdf") return loadPdfFirstPage(file);
    QImageReader reader(file); reader.setAutoTransform(true);
    QImage img = reader.read();
    if (!img.isNull()) { m_bgImage = img; update(); return true; }
    return false;
}

bool CanvasWidget::loadPdfFirstPage(const QString& file) {
    QPdfDocument pdf;
    auto err = pdf.load(file);
    if (err != QPdfDocument::Error::None) return false;
    if (pdf.pageCount() < 1) return false;

    const int pageIndex = 0;
    const QSizeF pt = pdf.pagePointSize(pageIndex);
    if (pt.isEmpty()) return false;

    const double targetWidth = 2000.0;
    const double scale = targetWidth / pt.width();
    const QSize imgSize(qMax(1, int(pt.width() * scale)),
                        qMax(1, int(pt.height() * scale)));

    QImage img = pdf.render(pageIndex, imgSize);
    if (img.isNull()) return false;

    m_bgImage = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    update();
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
void CanvasWidget::toggleMeasuresVisibility() { m_showMeasures = !m_showMeasures; update(); }
void CanvasWidget::startScaleDefinition(double) { m_hasFirst = false; m_mode = ToolMode::DefineScale; }

void CanvasWidget::startMeasureLinear() {
    // Rozpocznij rysowanie odcinka: wyczyść punkty i stos cofnięć
    m_mode = ToolMode::MeasureLinear;
    m_currentPts.clear();
    m_redoPts.clear();
    m_selectedMeasureIndex = -1;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    setCursor(Qt::CrossCursor);
    // Ustaw początkowy kolor i grubość linii na podstawie ustawień globalnych
    if (m_settings) {
        m_currentColor     = m_settings->defaultMeasureColor;
        m_currentLineWidth = m_settings->lineWidthPx;
    }
}
void CanvasWidget::startMeasurePolyline() {
    // Rozpocznij rysowanie polilinii: czyść listę punktów i stos cofnięć
    m_mode = ToolMode::MeasurePolyline;
    m_currentPts.clear();
    m_redoPts.clear();
    m_selectedMeasureIndex = -1;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
    setCursor(Qt::CrossCursor);
    // Ustaw początkowy kolor i grubość linii na podstawie ustawień globalnych
    if (m_settings) {
        m_currentColor     = m_settings->defaultMeasureColor;
        m_currentLineWidth = m_settings->lineWidthPx;
    }
}
void CanvasWidget::startMeasureAdvanced(QWidget* parent) {
    AdvancedMeasureDialog dlg(parent, m_settings);
    // Jeśli użytkownik anulował, nie zmieniaj stanu
    if (dlg.exec() != QDialog::Accepted) return;
    // Przygotuj szablon pomiaru zaawansowanego na podstawie dialogu
    m_advTemplate = Measure{};
    m_advTemplate.type = MeasureType::Advanced;
    m_advTemplate.name = dlg.name();
    m_advTemplate.color = dlg.color();
    // Jednostka pomiaru jest globalna, odczytujemy ją z ustawień projektu
    m_advTemplate.unit  = (m_settings->defaultUnit == ProjectSettings::Unit::Cm) ? QStringLiteral("cm") : QStringLiteral("m");
    // Domyślny zapas z dialogu jest podany w bieżącej jednostce; konwertujemy na metry
    double defBuf = dlg.bufferValue();
    if (m_settings->defaultUnit == ProjectSettings::Unit::Cm) defBuf /= 100.0;
    m_advTemplate.bufferDefaultMeters = defBuf;
    // Inicjalizuj lokalny kolor i szerokość linii z wartości dialogu i globalnych ustawień.
    // Kolor pochodzi z dialogu zaawansowanego; grubość linii z ustawień globalnych.
    m_currentColor     = dlg.color();
    if (m_settings) {
        m_currentLineWidth = m_settings->lineWidthPx;
    }
    // Przypisz grubość linii do szablonu, aby zachować ją przy zapisie pomiaru.
    m_advTemplate.lineWidthPx = m_currentLineWidth;
    // Rozpocznij tryb rysowania zaawansowanego
    m_mode = ToolMode::MeasureAdvanced;
    m_currentPts.clear();
    m_selectedMeasureIndex = -1;
    m_selectedTextIndex = -1;
    m_isDraggingSelectedText = false;
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

    if (m_showMeasures) drawMeasures(p);
    drawOverlay(p); // overlay is also in world coords

    p.resetTransform();
    p.setPen(Qt::gray);
    p.drawText(10, height()-10, "PPM: pan, kółko/+/-: zoom; Pomiary: menu; Enter kończy; Backspace cofa; Esc anuluje");
}

void CanvasWidget::drawMeasures(QPainter& p) {
    p.setRenderHint(QPainter::Antialiasing, true);
    // Rysuj każdy pomiar wraz z etykietą.  Jeśli pomiar jest zaznaczony,
    // w następnym kroku zostanie nałożona obwódka.
    for (size_t idx = 0; idx < m_measures.size(); ++idx) {
        const auto &m = m_measures[idx];
        // Pomijamy pomiar, jeśli jest ukryty lub jego warstwa jest wyłączona
        if (!m.visible || !isLayerVisible(m.layer)) continue;
        QPen pen(m.color, m.lineWidthPx);
        pen.setCosmetic(true);
        p.setPen(pen);
        // Rysuj segmenty pomiaru
        if (m.pts.size() >= 2) {
            for (size_t i=1; i<m.pts.size(); ++i) {
                p.drawLine(m.pts[i-1], m.pts[i]);
            }
        }
        // Etykieta długości
        if (!m.pts.empty()) {
            QString label = fmtLenInProjectUnit(m.totalWithBufferMeters);
            QFontMetrics fm(p.font());
            int textW = fm.horizontalAdvance(label) + 10;
            int textH = fm.height() + 4;
            QPointF mid = m.pts[m.pts.size()/2];
            QRectF box(mid + QPointF(8, -textH - 4), QSizeF(textW, textH));
            p.setPen(QPen(Qt::black));
            p.fillRect(box, QColor(255,255,255,200));
            p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }
    // Jeśli jest zaznaczony pomiar, nałóż na niego obwódkę czerwoną przerywaną
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        const auto &mSel = m_measures[m_selectedMeasureIndex];
        // Rysuj obwódkę tylko, jeśli pomiar jest widoczny i jego warstwa jest aktywna
        if (mSel.visible && isLayerVisible(mSel.layer) && mSel.pts.size() >= 2) {
            QPen pen(QColor(255,0,0), mSel.lineWidthPx);
            pen.setStyle(Qt::DashLine);
            pen.setCosmetic(true);
            p.setPen(pen);
            for (size_t i=1; i<mSel.pts.size(); ++i) {
                p.drawLine(mSel.pts[i-1], mSel.pts[i]);
            }
        }
    }
    // Na końcu narysuj wszystkie wstawione teksty
    for (size_t ti = 0; ti < m_textItems.size(); ++ti) {
        const auto &txt = m_textItems[ti];
        if (txt.text.isEmpty()) continue;
        // Jeżeli warstwa tekstu jest wyłączona, pomiń rysowanie
        if (!isLayerVisible(txt.layer)) {
            continue;
        }
        // Przelicz górny lewy narożnik oraz rozmiar prostokąta tekstu w pikselach
        QPointF topLeftScreen = toScreen(txt.boundingRect.topLeft());
        QSizeF sizePx(txt.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                      txt.boundingRect.height() * m_pixelsPerMeter * m_zoom);
        QRectF textBox(topLeftScreen, sizePx);
        // Wyznacz anchor strzałki w pikselach
        QPointF anchorScreen = toScreen(txt.pos);
        // Przygotuj prostokąt dymka: powiększ go o margines.
        // Większy margines poprawia czytelność dymka i oddziela tekst od obramowania.
        const double marginX = 8.0;
        const double marginY = 6.0;
        QRectF bubbleRect = textBox.adjusted(-marginX, -marginY, marginX, marginY);
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
        QRectF textRect = bubbleRect.adjusted(4, 2, -4, -2);
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
        // Przelicz górny lewy narożnik oraz rozmiar prostokąta tekstu w pikselach
        QPointF topLeftScreen = toScreen(txt.boundingRect.topLeft());
        QSizeF sizePx(txt.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                      txt.boundingRect.height() * m_pixelsPerMeter * m_zoom);
        QRectF textBox(topLeftScreen, sizePx);
        // Wyznacz anchor strzałki w pikselach
        QPointF anchorScreen = toScreen(txt.pos);
        // Przygotuj prostokąt dymka: powiększ go o margines.
        const double marginX = 8.0;
        const double marginY = 6.0;
        QRectF bubbleRect = textBox.adjusted(-marginX, -marginY, marginX, marginY);
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
        QRectF textRect = bubbleRect.adjusted(4, 2, -4, -2);
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
    if (m_mode == ToolMode::MeasureLinear || m_mode == ToolMode::MeasurePolyline || m_mode == ToolMode::MeasureAdvanced) {
        if (!m_currentPts.empty()) {
            QPen pen(Qt::DashLine);
            // Używaj lokalnie wybranych parametrów zamiast ustawień globalnych czy z szablonu
            pen.setColor(m_currentColor);
            pen.setWidth(m_currentLineWidth);
            pen.setCosmetic(true);
            p.setPen(pen);
            // existing segments
            for (size_t i=1;i<m_currentPts.size();++i) p.drawLine(m_currentPts[i-1], m_currentPts[i]);
            // live segment to mouse
            double L = polyLengthMeters(m_currentPts);
            if (m_hasMouseWorld) {
                p.drawLine(m_currentPts.back(), m_mouseWorld);
                double dx = m_mouseWorld.x() - m_currentPts.back().x();
                double dy = m_mouseWorld.y() - m_currentPts.back().y();
                L += std::hypot(dx, dy) / m_pixelsPerMeter;
            }
            // Etykieta przy ostatnim punkcie (lub przy kursorku, jeśli jest)
            QPointF at = m_hasMouseWorld ? m_mouseWorld : m_currentPts.back();
            // Tekst do wyświetlenia
            QString text = fmtLenInProjectUnit(L);
            QFontMetrics fm(p.font());
            int textW = fm.horizontalAdvance(text) + 10;
            int textH = fm.height() + 4;
            QRectF box(at + QPointF(8, -textH - 4), QSizeF(textW, textH));
            p.setPen(QPen(Qt::black));
            p.fillRect(box, QColor(255,255,255,200));
            p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    }
}

void CanvasWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton) {
        QPointF pos = toWorld(ev->localPos());
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
    QPointF pos = toWorld(ev->localPos());
    if (m_mode == ToolMode::DefineScale) {
        if (!m_hasFirst) { m_firstPoint = pos; m_hasFirst = true; }
        else { defineScalePromptAndApply(pos); m_hasFirst = false; m_mode = ToolMode::None; }
        return;
    }
    if (m_mode == ToolMode::MeasureLinear) {
        // Dodaj punkt i wyczyść stos przywracania, aby redo odnosił się tylko do bieżącej sekwencji
        m_currentPts.push_back(pos);
        m_redoPts.clear();
        // linia wymaga dwóch punktów; po drugim zakończ pomiar
        if (m_currentPts.size()==2) { finishCurrentMeasure(); }
        update(); return;
    }
    if (m_mode == ToolMode::MeasurePolyline || m_mode == ToolMode::MeasureAdvanced) {
        // Dodaj punkt do polilinii/zaawansowanej i zeruj historię redo
        m_currentPts.push_back(pos);
        m_redoPts.clear();
        update(); return;
    }
    // Tryb zaznaczania: wybierz pomiar lub tekst
    if (m_mode == ToolMode::Select) {
        QPointF wpos = pos;
        // W pierwszej kolejności sprawdź, czy kliknięto w kotwicę (strzałkę) któregoś
        // z dymków.  Jeśli tak, ustaw zaznaczenie na ten dymek i rozpocznij
        // przeciąganie kotwicy.
        int anchorIdx = -1;
        double threshold = 8.0 / (m_pixelsPerMeter * m_zoom);
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
            m_selectedMeasureIndex = -1;
            m_isDraggingSelectedAnchor = true;
            m_isDraggingSelectedText = false;
            update();
            return;
        }
        double marginWorldX = 0.0;
        double marginWorldY = 0.0;
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            marginWorldX = 8.0 / (m_pixelsPerMeter * m_zoom);
            marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
        }
        // Sprawdź, czy kliknięto w uchwyt rozmiaru któregoś dymka
        int resizeIdx = -1;
        ResizeHandle handle = ResizeHandle::None;
        double handleThreshold = 10.0 / (m_pixelsPerMeter * m_zoom);
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            QRectF bubbleRect = ti.boundingRect.adjusted(-marginWorldX, -marginWorldY, marginWorldX, marginWorldY);
            handle = hitResizeHandle(bubbleRect, wpos, handleThreshold);
            if (handle != ResizeHandle::None) {
                resizeIdx = i;
                break;
            }
        }
        if (resizeIdx >= 0) {
            m_selectedTextIndex = resizeIdx;
            m_selectedMeasureIndex = -1;
            m_resizeHandle = handle;
            m_isResizingSelectedBubble = true;
            m_resizeStartRect = m_textItems[resizeIdx].boundingRect.adjusted(-marginWorldX, -marginWorldY, marginWorldX, marginWorldY);
            m_resizeStartPos = wpos;
            m_isDraggingSelectedText = false;
            m_isDraggingSelectedAnchor = false;
            update();
            return;
        }
        // Jeśli kliknięto wewnątrz dymka, rozpocznij przeciąganie całego dymka
        int bubbleIdx = -1;
        for (int i = 0; i < (int)m_textItems.size(); ++i) {
            const auto &ti = m_textItems[i];
            QRectF hitRect = ti.boundingRect.adjusted(-marginWorldX, -marginWorldY, marginWorldX, marginWorldY);
            if (hitRect.contains(wpos)) {
                bubbleIdx = i;
                break;
            }
        }
        if (bubbleIdx >= 0) {
            m_selectedTextIndex = bubbleIdx;
            m_selectedMeasureIndex = -1;
            m_isDraggingSelectedText = true;
            m_isDraggingSelectedAnchor = false;
            // Offset między kliknięciem a lewym górnym rogiem dymka
            m_dragStartOffset = wpos - m_textItems[bubbleIdx].boundingRect.topLeft();
            update();
            return;
        }
        // W przeciwnym razie szukaj najbliższego pomiaru
        int idx = -1;
        double bestDist = 5.0 / m_zoom; // próg w jednostkach world (przybliżony)
        for (size_t i = 0; i < m_measures.size(); ++i) {
            const auto &m = m_measures[i];
            if (!m.visible || m.pts.size() < 2) continue;
            // Sprawdź odległość do każdej krawędzi
            for (size_t j=1; j < m.pts.size(); ++j) {
                QPointF a = m.pts[j-1];
                QPointF b = m.pts[j];
                QPointF ab = b - a;
                double ab2 = ab.x()*ab.x() + ab.y()*ab.y();
                if (ab2 == 0.0) continue;
                double t = ((wpos - a).x()*ab.x() + (wpos - a).y()*ab.y()) / ab2;
                t = std::max(0.0, std::min(1.0, t));
                QPointF proj = a + t * ab;
                double dx = proj.x() - wpos.x();
                double dy = proj.y() - wpos.y();
                double dist = std::sqrt(dx*dx + dy*dy);
                if (dist <= bestDist) {
                    bestDist = dist;
                    idx = (int)i;
                }
            }
        }
        m_selectedMeasureIndex = idx;
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
            // Sprawdź, czy kliknięto w uchwyt rozmiaru dymka
            double marginWorldX = 0.0;
            double marginWorldY = 0.0;
            if (m_pixelsPerMeter * m_zoom != 0.0) {
                marginWorldX = 8.0 / (m_pixelsPerMeter * m_zoom);
                marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
            }
            QRectF bubbleRect = m_tempTextItem.boundingRect.adjusted(-marginWorldX, -marginWorldY, marginWorldX, marginWorldY);
            // Sprawdź, czy kliknięto w uchwyt rozmiaru dymka
            double handleThreshold = 10.0 / (m_pixelsPerMeter * m_zoom);
            ResizeHandle handle = hitResizeHandle(bubbleRect, wpos, handleThreshold);
            if (handle != ResizeHandle::None) {
                m_resizeHandle = handle;
                m_isResizingTempBubble = true;
                m_resizeStartRect = bubbleRect;
                m_resizeStartPos = wpos;
                m_isDraggingTempBubble = false;
                m_isDraggingTempAnchor = false;
                return;
            }
            // Sprawdź, czy kliknięcie znajduje się w pobliżu kotwicy
            double threshold = 8.0 / (m_pixelsPerMeter * m_zoom);
            double dx = wpos.x() - m_tempTextItem.pos.x();
            double dy = wpos.y() - m_tempTextItem.pos.y();
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist <= threshold) {
                // Rozpocznij przeciąganie kotwicy
                m_isDraggingTempAnchor = true;
                m_isDraggingTempBubble = false;
                m_isResizingTempBubble = false;
                return;
            }
            // Sprawdź, czy kliknięto wewnątrz obszaru dymka (boundingRect)
            QRectF hitRect = bubbleRect;
            if (hitRect.contains(wpos)) {
                // Rozpocznij przeciąganie dymka; zapamiętaj offset
                m_isDraggingTempBubble = true;
                m_isDraggingTempAnchor = false;
                m_isResizingTempBubble = false;
                m_tempDragOffset = wpos - m_tempTextItem.boundingRect.topLeft();
                return;
            }
            // Kliknięcie poza dymkiem i kotwicą podczas wstawiania: potraktuj
            // to jako kliknięcie poza dymkiem; nie zmieniaj położenia.
            return;
        }
        // Jeżeli aktywne jest pole edycyjne (użytkownik wpisuje tekst)
        // i kliknięcie znajduje się wewnątrz tego pola, pozwól mu
        // obsłużyć zdarzenie bezpośrednio.
        if (m_textEdit) {
            QRect geom = m_textEdit->geometry();
            if (geom.contains(ev->position().toPoint())) {
                QWidget::mousePressEvent(ev);
                return;
            }
        }
        // Jeśli tymczasowy dymek jeszcze nie istnieje, to klik
        // ustawia pozycję kotwicy i rozpoczyna edycję.  Utwórz
        // m_tempTextItem i pole QTextEdit.
        m_hasTempTextItem = true;
        m_tempTextItem.pos = wpos;
        m_tempTextItem.text.clear();
        m_tempTextItem.color = m_insertTextColor;
        m_tempTextItem.font = m_insertTextFont;
        m_tempTextItem.layer = QStringLiteral("Tekst");
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
        }
        m_isTempBubblePinned = true;
        // Utwórz pole edycyjne na płótnie
        if (m_textEdit) {
            // Nie powinno mieć miejsca, ale dla pewności usuń stare pole
            cancelTextEdit();
        }
        m_textEdit = new QTextEdit(this);
        m_textEdit->setFrameStyle(QFrame::NoFrame);
        m_textEdit->setAcceptRichText(false);
        m_textEdit->installEventFilter(this);
        // Ustaw kolor i czcionkę dla edycji
        QPalette pal = m_textEdit->palette();
        pal.setColor(QPalette::Text, m_tempTextItem.color);
        m_textEdit->setPalette(pal);
        m_textEdit->setStyleSheet(QString("color: %1;").arg(m_tempTextItem.color.name()));
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
        int idx = -1;
        double bestDist = 5.0 / m_zoom;
        for (size_t i = 0; i < m_measures.size(); ++i) {
            const auto &m = m_measures[i];
            if (!m.visible || m.pts.size() < 2) continue;
            for (size_t j=1; j < m.pts.size(); ++j) {
                QPointF a = m.pts[j-1];
                QPointF b = m.pts[j];
                QPointF ab = b - a;
                double ab2 = ab.x()*ab.x() + ab.y()*ab.y();
                if (ab2 == 0.0) continue;
                double t = ((wpos - a).x()*ab.x() + (wpos - a).y()*ab.y()) / ab2;
                t = std::max(0.0, std::min(1.0, t));
                QPointF proj = a + t * ab;
                double dx = proj.x() - wpos.x();
                double dy = proj.y() - wpos.y();
                double dist = std::sqrt(dx*dx + dy*dy);
                if (dist <= bestDist) {
                    bestDist = dist;
                    idx = (int)i;
                }
            }
        }
        if (idx >= 0) {
            m_measures.erase(m_measures.begin() + idx);
            if (m_selectedMeasureIndex == idx) m_selectedMeasureIndex = -1;
            else if (m_selectedMeasureIndex > idx) m_selectedMeasureIndex--;
            update();
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
    if (m_mode == ToolMode::Select) {
        QPointF wpos = toWorld(ev->localPos());
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
        update();
        return;
    }
    if (m_isResizingTempBubble) {
        QPointF wpos = toWorld(ev->localPos());
        QRectF rect = m_resizeStartRect;
        const double minW = 40.0 / (m_pixelsPerMeter * m_zoom);
        const double minH = 20.0 / (m_pixelsPerMeter * m_zoom);
        switch (m_resizeHandle) {
        case ResizeHandle::TopLeft:
            rect.setTopLeft(wpos);
            break;
        case ResizeHandle::TopRight:
            rect.setTopRight(wpos);
            break;
        case ResizeHandle::BottomLeft:
            rect.setBottomLeft(wpos);
            break;
        case ResizeHandle::BottomRight:
            rect.setBottomRight(wpos);
            break;
        default:
            break;
        }
        rect = rect.normalized();
        if (rect.width() < minW) rect.setWidth(minW);
        if (rect.height() < minH) rect.setHeight(minH);
        double marginWorldX = 0.0;
        double marginWorldY = 0.0;
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            marginWorldX = 8.0 / (m_pixelsPerMeter * m_zoom);
            marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
        }
        m_tempTextItem.boundingRect = rect.adjusted(marginWorldX, marginWorldY, -marginWorldX, -marginWorldY);
        m_isTempBubblePinned = true;
        repositionTempTextEdit();
        update();
        return;
    }
    if (m_isResizingSelectedBubble && hasSelectedText()) {
        QPointF wpos = toWorld(ev->localPos());
        QRectF rect = m_resizeStartRect;
        const double minW = 40.0 / (m_pixelsPerMeter * m_zoom);
        const double minH = 20.0 / (m_pixelsPerMeter * m_zoom);
        switch (m_resizeHandle) {
        case ResizeHandle::TopLeft:
            rect.setTopLeft(wpos);
            break;
        case ResizeHandle::TopRight:
            rect.setTopRight(wpos);
            break;
        case ResizeHandle::BottomLeft:
            rect.setBottomLeft(wpos);
            break;
        case ResizeHandle::BottomRight:
            rect.setBottomRight(wpos);
            break;
        default:
            break;
        }
        rect = rect.normalized();
        if (rect.width() < minW) rect.setWidth(minW);
        if (rect.height() < minH) rect.setHeight(minH);
        double marginWorldX = 0.0;
        double marginWorldY = 0.0;
        if (m_pixelsPerMeter * m_zoom != 0.0) {
            marginWorldX = 8.0 / (m_pixelsPerMeter * m_zoom);
            marginWorldY = 6.0 / (m_pixelsPerMeter * m_zoom);
        }
        m_textItems[m_selectedTextIndex].boundingRect = rect.adjusted(marginWorldX, marginWorldY, -marginWorldX, -marginWorldY);
        if (m_textEdit && m_editingTextIndex == m_selectedTextIndex) {
            m_textEdit->move(toScreen(m_textItems[m_selectedTextIndex].boundingRect.topLeft()).toPoint());
            m_textEdit->resize(std::max(40, (int)std::round((rect.width() - marginWorldX * 2) * m_pixelsPerMeter * m_zoom)),
                               std::max(20, (int)std::round((rect.height() - marginWorldY * 2) * m_pixelsPerMeter * m_zoom)));
        }
        update();
        return;
    }
    // Przeciąganie tymczasowego dymka w trybie InsertText
    if (m_mode == ToolMode::InsertText && m_hasTempTextItem) {
        if (m_isDraggingTempBubble) {
            // Przesuwamy boundingRect względem kotwicy; kotwica pozostaje stała
            QPointF wpos = toWorld(ev->localPos());
            QPointF newTopLeft = wpos - m_tempDragOffset;
            QPointF delta = newTopLeft - m_tempTextItem.boundingRect.topLeft();
            m_tempTextItem.boundingRect.translate(delta.x(), delta.y());
            // Przesuń pole edycji
            repositionTempTextEdit();
            update();
            return;
        }
        if (m_isDraggingTempAnchor) {
            // Zmień pozycję kotwicy bez przesuwania dymka
            QPointF wpos = toWorld(ev->localPos());
            m_tempTextItem.pos = wpos;
            update();
            return;
        }
    }
    // Jeśli przeciągamy zaznaczony tekst w trybie zaznaczania lub jego kotwicę
    if (m_mode == ToolMode::Select && hasSelectedText()) {
        if (m_isDraggingSelectedText) {
            // Przeciąganie całego dymka – przesuwamy tylko boundingRect
            QPointF wpos = toWorld(ev->localPos());
            TextItem &ti = m_textItems[m_selectedTextIndex];
            QPointF newTopLeft = wpos - m_dragStartOffset;
            QPointF delta = newTopLeft - ti.boundingRect.topLeft();
            ti.boundingRect.translate(delta.x(), delta.y());
            update();
            return;
        }
        if (m_isDraggingSelectedAnchor) {
            // Przeciąganie kotwicy bez przesuwania dymka
            QPointF wpos = toWorld(ev->localPos());
            TextItem &ti = m_textItems[m_selectedTextIndex];
            ti.pos = wpos;
            update();
            return;
        }
    }
    m_mouseWorld = toWorld(ev->localPos());
    m_hasMouseWorld = true;
    update();
    QWidget::mouseMoveEvent(ev);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton) {
        m_isPanning = false;
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
    // Zastosuj kolor tekstu poprzez paletę i CSS (CSS zapewnia bardziej
    // niezawodne ustawienie koloru w niektórych motywach)
    QPalette pal = m_textEdit->palette();
    pal.setColor(QPalette::Text, m_insertTextColor);
    m_textEdit->setPalette(pal);
    m_textEdit->setStyleSheet(QString("color: %1;").arg(m_insertTextColor.name()));
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
    QSizeF docSize = m_textEdit->document()->size();
    QTextEdit* edit = m_textEdit;
    m_textEdit = nullptr;
    edit->deleteLater();
    // Jeżeli wstawiany jest tymczasowy dymek (m_hasTempTextItem), to
    // commitTempTextItem() obsłuży finalizację.  Wywołaj je i zakończ.
    if (m_hasTempTextItem) {
        if (!text.isEmpty()) {
            // Ustaw treść tymczasowego dymka i finalizuj
            m_tempTextItem.text = text;
        }
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
            double w_m = 0.0;
            double h_m = 0.0;
            if (m_pixelsPerMeter * m_zoom != 0.0) {
                w_m = docSize.width() / (m_pixelsPerMeter * m_zoom);
                h_m = docSize.height() / (m_pixelsPerMeter * m_zoom);
            }
            double x_m = ti.boundingRect.left();
            double y_m = ti.boundingRect.top();
            if (ti.boundingRect.isNull()) {
                x_m = ti.pos.x() - w_m / 2.0;
                y_m = ti.pos.y() - h_m;
            }
            ti.boundingRect = QRectF(x_m, y_m, w_m, h_m);
            // Ustaw zaznaczenie na edytowany element
            m_selectedTextIndex = m_editingTextIndex;
            m_selectedMeasureIndex = -1;
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
    QSizeF docSize;
    if (m_textEdit) {
        docSize = m_textEdit->document()->size();
    } else {
        // Użyj czcionki i treści z m_tempTextItem.  Jeśli tekst jest pusty,
        // przyjmij pojedynczy znak spacji dla obliczenia rozmiaru.
        QString txt = m_tempTextItem.text;
        if (txt.isEmpty()) txt = QStringLiteral(" ");
        QFont f = m_tempTextItem.font;
        QFontMetrics fm(f);
        docSize = QSizeF(fm.horizontalAdvance(txt), fm.height());
    }
    double w_m = 0.0;
    double h_m = 0.0;
    if (m_pixelsPerMeter * m_zoom != 0.0) {
        w_m = docSize.width() / (m_pixelsPerMeter * m_zoom);
        h_m = docSize.height() / (m_pixelsPerMeter * m_zoom);
    }
    if (m_isTempBubblePinned && !m_tempTextItem.boundingRect.isNull()) {
        w_m = std::max(w_m, m_tempTextItem.boundingRect.width());
        h_m = std::max(h_m, m_tempTextItem.boundingRect.height());
    }
    double x_m = m_tempTextItem.boundingRect.left();
    double y_m = m_tempTextItem.boundingRect.top();
    if (!m_isTempBubblePinned || m_tempTextItem.boundingRect.isNull()) {
        switch (m_tempTextItem.anchor) {
        case CalloutAnchor::Bottom:
            x_m = m_tempTextItem.pos.x() - w_m / 2.0;
            y_m = m_tempTextItem.pos.y() - h_m;
            break;
        case CalloutAnchor::Top:
            x_m = m_tempTextItem.pos.x() - w_m / 2.0;
            y_m = m_tempTextItem.pos.y();
            break;
        case CalloutAnchor::Left:
            x_m = m_tempTextItem.pos.x();
            y_m = m_tempTextItem.pos.y() - h_m / 2.0;
            break;
        case CalloutAnchor::Right:
            x_m = m_tempTextItem.pos.x() - w_m;
            y_m = m_tempTextItem.pos.y() - h_m / 2.0;
            break;
        }
    }
    m_tempTextItem.boundingRect = QRectF(x_m, y_m, w_m, h_m);
}

void CanvasWidget::repositionTempTextEdit() {
    if (!m_hasTempTextItem || !m_textEdit) return;
    // Oblicz pozycję lewego górnego rogu prostokąta tekstu w ekranie
    QPointF topLeftScreen = toScreen(m_tempTextItem.boundingRect.topLeft());
    QSizeF sizePx(m_tempTextItem.boundingRect.width() * m_pixelsPerMeter * m_zoom,
                  m_tempTextItem.boundingRect.height() * m_pixelsPerMeter * m_zoom);
    // Ustaw wymiary z niewielkim marginesem, aby tekst nie był ściśnięty
    int width = std::max(40, (int)std::round(sizePx.width() + 4));
    int height = std::max(20, (int)std::round(sizePx.height() + 4));
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
    m_selectedMeasureIndex = -1;
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
    switch (ev->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_mode == ToolMode::MeasurePolyline || m_mode == ToolMode::MeasureAdvanced) {
                finishCurrentMeasure(this);
            } else if (m_mode == ToolMode::InsertText) {
                // W trybie wstawiania tekstu zatwierdź tymczasowy dymek
                // lub edycję istniejącego tekstu.  Jeśli trwa wstawianie
                // tymczasowego dymka, commitTempTextItem() zajmie się
                // usunięciem pola edycyjnego i dodaniem elementu.
                if (m_hasTempTextItem) {
                    commitTempTextItem();
                } else if (m_textEdit) {
                    commitTextEdit();
                }
            }
            break;
        case Qt::Key_Backspace:
            if (!m_currentPts.empty()) { m_currentPts.pop_back(); update(); }
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
            } else {
                // Ogólne anulowanie: wyczyść punkty i zaznaczenia
                m_mode = ToolMode::None;
                m_hasFirst = false;
                m_currentPts.clear();
                m_selectedMeasureIndex = -1;
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
    if (obj == m_textEdit && event->type() == QEvent::KeyPress) {
        auto *keyEv = static_cast<QKeyEvent*>(event);
        if (keyEv->key() == Qt::Key_Return || keyEv->key() == Qt::Key_Enter) {
            if (m_hasTempTextItem) {
                QString txt = m_textEdit->toPlainText().trimmed();
                m_tempTextItem.text = txt;
                updateTempBoundingRect();
                m_isTempBubblePinned = true;
                QTextEdit* edit = m_textEdit;
                m_textEdit = nullptr;
                edit->deleteLater();
                update();
                return true;
            }
            if (m_editingTextIndex >= 0) {
                commitTextEdit();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CanvasWidget::defineScalePromptAndApply(const QPointF& secondPoint) {
    const double dx = secondPoint.x() - m_firstPoint.x();
    const double dy = secondPoint.y() - m_firstPoint.y();
    const double distPx = std::hypot(dx, dy);
    if (distPx <= 0.0) return;

    bool ok=false;
    double defaultVal = (m_settings->defaultUnit == ProjectSettings::Unit::Cm) ? 300.0 : 3.0;
    // Użyj globalnej liczby miejsc po przecinku do podawania skali
    int decimals = m_settings->decimals;
    double val = QInputDialog::getDouble(this, "Skalowanie", "Podaj wartość odległości:", defaultVal, 0.001, 100000.0, decimals, &ok);
    if (!ok) return;
    double meters = (m_settings->defaultUnit == ProjectSettings::Unit::Cm) ? (val / 100.0) : val;

    m_pixelsPerMeter = distPx / meters;
    update();
}

double CanvasWidget::polyLengthMeters(const std::vector<QPointF>& pts) const {
    if (pts.size() < 2) return 0.0;
    double px=0.0;
    for (size_t i=1;i<pts.size();++i) {
        const double dx = pts[i].x() - pts[i-1].x();
        const double dy = pts[i].y() - pts[i-1].y();
        px += std::hypot(dx, dy);
    }
    return px / m_pixelsPerMeter;
}

QString CanvasWidget::fmtLenInProjectUnit(double m) const {
    if (m_settings->defaultUnit == ProjectSettings::Unit::Cm) {
        // Długość w centymetrach; użyj globalnej liczby miejsc po przecinku
        return QString("%1 cm").arg(m*100.0, 0, 'f', m_settings->decimals);
    } else {
        // Długość w metrach; użyj również globalnej liczby miejsc po przecinku
        return QString("%1 m").arg(m, 0, 'f', m_settings->decimals);
    }
}

void CanvasWidget::finishCurrentMeasure(QWidget* parentForAdvanced) {
    if (m_currentPts.size() < 2) { m_currentPts.clear(); m_mode = ToolMode::None; update(); return; }
    Measure mm;
    mm.createdAt = QDateTime::currentDateTime();
    mm.pts = m_currentPts;
    // set type & default unit/color
    if (m_mode == ToolMode::MeasureLinear) {
        mm.type = MeasureType::Linear;
        mm.unit = (m_settings->defaultUnit==ProjectSettings::Unit::Cm) ? QStringLiteral("cm") : QStringLiteral("m");
        // Kolor i grubość linii pobieramy z lokalnych parametrów bieżącego pomiaru
        mm.color = m_currentColor;
        mm.lineWidthPx = m_currentLineWidth;
        // Ustaw wartości zapasów: globalny z ustawień, początkowy i końcowy na 0
        mm.bufferGlobalMeters  = (m_settings->defaultUnit == ProjectSettings::Unit::Cm)
                                   ? (m_settings->defaultBuffer / 100.0)
                                   : m_settings->defaultBuffer;
        mm.bufferDefaultMeters = 0.0;
        mm.bufferFinalMeters   = 0.0;
    } else if (m_mode == ToolMode::MeasurePolyline) {
        mm.type = MeasureType::Polyline;
        mm.unit = (m_settings->defaultUnit==ProjectSettings::Unit::Cm) ? QStringLiteral("cm") : QStringLiteral("m");
        mm.color = m_currentColor;
        mm.lineWidthPx = m_currentLineWidth;
        // Ustaw zapas globalny; brak zapasów początkowych i końcowych
        mm.bufferGlobalMeters  = (m_settings->defaultUnit == ProjectSettings::Unit::Cm)
                                   ? (m_settings->defaultBuffer / 100.0)
                                   : m_settings->defaultBuffer;
        mm.bufferDefaultMeters = 0.0;
        mm.bufferFinalMeters   = 0.0;
    } else { // Advanced
        // Szablon m_advTemplate ma już ustawiony kolor, grubość linii, nazwę,
        // jednostkę oraz zapas początkowy. Skopiuj go jako podstawę nowego pomiaru.
        mm = m_advTemplate;
        mm.createdAt = QDateTime::currentDateTime();
        mm.pts = m_currentPts;
        // Ustaw globalny zapas: jest on nadal pobierany z ustawień, a nie z szablonu
        mm.bufferGlobalMeters = (m_settings->defaultUnit == ProjectSettings::Unit::Cm)
                                  ? (m_settings->defaultBuffer / 100.0)
                                  : m_settings->defaultBuffer;
        if (mm.name.isEmpty()) { /* leave empty; will be set to default below */ }
        // Zapas końcowy: dialog wykorzystuje globalną jednostkę,
        // więc odczytujemy ją z ustawień projektu.
        FinalBufferDialog fd(parentForAdvanced, m_settings->defaultUnit);
        if (fd.exec() == QDialog::Accepted) {
            double val = fd.bufferValue();
            // Wartość z dialogu jest w tej samej jednostce co ustawienie projektu
            // (cm → val jest w cm; m → val jest w m). Konwertujemy na metry.
            if (m_settings->defaultUnit == ProjectSettings::Unit::Cm) val /= 100.0;
            mm.bufferFinalMeters = val;
        } else {
            // Jeśli dialog został anulowany, ustaw zapas końcowy na 0
            mm.bufferFinalMeters = 0.0;
        }
    }
    // assign id exactly once
    mm.id = m_nextId++;
    // default name if empty: "Pomiar <id>"
    if (mm.name.isEmpty()) mm.name = QString("Pomiar %1").arg(mm.id);
    // compute lengths
    mm.lengthMeters = polyLengthMeters(mm.pts);
    // Całkowita długość z zapasami obejmuje długość, globalny zapas,
    // zapas początkowy oraz zapas końcowy.
    mm.totalWithBufferMeters = mm.lengthMeters + mm.bufferGlobalMeters + mm.bufferDefaultMeters + mm.bufferFinalMeters;
    m_measures.push_back(mm);
    m_currentPts.clear();
    m_mode = ToolMode::None;
    update();

    // Powiadom zainteresowane moduły (np. MainWindow), że pomiar został
    // zakończony.  Dzięki temu interfejs może automatycznie schować panel
    // ustawień narzędzia bez konieczności ręcznej obsługi w każdym
    // miejscu, w którym kończy się rysowanie.
    emit measurementFinished();
}

void CanvasWidget::openReportDialog(QWidget* parent)
{
    ReportDialog dlg(parent, m_settings, &m_measures);
    dlg.exec();
    update();
}
