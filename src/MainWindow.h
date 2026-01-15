#pragma once
#include <QMainWindow>
#include "Settings.h"
class CanvasWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ProjectSettings& settings() { return m_settings; }
private slots:
    void onOpenBackground();
    void onToggleBackground();
    void onSetScale();
    void onToggleMeasuresLayer();
    void onReport();
    void onMeasureLinear();
    void onMeasurePolyline();
    void onMeasureAdvanced();
    void onProjectSettings();

    /**
     * Obsługuje wybór narzędzia z drzewa ToolsDockWidget.  W zależności od
     * tekstu narzędzia uruchamia odpowiedni tryb w CanvasWidget i
     * wyświetla powiązany panel ustawień.
     */
    void onToolSelected(const QString& tool);
private:
    CanvasWidget* m_canvas = nullptr;
    ProjectSettings m_settings;
    void createMenus();

    /**
     * Wyświetla w dolnym panelu (ToolSettingsWidget) kontrolki związane z rysowaniem pomiarów.
     *
     * @param withUndoRedo gdy true, do panelu dodawane są przyciski Cofnij/Przywróć,
     *        obsługujące wielosegmentowe rysowanie (polilinia, pomiar zaawansowany). Dla
     *        pomiaru liniowego (jeden odcinek) przekazujemy false – przyciski cofania
     *        nie są potrzebne.
     */
    void showMeasurementControls(bool withUndoRedo);

    /**
     * Wyświetla panel ustawień dla trybu zaznaczania.  Umożliwia zmianę
     * koloru i grubości linii zaznaczonego pomiaru oraz jego usunięcie.
     */
    void showSelectControls();

    /**
     * Wyświetla panel ustawień dla zaznaczonego elementu tekstowego. Umożliwia
     * edycję zawartości tekstu, zmianę koloru i czcionki, a także jego usunięcie.
     */
    void showTextSelectControls();

    /**
     * Wyświetla panel ustawień dla wstawiania tekstu.  Panel zawiera
     * przycisk Anuluj kończący tryb wstawiania.
     */
    void showInsertTextControls();

    /**
     * Wyświetla panel ustawień dla trybu usuwania.  Panel zawiera jedynie
     * przycisk Anuluj, który przywraca tryb do None.
     */
    void showDeleteControls();

    // Panel po lewej z listą narzędzi
    class ToolsDockWidget* m_toolsDock = nullptr;
    // Panel po prawej z wyborem obiektu i drzewem kontekstowym
    class ProjectNavigatorWidget* m_projectDock = nullptr;
    // Panel dolny z ustawieniami aktualnego narzędzia
    class ToolSettingsWidget* m_settingsDock = nullptr;
    // Panel górny (ulubione narzędzia)
    class FavoritesDockWidget* m_favoritesDock = nullptr;
};
