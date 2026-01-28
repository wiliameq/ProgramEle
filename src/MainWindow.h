#pragma once
#include <QMainWindow>
#include <QVector>
#include "Settings.h"
class CanvasWidget;
class QDockWidget;
class QAction;
class QLabel;
class QComboBox;
class QPushButton;
class QToolButton;
class QStackedWidget;
class QSlider;
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
    void onNewProject();
    void onAddBuilding();
    void onRemoveBuilding();
    void onRenameBuilding();
    void onAddFloor();
    void onRemoveFloor();
    void onRenameFloor();
    void onBuildingChanged(int index);
    void onFloorChanged(int index);
    void onApplyBackgroundTo();
    void onClearBackground();
    void onAdjustBackground();
private:
    struct FloorData {
        QString name;
        CanvasWidget* canvas = nullptr;
    };
    struct Building {
        QString name;
        QVector<FloorData> floors;
    };

    CanvasWidget* m_canvas = nullptr;
    ProjectSettings m_settings;
    void createMenus();
    void setProjectActive(bool active);
    void buildProjectPanel();
    void refreshProjectPanel(int preferredBuildingIndex = -1, int preferredFloorIndex = -1);
    QString createProjectTempFile(const QString& projectName,
                                  const QString& address,
                                  const QString& investor);
    void writeProjectTempFile();
    QString nextBuildingName() const;
    QString nextFloorName(const Building& building) const;
    FloorData* currentFloorData();
    const FloorData* currentFloorData() const;
    void applyCanvasForSelection();
    void updateBackgroundControls();
    void ensureFloorCanvas(FloorData& floor);
    void removeFloorCanvas(FloorData& floor);
    bool hasOtherFloors() const;
    void showScaleControls();
    void showBackgroundAdjustControls();
    // Puste panele boczne (lewy/prawy)
    class QDockWidget* m_leftDock = nullptr;
    class QDockWidget* m_rightDock = nullptr;
    // Panel dolny z ustawieniami aktualnego narzędzia
    class ToolSettingsWidget* m_settingsDock = nullptr;
    QStackedWidget* m_canvasStack = nullptr;

    QAction* m_newProjectAction = nullptr;
    QAction* m_reportAction = nullptr;
    QAction* m_measureLinearAction = nullptr;
    QAction* m_measurePolylineAction = nullptr;
    QAction* m_measureAdvancedAction = nullptr;
    QAction* m_toggleMeasuresLayerAction = nullptr;

    QLabel* m_projectNameLabel = nullptr;
    QWidget* m_projectControls = nullptr;
    QComboBox* m_buildingCombo = nullptr;
    QComboBox* m_floorCombo = nullptr;
    QWidget* m_backgroundPanel = nullptr;
    QToolButton* m_backgroundToggle = nullptr;
    QToolButton* m_measurementsToggle = nullptr;
    QWidget* m_measurementsPanel = nullptr;
    QPushButton* m_insertBackgroundBtn = nullptr;
    QPushButton* m_toggleBackgroundBtn = nullptr;
    QPushButton* m_scaleBackgroundBtn = nullptr;
    QPushButton* m_applyBackgroundBtn = nullptr;
    QPushButton* m_clearBackgroundBtn = nullptr;
    QPushButton* m_adjustBackgroundBtn = nullptr;
    QSlider* m_backgroundOpacitySlider = nullptr;
    QPushButton* m_reportBtn = nullptr;
    QPushButton* m_measureLinearBtn = nullptr;
    QPushButton* m_measurePolylineBtn = nullptr;
    QPushButton* m_measureAdvancedBtn = nullptr;
    QPushButton* m_removeBuildingBtn = nullptr;
    QPushButton* m_renameBuildingBtn = nullptr;
    QPushButton* m_removeFloorBtn = nullptr;
    QPushButton* m_renameFloorBtn = nullptr;

    bool m_projectActive = false;
    QString m_projectName;
    QString m_projectAddress;
    QString m_projectInvestor;
    QString m_projectFilePath;
    QVector<Building> m_buildings;
};
