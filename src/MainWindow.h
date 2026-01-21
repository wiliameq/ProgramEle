#pragma once
#include <QMainWindow>
#include "Settings.h"
class CanvasWidget;
class QDockWidget;
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
    // Puste panele boczne (lewy/prawy)
    class QDockWidget* m_leftDock = nullptr;
    class QDockWidget* m_rightDock = nullptr;
    // Panel dolny z ustawieniami aktualnego narzędzia
    class ToolSettingsWidget* m_settingsDock = nullptr;
};
