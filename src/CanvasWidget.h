#pragma once
#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QColor>
#include <QFont>
#include <QRectF>
#include <QTextEdit>
#include <QDateTime>
#include <vector>
#include <unordered_map>
#include "MeasurementsTool.h"
#include "Settings.h"

class QWheelEvent;
class QMainWindow;
class ReportDialog;

// Tryb aktywnego narzędzia na płótnie.  Umożliwiamy teraz także zaznaczanie,
// wstawianie tekstu i usuwanie obiektów.  Wartości te kontrolują
// zachowanie obsługi myszy i paska ustawień.
enum class ToolMode {
    None,
    DefineScale,
    Select,      ///< wybieranie istniejących pomiarów na płótnie
    InsertText,  ///< wstawianie tekstu w dowolnym miejscu na płótnie
    Delete       ///< usuwanie pomiarów poprzez kliknięcie
};

/**
 * Kierunek kotwicy dla dymka tekstowego.  Określa, w którą stronę
 * skierowana jest strzałka dymka względem współrzędnej pos zapisanej
 * w elemencie TextItem.  Wartość Bottom oznacza, że dymek znajduje
 * się nad punktem pos, a strzałka wskazuje w dół.  Analogicznie
 * Top oznacza dymek pod punktem pos, Left – dymek po prawej stronie,
 * a Right – dymek po lewej stronie.
 */
enum class CalloutAnchor { Bottom, Top, Left, Right };

// Pomocnicza struktura przechowująca tekst wstawiony na płótnie.  Każdy
// element zawiera pozycję w współrzędnych świata (world) oraz treść
// tekstu.  Tekst nie jest związany z żadnym pomiarem; jest rysowany
// niezależnie w drawTextItems().
struct TextItem {
    QPointF pos;    ///< współrzędne świata, gdzie umieszczono tekst
    QString text;   ///< treść tekstu
    QColor color = Qt::black; ///< kolor tekstu
    QFont font;    ///< czcionka używana do rysowania
    /**
     * Prostokąt ograniczający tekst w jednostkach świata.  Jest
     * obliczany w momencie wstawiania lub edycji tekstu przy użyciu
     * QFontMetrics i przeliczany na jednostki świata (cm).  Używany
     * do detekcji kliknięć i zaznaczania tekstu.
     */
    QRectF boundingRect;

    /**
     * Nazwa warstwy dla elementu komentarza.  Komentarze są traktowane jako
     * odrębna warstwa domyślnie, ale można przypisać je do innej kategorii.
     */
    QString layer = QStringLiteral("Komentarze");

    /**
     * Kierunek (kotwica) strzałki dymka.  Pozwala określić, w którą stronę
 * skierowana jest strzałka w drawTextItems().  Wartość ta jest
     * ustawiana przy dodawaniu tekstu (kopiowana z m_insertTextAnchor)
     * i może być później zmieniana dla zaznaczonego elementu.  Domyślnie
     * strzałka jest skierowana w dół (u dołu dymka).
     */
    CalloutAnchor anchor = CalloutAnchor::Bottom;

    /**
     * Kolor wypełnienia tła dymka.  Domyślnie półprzezroczysta biel,
     * ale użytkownik może go zmienić z panelu ustawień.  Zmienna ta
     * przechowuje pełny kolor (z kanałem alfa), który jest używany do
     * wypełnienia całego kształtu dymka wraz ze strzałką.
     */
    QColor bgColor = QColor(255, 255, 255, 200);

    /**
     * Kolor obramowania dymka.  Domyślnie przyjmuje wartość koloru tekstu,
     * ale można go ustawić niezależnie, aby nadać dymkowi kontrastowy
     * kontur.  Linie obramowania są rysowane w tej barwie.
     */
    QColor borderColor = Qt::black;
};

class AdvancedMeasureDialog;
class FinalBufferDialog;

class CanvasWidget : public QWidget, public ToolHost {
    Q_OBJECT
public:
    enum class ResizeHandle { None, TopLeft, TopRight, BottomLeft, BottomRight };
    explicit CanvasWidget(QWidget* parent, ProjectSettings* settings);

    // Background
    bool loadBackgroundFile(const QString& file);
    bool loadBackgroundImage(const QString& file, QImage& image) const;
    void toggleBackgroundVisibility();
    void setBackgroundVisible(bool visible);
    bool hasBackground() const;
    bool isBackgroundVisible() const;
    void clearBackground();
    void setBackgroundImage(const QImage& image);
    const QImage& backgroundImage() const;

    // View & layers
    void startScaleDefinition(double);
    void confirmScaleStep(QWidget* parent);
    void removeScalePoint();
    bool scaleHasFirstPoint() const;
    bool scaleHasSecondPoint() const;
    int scaleStep() const;
    void toggleMeasuresVisibility();

    // Measurements
    void startMeasureLinear();
    void startMeasurePolyline();
    void startMeasureAdvanced(QWidget* parent);
    void openReportDialog(QWidget* parent);

signals:
    void scaleStateChanged(int step, bool hasFirst, bool hasSecond);
    void scaleFinished();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void commitActiveTextEdit();
    void applyScaleFromPoints(QWidget* parent);
    void emitScaleStateChanged();
    void scaleCanvasContents(double factor);
    // Settings
    ProjectSettings* m_settings = nullptr;

    // Background
    QImage m_bgImage;
    bool m_showBackground = true;

    // Measures layer
    bool m_showMeasures = true;

    // Scale (piksele na centymetr)
    double m_pixelsPerMeter = 100.0;
    enum class ScaleStep { None, FirstPending, SecondPending, Adjusting };
    ScaleStep m_scaleStep = ScaleStep::None;
    QPointF m_scaleFirstPoint;
    QPointF m_scaleSecondPoint;
    bool m_scaleHasFirst = false;
    bool m_scaleHasSecond = false;
    int m_scaleDragPoint = 0;

    // View transform
    double m_zoom = 1.0;
    QPointF m_viewOffset{0,0};
    bool m_isPanning = false;
    QPointF m_lastMouseScreen{0,0};

    // Mouse tracking (world coords)
    QPointF m_mouseWorld{0,0};
    bool m_hasMouseWorld = false;
    bool m_debugDrawTextHandles = true;

    // Mode
    ToolMode m_mode = ToolMode::None;

    // --- Narzędzia ---
    MeasurementsTool m_measurementsTool;
    ToolModule* m_activeTool = nullptr;
    // Index of selected text item in m_textItems; -1 means none selected.
    int m_selectedTextIndex = -1;
    // Lista tekstów wstawionych na płótnie.  Każdy wpis przechowuje
    // pozycję i treść.  Teksty są rysowane w drawTextItems().
    std::vector<TextItem> m_textItems;

    // --- Wstawianie nowego dymka tekstowego w trybie InsertText ---
    // Flagę ustawiamy na true po pierwszym kliknięciu na płótnie w trybie
    // InsertText.  Oznacza ona, że użytkownik wstawił już punkt kotwicy
    // dymka (koniec strzałki) i powinna być wyświetlana tymczasowa
    // konstrukcja dymka.  Podczas wstawiania można przeciągać końcówkę
    // strzałki (anchor) oraz sam dymek niezależnie, aż do zatwierdzenia
    // narzędziem Potwierdź albo klawiszem Enter.
    bool m_hasTempTextItem = false;
    // Tymczasowy element tekstowy tworzony w trybie InsertText.  Po
    // zatwierdzeniu jest kopiowany do m_textItems, a flaga m_hasTempTextItem
    // resetowana.  Ten obiekt przechowuje pozycję kotwicy (pos), treść,
    // boundingRect (współrzędne świata) oraz kolory i czcionkę.
    TextItem m_tempTextItem;
    // Flagi przeciągania dla tymczasowego dymka.  Gdy użytkownik kliknie
    // wewnątrz prostokąta tekstu, m_isDraggingTempBubble jest ustawiana
    // na true i w mouseMoveEvent dymek jest przesuwany niezależnie od
    // kotwicy.  Gdy użytkownik kliknie w pobliżu kotwicy, m_isDraggingTempAnchor
    // jest ustawiana na true i w mouseMoveEvent pozycja kotwicy jest
    // aktualizowana.  Po zwolnieniu przycisku myszy obie flagi są
    // resetowane.
    bool m_isDraggingTempBubble = false;
    bool m_isDraggingTempAnchor = false;
    // Flaga wskazująca, że dymek tymczasowy ma ustaloną pozycję
    // niezależną od kotwicy (np. po utworzeniu lub przesunięciu dymka).
    bool m_isTempBubblePinned = false;
    // Wektor odległości między kliknięciem a początkiem dymka (lewy górny
    // narożnik boundingRect) używany podczas przeciągania dymka.  Dzięki
    // temu dymek nie "skacze" do pozycji myszy.
    QPointF m_tempDragOffset;
    // Flaga przeciągania kotwicy wybranego elementu w trybie Select.
    // Pozwala rozróżnić przeciąganie dymka od przeciągania końca strzałki.
    bool m_isDraggingSelectedAnchor = false;
    bool m_isCommittingText = false;

    // Oblicza boundingRect dla m_tempTextItem na podstawie jego treści,
    // czcionki, pozycji kotwicy oraz kierunku strzałki.  Funkcja ta
    // powinna być wywoływana po każdej zmianie tekstu, czcionki, anchor
    // lub pozycji kotwicy m_tempTextItem.  Wartości szerokości i
    // wysokości prostokąta są obliczane w pikselach przy użyciu
    // QFontMetrics, a następnie przeliczane na jednostki świata.
    void updateTempBoundingRect();

    // Ustawia geometrię pola m_textEdit tak, aby pasowało do dymka
    // tymczasowego (m_tempTextItem).  Wywoływane po zmianie tekstu,
    // czcionki, pozycji lub skali.  Dymek i pole tekstowe są
    // wycentrowane zgodnie z anchor.  Ta funkcja nie zmienia
    // m_tempTextItem.boundingRect – zakłada, że został on wcześniej
    // zaktualizowany.
    void repositionTempTextEdit();

    // Finalizuje tymczasowy dymek tekstowy: kopiuje m_tempTextItem do
    // m_textItems, resetuje flagę m_hasTempTextItem, usuwa pole
    // edycyjne i ustawia tryb na None.  Ustawia także m_selectedTextIndex
    // na nowo dodany element, aby użytkownik mógł od razu go wybrać.
    void commitTempTextItem();

    // Anuluje wstawianie dymka tekstowego.  Usuwa pole edycyjne,
    // resetuje m_hasTempTextItem oraz flagi przeciągania i ustawia
    // tryb na None.  Nie wprowadza żadnych zmian w m_textItems.
    void cancelTempTextItem();
    // Przechowuje treść wstawianego tekstu w trybie InsertText, zanim
    // użytkownik kliknie na płótnie, aby określić pozycję.  Po wstawieniu
    // tekstu pole jest czyszczone, a tryb powraca do None.
    QString m_pendingText;

    // Pozycja świata, w której ma zostać wstawiony tekst (dla narzędzia
    // InsertText).  Pole m_hasTextInsertPos określa, czy pozycja została
    // już wybrana.  Użytkownik najpierw klika na płótnie, a następnie
    // potwierdza wstawienie tekstu.  Po wstawieniu te pola są czyszczone.
    QPointF m_textInsertPos;
    bool m_hasTextInsertPos = false;

    // Kolor i czcionka używane do wstawiania tekstu (dla narzędzia
    // InsertText).  Wartości te mogą być ustawiane z panelu ustawień.
    QColor m_insertTextColor = Qt::black;
    QFont m_insertTextFont;

    // Kolory domyślne dla nowo wstawianych dymków.  m_insertBubbleFillColor
    // określa kolor wypełnienia tła (z kanałem alfa), a
    // m_insertBubbleBorderColor – kolor obramowania.  Są one używane
    // w insertPendingText do inicjalizacji pól bgColor i borderColor
    // w strukturze TextItem.
    QColor m_insertBubbleFillColor = QColor(255, 255, 255, 200);
    QColor m_insertBubbleBorderColor = Qt::black;

    // --- Edycja tekstu w stylu Paint ---
    // Wskaźnik do aktywnego pola edycyjnego QTextEdit, które pojawia się
    // bezpośrednio na płótnie, gdy użytkownik wstawia lub edytuje tekst.
    // Po zatwierdzeniu lub anulowaniu edycji pole jest usuwane.
    QTextEdit* m_textEdit = nullptr;
    ResizeHandle m_resizeHandle = ResizeHandle::None;
    bool m_isResizingTempBubble = false;
    bool m_isResizingSelectedBubble = false;
    QRectF m_resizeStartRect;
    QPointF m_resizeStartPos;
    // Indeks edytowanego tekstu w m_textItems.  Wartość -1 oznacza
    // wstawianie nowego tekstu.
    int m_editingTextIndex = -1;

    // --- Obsługa przeciągania zaznaczonego tekstu ---
    // Flaga wskazująca, że trwa przeciąganie zaznaczonego elementu tekstowego.
    bool m_isDraggingSelectedText = false;
    // Wektor offsetu między pozycją świata kliknięcia a górnym lewym rogiem
    // zaznaczonego tekstu. Używany przy przesuwaniu tekstu myszą.
    QPointF m_dragStartOffset;

    // Mapa nazw warstw na flagę widoczności.  Pozwala na włączanie i
    // wyłączanie całych kategorii obiektów (np. "Ściany", "Drzwi")
    // poprzez kliknięcie w panelu Projekt.  Wartości true oznaczają,
    // że elementy z danej warstwy będą rysowane; false ukrywa warstwę.
    std::unordered_map<QString, bool> m_layerVisibility;

    // Domyślny kierunek strzałki dla nowych tekstów.  Wartość ta jest
    // używana przy wstawianiu tekstu (przez insertPendingText i commitTextEdit).
    CalloutAnchor m_insertTextAnchor = CalloutAnchor::Bottom;

    // Helpers

public:
    /**
     * Przełącza widoczność wskazanej warstwy.  Jeśli warstwa nie istnieje,
     * zostanie ona dodana do mapy i ustawiona jako widoczna.  Po zmianie
     * odświeża płótno.
     */
    void toggleLayerVisibility(const QString& layer);

    /**
     * Zwraca, czy dana warstwa jest aktualnie widoczna.  Jeśli warstwa nie
     * występuje w mapie, domyślnie uważana jest za widoczną (zwraca true).
     */
    QPointF toWorld(const QPointF& screen) const override;
    QPointF toScreen(const QPointF& world) const override;
    double zoom() const override { return m_zoom; }
    double pixelsPerMeter() const override { return m_pixelsPerMeter; }
    ProjectSettings* settings() const override { return m_settings; }
    bool isLayerVisible(const QString& layer) const override;
    void requestUpdate() override { update(); }
    void drawOverlay(QPainter& p);
    void drawTextItems(QPainter& p);

    // --- Zaznaczanie i manipulacja tekstem ---
public:
    /**
     * Zwraca true, jeśli zaznaczony jest jakiś element tekstowy.
     */
    bool hasSelectedText() const { return m_selectedTextIndex >= 0 && m_selectedTextIndex < (int)m_textItems.size(); }
    /**
     * Zwraca tekst zaznaczonego elementu lub pusty string, jeśli brak.
     */
    QString selectedText() const {
        if (hasSelectedText()) return m_textItems[m_selectedTextIndex].text;
        return QString();
    }
    /**
     * Zwraca kolor zaznaczonego elementu tekstowego lub domyślny kolor wstawiania.
     */
    QColor selectedTextColor() const {
        if (hasSelectedText()) return m_textItems[m_selectedTextIndex].color;
        return m_insertTextColor;
    }

    /**
     * Zwraca domyślny kolor tekstu dla nowo wstawianych elementów.
     */
    QColor insertTextColor() const { return m_insertTextColor; }
    /**
     * Zwraca domyślną czcionkę dla nowo wstawianych elementów tekstowych.
     */
    QFont insertTextFont() const { return m_insertTextFont; }
    /**
     * Zwraca czcionkę zaznaczonego elementu tekstowego lub domyślną czcionkę wstawiania.
     */
    QFont selectedTextFont() const {
        if (hasSelectedText()) return m_textItems[m_selectedTextIndex].font;
        return m_insertTextFont;
    }
    /**
     * Ustawia kolor zaznaczonego tekstu. Jeśli brak zaznaczenia, nic nie robi.
     */
    void setSelectedTextColor(const QColor &c);
    /**
     * Ustawia czcionkę zaznaczonego tekstu. Jeśli brak zaznaczenia, nic nie robi.
     */
    void setSelectedTextFont(const QFont &f);
    /**
     * Aktualizuje zawartość, kolor oraz czcionkę zaznaczonego tekstu. Wylicza na nowo
     * boundingRect na podstawie podanej czcionki i treści. Jeśli brak zaznaczenia,
     * funkcja nic nie robi.
     */
    void updateSelectedText(const QString &text, const QColor &color, const QFont &font);

    /**
     * Zwraca kolor wypełnienia dymka dla nowo wstawianych tekstów.
     */
    QColor insertTextBgColor() const { return m_insertBubbleFillColor; }
    /**
     * Zwraca kolor obramowania dymka dla nowo wstawianych tekstów.
     */
    QColor insertTextBorderColor() const { return m_insertBubbleBorderColor; }
    /**
     * Ustawia kolor wypełnienia dymka dla nowo wstawianych tekstów.
     */
    void setInsertTextBgColor(const QColor& c) {
        // Aktualizuj domyślny kolor tła dla nowych dymków
        m_insertBubbleFillColor = c;
        // Jeśli trwa wstawianie tymczasowego dymka, zmień jego kolor
        if (m_hasTempTextItem) {
            m_tempTextItem.bgColor = c;
            // Odśwież widok
            update();
        }
    }
    /**
     * Ustawia kolor obramowania dymka dla nowo wstawianych tekstów.
     */
    void setInsertTextBorderColor(const QColor& c) {
        m_insertBubbleBorderColor = c;
        if (m_hasTempTextItem) {
            m_tempTextItem.borderColor = c;
            update();
        }
    }
    /**
     * Zwraca kolor wypełnienia dymka zaznaczonego tekstu albo domyślny kolor
     * wstawiania, jeśli brak zaznaczenia.
     */
    QColor selectedTextBgColor() const {
        if (hasSelectedText()) return m_textItems[m_selectedTextIndex].bgColor;
        return m_insertBubbleFillColor;
    }
    /**
     * Zwraca kolor obramowania dymka zaznaczonego tekstu albo domyślny
     * kolor wstawiania, jeśli brak zaznaczenia.
     */
    QColor selectedTextBorderColor() const {
        if (hasSelectedText()) return m_textItems[m_selectedTextIndex].borderColor;
        return m_insertBubbleBorderColor;
    }
    /**
     * Ustawia kolor wypełnienia dymka zaznaczonego tekstu.
     */
    void setSelectedTextBgColor(const QColor &c);
    /**
     * Ustawia kolor obramowania dymka zaznaczonego tekstu.
     */
    void setSelectedTextBorderColor(const QColor &c);

    /**
     * Ustawia kierunek strzałki dla tekstu wstawianego w przyszłości.
     * Wartość ta jest kopiowana do każdego nowego elementu TextItem.
     */
    void setInsertTextAnchor(CalloutAnchor a) {
        m_insertTextAnchor = a;
        if (m_hasTempTextItem) {
            // Zmień anchor tymczasowego dymka i przelicz boundingRect
            m_tempTextItem.anchor = a;
            updateTempBoundingRect();
            repositionTempTextEdit();
            update();
        }
    }
    /**
     * Zwraca aktualnie ustawiony domyślny kierunek strzałki dla nowych tekstów.
     */
    CalloutAnchor insertTextAnchor() const { return m_insertTextAnchor; }
    /**
     * Zwraca kierunek strzałki zaznaczonego elementu tekstowego.  Jeśli
     * brak zaznaczenia, zwraca domyślny kierunek dla nowych tekstów.
     */
    CalloutAnchor selectedTextAnchor() const {
        if (hasSelectedText()) return m_textItems[m_selectedTextIndex].anchor;
        return m_insertTextAnchor;
    }
    /**
     * Ustawia kierunek strzałki dla zaznaczonego elementu tekstowego. Jeśli
     * brak zaznaczenia, funkcja nie robi nic.  Po zmianie aktualizuje
     * boundingRect elementu tak, aby odpowiadał nowej orientacji.
     */
    void setSelectedTextAnchor(CalloutAnchor a);
    /**
     * Usuwa zaznaczony element tekstowy. Ustawia m_selectedTextIndex na -1.
     */
    void deleteSelectedText();
    /**
     * Rozpoczyna edycję istniejącego tekstu w trybie InsertText.  Używa
     * wewnętrznego pola m_editingTextIndex do oznaczenia edytowanego elementu.
     */
    void startEditExistingText(int index);

public:
    // --- operacje na bieżącym pomiarze ---
    /**
     * Anuluje aktualnie rysowany pomiar. Czyści listę punktów i przywraca tryb "None".
     */
    void cancelCurrentMeasure();
    /**
     * Cofnięcie ostatnio dodanego punktu w bieżącym pomiarze. Cofnięty punkt jest
     * zapisywany w buforze modułu pomiarów, aby umożliwić późniejsze przywrócenie.
     */
    void undoCurrentMeasure();
    /**
     * Przywraca ostatnio cofnięty punkt w module pomiarów.
     */
    void redoCurrentMeasure();
    /**
     * Zatwierdza bieżący pomiar. Wywołuje finishCurrentMeasure() z przekazanym
     * rodzicem w celu obsługi dialogu zapasu końcowego (dla pomiaru zaawansowanego).
     */
    void confirmCurrentMeasure(QWidget* parentForAdvanced = nullptr);
    /**
     * Aktualizuje kolor wszystkich istniejących pomiarów na bieżący kolor
     * ustawiony w ProjectSettings. Funkcja ta nie zmienia koloru pomiarów
     * aktualnie rysowanych pomiarów.
     */
    void updateAllMeasureColors();
    /**
     * Zwraca true, jeśli istnieje co najmniej jeden zapisany pomiar.
     */
    bool hasAnyMeasure() const { return m_measurementsTool.hasAnyMeasure(); }

    /**
     * Zwraca aktualny kolor ustawiony dla bieżącego pomiaru.  Wartość ta jest
     * kopiowana z ustawień globalnych na początku pomiaru lub ustawiana
     * poprzez pasek ustawień. Nie wpływa na kolor domyślny dla kolejnych
     * pomiarów.
     */
    QColor currentColor() const { return m_measurementsTool.currentColor(); }

    /**
     * Zwraca aktualną grubość linii dla bieżącego pomiaru. Jest ona
     * inicjowana z ustawień globalnych i modyfikowana z paska ustawień,
     * bez wpływu na globalne ustawienia.
     */
    int currentLineWidth() const { return m_measurementsTool.currentLineWidth(); }

    /**
     * Ustawia kolor aktualnego pomiaru. Jeśli w trybie zaawansowanym,
     * aktualizuje także kolor w module pomiarów, aby zachować spójność.
     */
    void setCurrentColor(const QColor& c);

    /**
     * Ustawia grubość linii dla bieżącego pomiaru. Nie modyfikuje
     * ustawień globalnych.
     */
    void setCurrentLineWidth(int w);

    /**
     * Aktualizuje grubość linii wszystkich istniejących pomiarów na wartość
     * globalną zdefiniowaną w ustawieniach projektu.  Funkcja ta nie zmienia
     * bieżąco rysowanego pomiaru.  Po aktualizacji
     * konieczne jest odświeżenie widoku (update()).
     */
    void updateAllMeasureLineWidths();

    /**
     * Rozpoczyna tryb zaznaczania.  Ustawia m_mode na Select i czyści
     * bieżące punkty pomiaru.  Po kliknięciu użytkownik może wybrać
     * istniejący pomiar.  Zaznaczony pomiar jest utrzymywany przez
     * aktywny moduł pomiarów.  Wywołanie tej funkcji nie zmienia
     * domyślnych ustawień globalnych.
     */
    void startSelect();

    /**
     * Rozpoczyna tryb wstawiania tekstu.  Wyświetla dialog z prośbą o tekst
     * (jeśli parent nie jest nullptr) i, jeśli użytkownik poda treść,
     * zapamiętuje ją w m_pendingText.  Kolejny klik myszy na płótnie
     * spowoduje wstawienie tekstu w wybranym miejscu i zakończenie trybu.
     * Jeśli użytkownik anuluje dialog, funkcja powraca bez zmiany trybu.
     */
    void startInsertText(QWidget* parent);

    /**
     * Rozpoczyna tryb usuwania.  Po wejściu w ten tryb kliknięcie na
     * istniejący pomiar spowoduje jego usunięcie.  Po usunięciu pozostaje
     * w trybie Delete aż do anulowania (Esc) lub zakończenia.  Funkcja
     * czyści bieżące punkty pomiaru.
     */
    void startDelete();

    /**
     * Ustawia kolor wybranego pomiaru.  Jeśli
     * żaden pomiar nie jest zaznaczony, funkcja nie robi nic.  Po zmianie
     * kolor jest odzwierciedlany w widoku i eksporcie.
     */
    void setSelectedMeasureColor(const QColor &c);

    /**
     * Ustawia grubość linii zaznaczonego pomiaru.  Zakres grubości jest
     * ograniczony do [1, 8].  Jeśli żaden pomiar nie jest zaznaczony,
     * funkcja nie robi nic.
     */
    void setSelectedMeasureLineWidth(int w);

    /**
     * Usuwa wybrany pomiar z listy.  Po usunięciu tryb pozostaje
     * w Delete lub Select.  Jeśli żaden pomiar nie jest zaznaczony, funkcja
     * nie robi nic.
     */
    void deleteSelectedMeasure();

    /**
     * Zwraca kolor zaznaczonego pomiaru.  Jeśli brak zaznaczenia,
     * zwraca domyślny kolor bieżącego pomiaru (m_currentColor).
     */
    QColor selectedMeasureColor() const { return m_measurementsTool.selectedMeasureColor(); }

    /**
     * Zwraca grubość linii zaznaczonego pomiaru.  Jeśli brak zaznaczenia,
     * zwraca domyślną grubość bieżącego pomiaru.
     */
    int selectedMeasureLineWidth() const { return m_measurementsTool.selectedMeasureLineWidth(); }

    /**
     * Wstawia wstawiony tekst (zebrany w m_pendingText) w pozycji
     * określonej przez m_textInsertPos.  Używa m_insertTextColor i
     * m_insertTextFont do określenia stylu.  Po wstawieniu czyści pola
     * m_hasTextInsertPos i m_pendingText oraz ustawia tryb na None.
     */
    void insertPendingText(const QString& text);

    // --- Edycja tekstu w stylu Paint ---
    /**
     * Rozpoczyna edycję tekstu w podanej pozycji świata i ekranu.
     * Tworzy pole QTextEdit, w którym użytkownik może wpisać treść
     * bezpośrednio na płótnie.  Kolor i czcionka są ustawiane z
     * m_insertTextColor i m_insertTextFont.  Jeśli trwa edycja innego
     * tekstu, jest ona najpierw zatwierdzana.
     */
    void startTextEdit(const QPointF &worldPos, const QPointF &screenPos);
    /**
     * Zatwierdza aktualnie edytowany tekst (jeśli m_textEdit nie jest
     * nullptr).  Aktualizuje lub dodaje wpis w m_textItems i usuwa
     * pole edycyjne.
     */
    void commitTextEdit();
    /**
     * Anuluje edycję tekstu bez zapisywania zmian.  Czyści pole
     * QTextEdit i przywraca tryb None.
     */
    void cancelTextEdit();

    /**
     * Ustawia kolor używany przy wstawianiu tekstu.  Wartość ta jest
     * stosowana do m_insertTextColor i używana przy wstawianiu
     * pending textu.
     */
    void setInsertTextColor(const QColor& c) {
        // Ustaw domyślny kolor tekstu
        m_insertTextColor = c;
        // Jeśli trwa wstawianie tymczasowego dymka, aktualizuj kolor tekstu
        if (m_hasTempTextItem) {
            m_tempTextItem.color = c;
            // Zaktualizuj kolor tekstu w edytorze, jeśli istnieje
            if (m_textEdit) {
                QPalette pal = m_textEdit->palette();
                pal.setColor(QPalette::Text, c);
                pal.setColor(QPalette::Base, Qt::transparent);
                m_textEdit->setPalette(pal);
                m_textEdit->setStyleSheet(QString("color: %1; background: transparent;").arg(c.name()));
            }
            update();
        }
    }

    /**
     * Ustawia czcionkę używaną przy wstawianiu tekstu.  Wartość ta
     * jest używana podczas rysowania nowych elementów tekstowych.
     */
    void setInsertTextFont(const QFont& f) {
        // Ustaw domyślną czcionkę
        m_insertTextFont = f;
        // Jeśli wstawiany jest tymczasowy dymek, zmień jego czcionkę
        if (m_hasTempTextItem) {
            m_tempTextItem.font = f;
            // Uaktualnij boundingRect w oparciu o nową czcionkę
            updateTempBoundingRect();
            repositionTempTextEdit();
            // Ustaw czcionkę w edytorze tekstowym
            if (m_textEdit) {
                m_textEdit->setFont(f);
            }
            update();
        }
    }

signals:
    /**
     * Emitowany, gdy bieżący pomiar zostanie zakończony (zatwierdzony albo
     * zakończony automatycznie).  Służy do powiadamiania innych modułów,
     * że tryb rysowania został zakończony i można np. schować panel
     * ustawień narzędzia.
     */
    void measurementFinished();
};
