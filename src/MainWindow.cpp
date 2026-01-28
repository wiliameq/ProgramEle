#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ToolSettingsWidget.h"
#include "Dialogs.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <QStackedWidget>
#include <QApplication>
#include <QKeyEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTreeWidget>
#include <QShortcut>
#include <QSlider>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QInputDialog>
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_canvasStack = new QStackedWidget(this);
    setCentralWidget(m_canvasStack);
    setWindowTitle("ElecCad2D (Qt6) — v5 Measurements");
    resize(1280, 860);

    // Stwórz panele dokowalne (lewy, prawy, dolny)
    m_leftDock = new QDockWidget(tr("Narzędzia"), this);
    m_leftDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    m_leftDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_leftDock->setWidget(new QWidget(m_leftDock));
    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);

    m_rightDock = new QDockWidget(tr("Projekt"), this);
    m_rightDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_rightDock->setWidget(new QWidget(m_rightDock));
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);

    m_settingsDock = new ToolSettingsWidget(this);
    m_settingsDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_settingsDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, m_settingsDock);

    buildProjectPanel();
    createMenus();
    createMeasurementsMenu(m_projectMenuBar);
    setProjectActive(false);
    statusBar()->showMessage("Gotowy");
}
void MainWindow::createMenus() {
    auto fileMenu = menuBar()->addMenu("Plik");
    m_newProjectAction = fileMenu->addAction("Nowy projekt...");
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::onNewProject);
    auto viewMenu = menuBar()->addMenu("Widok");
    m_toggleMeasuresLayerAction = viewMenu->addAction("Warstwy → Pomiary");
    m_toggleMeasuresLayerAction->setCheckable(true);
    m_toggleMeasuresLayerAction->setChecked(true);
    connect(m_toggleMeasuresLayerAction, &QAction::toggled, this, &MainWindow::onToggleMeasuresLayer);
}
void MainWindow::createMeasurementsMenu(QMenuBar* targetMenuBar) {
    if (!targetMenuBar) {
        return;
    }
    auto measMenu = targetMenuBar->addMenu("Pomiary");
    m_reportAction = measMenu->addAction("Raport...");
    connect(m_reportAction, &QAction::triggered, this, &MainWindow::onReport);
    m_measureLinearAction = measMenu->addAction("Pomiar liniowy");
    connect(m_measureLinearAction, &QAction::triggered, this, &MainWindow::onMeasureLinear);
    m_measurePolylineAction = measMenu->addAction("Pomiar wieloliniowy (polilinia)");
    connect(m_measurePolylineAction, &QAction::triggered, this, &MainWindow::onMeasurePolyline);
    m_measureAdvancedAction = measMenu->addAction("Pomiar zaawansowany...");
    connect(m_measureAdvancedAction, &QAction::triggered, this, &MainWindow::onMeasureAdvanced);
}
void MainWindow::onOpenBackground() {
    if (!m_canvas) {
        return;
    }
    QString fn = QFileDialog::getOpenFileName(this, "Wybierz tło", QString(), "Rysunki (*.png *.jpg *.jpeg *.bmp *.pdf)");
    if (fn.isEmpty()) {
        return;
    }
    if (!m_canvas->loadBackgroundFile(fn)) {
        QMessageBox::warning(this,
                             QString::fromUtf8("Błąd wczytania tła"),
                             QString::fromUtf8("Nie udało się wczytać wybranego pliku tła."));
        return;
    }
    updateBackgroundControls();
}
void MainWindow::onToggleBackground() {
    if (!m_canvas || !m_canvas->hasBackground()) {
        return;
    }
    m_canvas->toggleBackgroundVisibility();
    updateBackgroundControls();
}
void MainWindow::onSetScale() {
    if (!m_canvas || !m_canvas->hasBackground()) {
        return;
    }
    m_canvas->startScaleDefinition(1.0);
    showScaleControls();
}

void MainWindow::onAdjustBackground() {
    if (!m_canvas || !m_canvas->hasBackground()) {
        return;
    }
    m_canvas->startBackgroundAdjust();
    showBackgroundAdjustControls();
}
void MainWindow::onToggleMeasuresLayer() { m_canvas->toggleMeasuresVisibility(); }
void MainWindow::onReport() { m_canvas->openReportDialog(this); }
void MainWindow::onMeasureLinear() {
    m_canvas->startMeasureLinear();
}
void MainWindow::onMeasurePolyline() {
    m_canvas->startMeasurePolyline();
}
void MainWindow::onMeasureAdvanced() {
    // Po otwarciu dialogu pomiaru zaawansowanego CanvasWidget sam ustawia m_advTemplate.
    m_canvas->startMeasureAdvanced(this);
}

void MainWindow::onNewProject() {
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_buildings.clear();
    Building first;
    first.name = nextBuildingName();
    first.floors.append(FloorData{nextFloorName(first), nullptr});
    m_buildings.push_back(first);
    ensureFloorCanvas(m_buildings[0].floors[0]);

    m_projectName = dialog.projectName();
    m_projectAddress = dialog.projectAddress();
    m_projectInvestor = dialog.investorName();
    m_projectFilePath = createProjectTempFile(m_projectName, m_projectAddress, m_projectInvestor);

    setProjectActive(true);
    refreshProjectPanel();
    statusBar()->showMessage(QString::fromUtf8("Utworzono projekt: %1").arg(m_projectName));
}

void MainWindow::onAddBuilding() {
    Building building;
    building.name = nextBuildingName();
    building.floors.append(FloorData{nextFloorName(building), nullptr});
    m_buildings.push_back(building);
    int buildingIndex = m_buildings.size() - 1;
    ensureFloorCanvas(m_buildings[buildingIndex].floors[0]);
    refreshProjectPanel(buildingIndex, 0);
    writeProjectTempFile();
}

void MainWindow::onRemoveBuilding() {
    int index = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (index < 0 || index >= m_buildings.size()) {
        return;
    }
    if (m_buildings.size() <= 1) {
        return;
    }
    for (auto& floor : m_buildings[index].floors) {
        removeFloorCanvas(floor);
    }
    m_buildings.removeAt(index);
    int lastIndex = static_cast<int>(m_buildings.size()) - 1;
    int nextIndex = index < lastIndex ? index : lastIndex;
    refreshProjectPanel(nextIndex, 0);
    writeProjectTempFile();
}

void MainWindow::onRenameBuilding() {
    int index = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (index < 0 || index >= m_buildings.size()) {
        return;
    }
    const QString currentName = m_buildings[index].name;
    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        QString::fromUtf8("Zmień nazwę budynku"),
        QString::fromUtf8("Nowa nazwa:"),
        QLineEdit::Normal,
        currentName,
        &ok);
    newName = newName.trimmed();
    if (!ok || newName.isEmpty() || newName == currentName) {
        return;
    }
    m_buildings[index].name = newName;
    if (m_buildingCombo) {
        m_buildingCombo->setItemText(index, newName);
    }
    writeProjectTempFile();
}

void MainWindow::onAddFloor() {
    int index = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (index < 0 || index >= m_buildings.size()) {
        return;
    }
    auto& building = m_buildings[index];
    building.floors.append(FloorData{nextFloorName(building), nullptr});
    int floorIndex = building.floors.size() - 1;
    ensureFloorCanvas(building.floors[floorIndex]);
    refreshProjectPanel(index, floorIndex);
    writeProjectTempFile();
}

void MainWindow::onRemoveFloor() {
    int index = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (index < 0 || index >= m_buildings.size()) {
        return;
    }
    auto& building = m_buildings[index];
    int floorIndex = m_floorCombo ? m_floorCombo->currentIndex() : -1;
    if (floorIndex < 0 || floorIndex >= building.floors.size()) {
        return;
    }
    if (building.floors.size() <= 1) {
        return;
    }
    removeFloorCanvas(building.floors[floorIndex]);
    building.floors.removeAt(floorIndex);
    int lastFloorIndex = static_cast<int>(building.floors.size()) - 1;
    int nextFloorIndex = floorIndex < lastFloorIndex ? floorIndex : lastFloorIndex;
    refreshProjectPanel(index, nextFloorIndex);
    writeProjectTempFile();
}

void MainWindow::onRenameFloor() {
    int buildingIndex = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (buildingIndex < 0 || buildingIndex >= m_buildings.size()) {
        return;
    }
    auto& building = m_buildings[buildingIndex];
    int floorIndex = m_floorCombo ? m_floorCombo->currentIndex() : -1;
    if (floorIndex < 0 || floorIndex >= building.floors.size()) {
        return;
    }
    const QString currentName = building.floors[floorIndex].name;
    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        QString::fromUtf8("Zmień nazwę piętra"),
        QString::fromUtf8("Nowa nazwa:"),
        QLineEdit::Normal,
        currentName,
        &ok);
    newName = newName.trimmed();
    if (!ok || newName.isEmpty() || newName == currentName) {
        return;
    }
    building.floors[floorIndex].name = newName;
    if (m_floorCombo) {
        m_floorCombo->setItemText(floorIndex, newName);
    }
    writeProjectTempFile();
}

void MainWindow::onBuildingChanged(int index) {
    if (index < 0 || index >= m_buildings.size()) {
        return;
    }
    m_floorCombo->clear();
    for (const auto& floor : m_buildings[index].floors) {
        m_floorCombo->addItem(floor.name);
    }
    m_floorCombo->setCurrentIndex(0);
    if (m_removeFloorBtn) {
        m_removeFloorBtn->setEnabled(m_buildings[index].floors.size() > 1);
    }
    if (m_renameFloorBtn) {
        m_renameFloorBtn->setEnabled(!m_buildings[index].floors.isEmpty());
    }
    applyCanvasForSelection();
}

void MainWindow::onFloorChanged(int index) {
    Q_UNUSED(index);
    applyCanvasForSelection();
}

void MainWindow::setProjectActive(bool active) {
    m_projectActive = active;
    bool enabled = m_projectActive;
    if (m_reportAction) {
        m_reportAction->setEnabled(enabled);
    }
    if (m_measureLinearAction) {
        m_measureLinearAction->setEnabled(enabled);
    }
    if (m_measurePolylineAction) {
        m_measurePolylineAction->setEnabled(enabled);
    }
    if (m_measureAdvancedAction) {
        m_measureAdvancedAction->setEnabled(enabled);
    }
    if (m_toggleMeasuresLayerAction) {
        m_toggleMeasuresLayerAction->setEnabled(enabled);
    }
    if (m_leftDock) {
        m_leftDock->setEnabled(enabled);
    }
    if (m_settingsDock) {
        m_settingsDock->setEnabled(enabled);
    }
    if (m_rightDock) {
        m_rightDock->setEnabled(enabled);
    }
    if (m_canvas) {
        m_canvas->setEnabled(enabled);
    }
    if (m_canvasStack) {
        m_canvasStack->setEnabled(enabled);
    }
    if (m_projectControls) {
        m_projectControls->setVisible(enabled);
    }
    if (m_projectNameLabel) {
        m_projectNameLabel->setText(enabled
            ? m_projectName
            : QString::fromUtf8("Brak aktywnego projektu"));
    }
}

void MainWindow::buildProjectPanel() {
    auto panel = new QWidget(m_rightDock);
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    m_projectMenuBar = new QMenuBar(panel);
    m_projectMenuBar->setNativeMenuBar(false);
    m_projectMenuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_projectMenuBar);

    m_projectNameLabel = new QLabel(QString::fromUtf8("Brak aktywnego projektu"), panel);
    QFont nameFont = m_projectNameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 2);
    m_projectNameLabel->setFont(nameFont);
    layout->addWidget(m_projectNameLabel);

    m_projectControls = new QWidget(panel);
    auto controlsLayout = new QVBoxLayout(m_projectControls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);

    auto buildingRow = new QHBoxLayout();
    auto buildingLabel = new QLabel(QString::fromUtf8("Budynek"), m_projectControls);
    m_buildingCombo = new QComboBox(m_projectControls);
    auto addBuildingBtn = new QPushButton("+", m_projectControls);
    m_removeBuildingBtn = new QPushButton("-", m_projectControls);
    m_renameBuildingBtn = new QPushButton(QString::fromUtf8("✎"), m_projectControls);
    addBuildingBtn->setFixedWidth(28);
    m_removeBuildingBtn->setFixedWidth(28);
    m_renameBuildingBtn->setFixedWidth(28);
    m_renameBuildingBtn->setToolTip(QString::fromUtf8("Zmień nazwę budynku"));
    buildingRow->addWidget(buildingLabel);
    buildingRow->addWidget(m_buildingCombo, 1);
    buildingRow->addWidget(addBuildingBtn);
    buildingRow->addWidget(m_renameBuildingBtn);
    buildingRow->addWidget(m_removeBuildingBtn);
    controlsLayout->addLayout(buildingRow);

    auto floorRow = new QHBoxLayout();
    auto floorLabel = new QLabel(QString::fromUtf8("Piętro"), m_projectControls);
    m_floorCombo = new QComboBox(m_projectControls);
    auto addFloorBtn = new QPushButton("+", m_projectControls);
    m_removeFloorBtn = new QPushButton("-", m_projectControls);
    m_renameFloorBtn = new QPushButton(QString::fromUtf8("✎"), m_projectControls);
    addFloorBtn->setFixedWidth(28);
    m_removeFloorBtn->setFixedWidth(28);
    m_renameFloorBtn->setFixedWidth(28);
    m_renameFloorBtn->setToolTip(QString::fromUtf8("Zmień nazwę piętra"));
    floorRow->addWidget(floorLabel);
    floorRow->addWidget(m_floorCombo, 1);
    floorRow->addWidget(addFloorBtn);
    floorRow->addWidget(m_renameFloorBtn);
    floorRow->addWidget(m_removeFloorBtn);
    controlsLayout->addLayout(floorRow);

    m_backgroundToggle = new QToolButton(m_projectControls);
    m_backgroundToggle->setText(QString::fromUtf8("Tło"));
    m_backgroundToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_backgroundToggle->setArrowType(Qt::RightArrow);
    m_backgroundToggle->setCheckable(true);
    m_backgroundToggle->setChecked(false);
    m_backgroundToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    controlsLayout->addWidget(m_backgroundToggle);

    m_backgroundPanel = new QWidget(m_projectControls);
    auto backgroundLayout = new QVBoxLayout(m_backgroundPanel);
    backgroundLayout->setContentsMargins(20, 0, 0, 0);
    backgroundLayout->setSpacing(6);
    m_insertBackgroundBtn = new QPushButton(QString::fromUtf8("Wstaw tło"), m_backgroundPanel);
    m_toggleBackgroundBtn = new QPushButton(QString::fromUtf8("Ukryj/Pokaż tło"), m_backgroundPanel);
    m_scaleBackgroundBtn = new QPushButton(QString::fromUtf8("Wyskaluj tło"), m_backgroundPanel);
    m_adjustBackgroundBtn = new QPushButton(QString::fromUtf8("Dopasuj tło"), m_backgroundPanel);
    m_applyBackgroundBtn = new QPushButton(QString::fromUtf8("Zastosuj do..."), m_backgroundPanel);
    m_clearBackgroundBtn = new QPushButton(QString::fromUtf8("Usuń tło"), m_backgroundPanel);
    m_backgroundOpacitySlider = new QSlider(Qt::Horizontal, m_backgroundPanel);
    m_backgroundOpacitySlider->setRange(0, 100);
    m_backgroundOpacitySlider->setValue(100);
    m_backgroundOpacitySlider->setToolTip(QString::fromUtf8("Przezroczystość tła"));
    backgroundLayout->addWidget(m_insertBackgroundBtn);
    backgroundLayout->addWidget(m_toggleBackgroundBtn);
    backgroundLayout->addWidget(m_scaleBackgroundBtn);
    backgroundLayout->addWidget(m_adjustBackgroundBtn);
    backgroundLayout->addWidget(m_backgroundOpacitySlider);
    backgroundLayout->addWidget(m_applyBackgroundBtn);
    backgroundLayout->addWidget(m_clearBackgroundBtn);
    controlsLayout->addWidget(m_backgroundPanel);
    m_backgroundPanel->setVisible(false);


    layout->addWidget(m_projectControls);
    layout->addStretch(1);

    connect(addBuildingBtn, &QPushButton::clicked, this, &MainWindow::onAddBuilding);
    connect(m_renameBuildingBtn, &QPushButton::clicked, this, &MainWindow::onRenameBuilding);
    connect(m_removeBuildingBtn, &QPushButton::clicked, this, &MainWindow::onRemoveBuilding);
    connect(addFloorBtn, &QPushButton::clicked, this, &MainWindow::onAddFloor);
    connect(m_renameFloorBtn, &QPushButton::clicked, this, &MainWindow::onRenameFloor);
    connect(m_removeFloorBtn, &QPushButton::clicked, this, &MainWindow::onRemoveFloor);
    connect(m_buildingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBuildingChanged);
    connect(m_floorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFloorChanged);
    connect(m_backgroundToggle, &QToolButton::toggled, this, [this](bool checked) {
        if (!m_backgroundPanel) {
            return;
        }
        m_backgroundPanel->setVisible(checked);
        m_backgroundToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    });
    connect(m_insertBackgroundBtn, &QPushButton::clicked, this, &MainWindow::onOpenBackground);
    connect(m_toggleBackgroundBtn, &QPushButton::clicked, this, &MainWindow::onToggleBackground);
    connect(m_scaleBackgroundBtn, &QPushButton::clicked, this, &MainWindow::onSetScale);
    connect(m_adjustBackgroundBtn, &QPushButton::clicked, this, &MainWindow::onAdjustBackground);
    connect(m_applyBackgroundBtn, &QPushButton::clicked, this, &MainWindow::onApplyBackgroundTo);
    connect(m_clearBackgroundBtn, &QPushButton::clicked, this, &MainWindow::onClearBackground);
    connect(m_backgroundOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if (!m_canvas) {
            return;
        }
        m_canvas->setBackgroundOpacity(value / 100.0);
    });
    m_rightDock->setWidget(panel);
    updateBackgroundControls();
}

void MainWindow::refreshProjectPanel(int preferredBuildingIndex, int preferredFloorIndex) {
    if (!m_projectActive || !m_buildingCombo || !m_floorCombo) {
        return;
    }
    int currentBuildingIndex = m_buildingCombo->currentIndex();
    int currentFloorIndex = m_floorCombo->currentIndex();
    m_buildingCombo->blockSignals(true);
    m_buildingCombo->clear();
    for (const auto& building : m_buildings) {
        m_buildingCombo->addItem(building.name);
    }
    m_buildingCombo->blockSignals(false);

    int buildingIndex = preferredBuildingIndex >= 0 ? preferredBuildingIndex : currentBuildingIndex;
    if (!m_buildings.isEmpty()) {
        if (buildingIndex < 0) {
            buildingIndex = 0;
        } else if (buildingIndex >= m_buildings.size()) {
            buildingIndex = m_buildings.size() - 1;
        }
    }
    if (buildingIndex >= 0 && buildingIndex < m_buildings.size()) {
        m_buildingCombo->setCurrentIndex(buildingIndex);
    }
    if (buildingIndex >= 0 && buildingIndex < m_buildings.size()) {
        m_floorCombo->clear();
        for (const auto& floor : m_buildings[buildingIndex].floors) {
            m_floorCombo->addItem(floor.name);
        }
        int floorIndex = preferredFloorIndex >= 0 ? preferredFloorIndex : currentFloorIndex;
        if (!m_buildings[buildingIndex].floors.isEmpty()) {
            if (floorIndex < 0) {
                floorIndex = 0;
            } else if (floorIndex >= m_buildings[buildingIndex].floors.size()) {
                floorIndex = m_buildings[buildingIndex].floors.size() - 1;
            }
        }
        if (floorIndex >= 0 && floorIndex < m_buildings[buildingIndex].floors.size()) {
            m_floorCombo->setCurrentIndex(floorIndex);
        }
        if (m_removeFloorBtn) {
            m_removeFloorBtn->setEnabled(m_buildings[buildingIndex].floors.size() > 1);
        }
        if (m_renameFloorBtn) {
            m_renameFloorBtn->setEnabled(!m_buildings[buildingIndex].floors.isEmpty());
        }
    }
    if (m_removeBuildingBtn) {
        m_removeBuildingBtn->setEnabled(m_buildings.size() > 1);
    }
    if (m_renameBuildingBtn) {
        m_renameBuildingBtn->setEnabled(!m_buildings.isEmpty());
    }
    applyCanvasForSelection();
}

QString MainWindow::createProjectTempFile(const QString& projectName,
                                         const QString& address,
                                         const QString& investor) {
    m_projectName = projectName;
    m_projectAddress = address;
    m_projectInvestor = investor;
    QString safeName = projectName;
    safeName.replace(QRegularExpression(QStringLiteral("[^\\w\\d\\- ]")), "_");
    safeName = safeName.trimmed();
    if (safeName.isEmpty()) {
        safeName = QStringLiteral("projekt");
    }
    QString filePath = QDir(QDir::tempPath()).filePath(QString("%1.json").arg(safeName));
    m_projectFilePath = filePath;
    writeProjectTempFile();
    return filePath;
}

void MainWindow::writeProjectTempFile() {
    if (m_projectFilePath.isEmpty()) {
        return;
    }
    QJsonObject root;
    root["name"] = m_projectName;
    root["address"] = m_projectAddress;
    root["investor"] = m_projectInvestor;

    QJsonArray buildingsArray;
    for (const auto& building : m_buildings) {
        QJsonObject buildingObj;
        buildingObj["name"] = building.name;
        QJsonArray floors;
        for (const auto& floor : building.floors) {
            floors.append(floor.name);
        }
        buildingObj["floors"] = floors;
        buildingsArray.append(buildingObj);
    }
    root["buildings"] = buildingsArray;

    QFile file(m_projectFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this,
                             QString::fromUtf8("Błąd zapisu"),
                             QString::fromUtf8("Nie udało się zapisać pliku tymczasowego projektu."));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

QString MainWindow::nextBuildingName() const {
    return QString::fromUtf8("Budynek %1").arg(m_buildings.size() + 1);
}

QString MainWindow::nextFloorName(const Building& building) const {
    return QString::fromUtf8("Piętro %1").arg(building.floors.size() + 1);
}

MainWindow::FloorData* MainWindow::currentFloorData() {
    int buildingIndex = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (buildingIndex < 0 || buildingIndex >= m_buildings.size()) {
        return nullptr;
    }
    int floorIndex = m_floorCombo ? m_floorCombo->currentIndex() : -1;
    if (floorIndex < 0 || floorIndex >= m_buildings[buildingIndex].floors.size()) {
        return nullptr;
    }
    return &m_buildings[buildingIndex].floors[floorIndex];
}

const MainWindow::FloorData* MainWindow::currentFloorData() const {
    int buildingIndex = m_buildingCombo ? m_buildingCombo->currentIndex() : -1;
    if (buildingIndex < 0 || buildingIndex >= m_buildings.size()) {
        return nullptr;
    }
    int floorIndex = m_floorCombo ? m_floorCombo->currentIndex() : -1;
    if (floorIndex < 0 || floorIndex >= m_buildings[buildingIndex].floors.size()) {
        return nullptr;
    }
    return &m_buildings[buildingIndex].floors[floorIndex];
}

void MainWindow::applyCanvasForSelection() {
    auto* floor = currentFloorData();
    if (!floor) {
        m_canvas = nullptr;
        updateBackgroundControls();
        return;
    }
    ensureFloorCanvas(*floor);
    if (m_canvasStack && floor->canvas) {
        m_canvasStack->setCurrentWidget(floor->canvas);
    }
    m_canvas = floor->canvas;
    updateBackgroundControls();
}

void MainWindow::updateBackgroundControls() {
    bool hasBackground = m_canvas && m_canvas->hasBackground();
    if (m_toggleBackgroundBtn) {
        m_toggleBackgroundBtn->setEnabled(hasBackground);
    }
    if (m_scaleBackgroundBtn) {
        m_scaleBackgroundBtn->setEnabled(hasBackground);
    }
    if (m_adjustBackgroundBtn) {
        m_adjustBackgroundBtn->setEnabled(hasBackground);
    }
    if (m_applyBackgroundBtn) {
        m_applyBackgroundBtn->setEnabled(hasBackground && hasOtherFloors());
    }
    if (m_clearBackgroundBtn) {
        m_clearBackgroundBtn->setEnabled(hasBackground);
    }
    if (m_backgroundOpacitySlider) {
        m_backgroundOpacitySlider->setEnabled(hasBackground);
        if (m_canvas) {
            m_backgroundOpacitySlider->blockSignals(true);
            m_backgroundOpacitySlider->setValue(static_cast<int>(m_canvas->backgroundOpacity() * 100.0));
            m_backgroundOpacitySlider->blockSignals(false);
        }
    }
}

void MainWindow::ensureFloorCanvas(FloorData& floor) {
    if (floor.canvas || !m_canvasStack) {
        return;
    }
    floor.canvas = new CanvasWidget(m_canvasStack, &m_settings);
    m_canvasStack->addWidget(floor.canvas);
}

void MainWindow::removeFloorCanvas(FloorData& floor) {
    if (!floor.canvas || !m_canvasStack) {
        return;
    }
    if (floor.canvas == m_canvas) {
        m_canvas = nullptr;
    }
    m_canvasStack->removeWidget(floor.canvas);
    floor.canvas->deleteLater();
    floor.canvas = nullptr;
}

bool MainWindow::hasOtherFloors() const {
    int totalFloors = 0;
    for (const auto& building : m_buildings) {
        totalFloors += building.floors.size();
        if (totalFloors > 1) {
            return true;
        }
    }
    return false;
}

void MainWindow::onApplyBackgroundTo() {
    if (!m_canvas || !m_canvas->hasBackground()) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("Zastosuj tło do"));
    auto* layout = new QVBoxLayout(&dialog);

    auto* warningLabel = new QLabel(QString::fromUtf8("Uwaga: Po zastosowaniu tła konieczne jest ponowne wyskalowanie."), &dialog);
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);

    auto* tree = new QTreeWidget(&dialog);
    tree->setHeaderLabels({QString::fromUtf8("Budynek / Piętro")});
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setUniformRowHeights(true);

    for (int b = 0; b < m_buildings.size(); ++b) {
        const auto& building = m_buildings[b];
        auto* buildingItem = new QTreeWidgetItem(tree);
        buildingItem->setText(0, building.name);
        buildingItem->setFlags(buildingItem->flags() & ~Qt::ItemIsSelectable);
        for (int f = 0; f < building.floors.size(); ++f) {
            const auto& floor = building.floors[f];
            auto* floorItem = new QTreeWidgetItem(buildingItem);
            floorItem->setText(0, floor.name);
            floorItem->setCheckState(0, Qt::Unchecked);
            floorItem->setData(0, Qt::UserRole, b);
            floorItem->setData(0, Qt::UserRole + 1, f);
        }
        buildingItem->setExpanded(true);
    }

    layout->addWidget(tree);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QImage background = m_canvas->backgroundImage();
    if (background.isNull()) {
        return;
    }

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto* buildingItem = tree->topLevelItem(i);
        for (int j = 0; j < buildingItem->childCount(); ++j) {
            auto* floorItem = buildingItem->child(j);
            if (floorItem->checkState(0) != Qt::Checked) {
                continue;
            }
            int buildingIndex = floorItem->data(0, Qt::UserRole).toInt();
            int floorIndex = floorItem->data(0, Qt::UserRole + 1).toInt();
            if (buildingIndex < 0 || buildingIndex >= m_buildings.size()) {
                continue;
            }
            auto& building = m_buildings[buildingIndex];
            if (floorIndex < 0 || floorIndex >= building.floors.size()) {
                continue;
            }
            auto& floor = building.floors[floorIndex];
            ensureFloorCanvas(floor);
            if (floor.canvas) {
                floor.canvas->setBackgroundImage(background);
                floor.canvas->setBackgroundVisible(true);
            }
        }
    }
    updateBackgroundControls();
}

void MainWindow::onClearBackground() {
    if (!m_canvas || !m_canvas->hasBackground()) {
        return;
    }
    m_canvas->clearBackground();
    updateBackgroundControls();
}

void MainWindow::showScaleControls() {
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(8);

    auto confirmBtn = new QPushButton(QString::fromUtf8("Zatwierdź"), panel);
    auto removeBtn = new QPushButton(QString::fromUtf8("Cofnij"), panel);
    auto cancelBtn = new QPushButton(QString::fromUtf8("Anuluj"), panel);
    confirmBtn->setEnabled(false);
    removeBtn->setEnabled(false);
    confirmBtn->setMinimumWidth(0);
    removeBtn->setMinimumWidth(0);
    cancelBtn->setMinimumWidth(0);
    confirmBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    removeBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    cancelBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto statusLabel = new QLabel(QString::fromUtf8("Zaznacz pierwszy punkt"), panel);
    statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    lay->addWidget(statusLabel, 1);
    lay->addWidget(confirmBtn);
    lay->addWidget(removeBtn);
    lay->addWidget(cancelBtn);

    auto updateUi = [this, statusLabel, confirmBtn, removeBtn]() {
        if (!m_canvas) {
            confirmBtn->setEnabled(false);
            removeBtn->setEnabled(false);
            return;
        }
        int step = m_canvas->scaleStep();
        bool hasFirst = m_canvas->scaleHasFirstPoint();
        bool hasSecond = m_canvas->scaleHasSecondPoint();

        if (step == 1) {
            statusLabel->setText(QString::fromUtf8("Zaznacz pierwszy punkt"));
            confirmBtn->setEnabled(hasFirst);
        } else if (step == 2) {
            statusLabel->setText(QString::fromUtf8("Zaznacz drugi punkt"));
            confirmBtn->setEnabled(hasSecond);
        } else if (step == 3) {
            statusLabel->setText(QString::fromUtf8("Dopasuj punkty i zatwierdź"));
            confirmBtn->setEnabled(true);
        } else {
            statusLabel->setText(QString::fromUtf8("Zaznacz punkt"));
            confirmBtn->setEnabled(false);
        }
        removeBtn->setEnabled(hasFirst || hasSecond);
    };

    connect(confirmBtn, &QPushButton::clicked, this, [this]() {
        if (m_canvas) {
            m_canvas->confirmScaleStep(this);
        }
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        if (m_canvas) {
            m_canvas->removeScalePoint();
        }
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        if (m_canvas) {
            QKeyEvent cancelEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(m_canvas, &cancelEvent);
        }
    });
    connect(m_canvas, &CanvasWidget::scaleStateChanged, panel,
            [updateUi](int, bool, bool) { updateUi(); });
    connect(m_canvas, &CanvasWidget::scaleFinished, panel, [this]() {
        m_settingsDock->setSettingsWidget(nullptr);
    });

    updateUi();
    m_settingsDock->setSettingsWidget(panel);
}

void MainWindow::showBackgroundAdjustControls() {
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(8);

    auto moveBtn = new QPushButton(QString::fromUtf8("Przesuń"), panel);
    auto rotateBtn = new QPushButton(QString::fromUtf8("Obróć"), panel);
    auto confirmBtn = new QPushButton(QString::fromUtf8("Potwierdź"), panel);
    auto undoBtn = new QPushButton(QString::fromUtf8("Cofnij"), panel);
    auto closeBtn = new QPushButton(QString::fromUtf8("Zamknij"), panel);

    moveBtn->setCheckable(true);
    rotateBtn->setCheckable(true);
    moveBtn->setChecked(false);
    rotateBtn->setChecked(false);
    lay->addWidget(moveBtn);
    lay->addWidget(rotateBtn);
    lay->addStretch();
    lay->addWidget(confirmBtn);
    lay->addWidget(undoBtn);
    lay->addWidget(closeBtn);

    auto syncButtons = [this, moveBtn, rotateBtn]() {
        if (!m_canvas) {
            return;
        }
        moveBtn->setChecked(m_canvas->isBackgroundMoveMode());
        rotateBtn->setChecked(m_canvas->isBackgroundRotateMode());
    };

    connect(moveBtn, &QPushButton::clicked, this, [this, moveBtn, rotateBtn]() {
        if (!m_canvas) {
            return;
        }
        m_canvas->setBackgroundMoveMode(true);
        m_canvas->setBackgroundRotateMode(false);
        moveBtn->setChecked(true);
        rotateBtn->setChecked(false);
    });
    connect(rotateBtn, &QPushButton::clicked, this, [this, moveBtn, rotateBtn]() {
        if (!m_canvas) {
            return;
        }
        m_canvas->setBackgroundRotateMode(true);
        m_canvas->setBackgroundMoveMode(false);
        moveBtn->setChecked(false);
        rotateBtn->setChecked(true);
    });
    connect(confirmBtn, &QPushButton::clicked, this, [this]() {
        if (!m_canvas) {
            return;
        }
        m_canvas->confirmBackgroundAdjust();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    connect(undoBtn, &QPushButton::clicked, this, [this]() {
        if (!m_canvas) {
            return;
        }
        m_canvas->undoBackgroundAdjust();
    });
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        if (!m_canvas) {
            return;
        }
        m_canvas->cancelBackgroundAdjust();
    });

    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), panel);
    escShortcut->setContext(Qt::ApplicationShortcut);
    connect(escShortcut, &QShortcut::activated, this, [this]() {
        if (!m_canvas) {
            return;
        }
        m_canvas->cancelBackgroundAdjust();
    });

    connect(m_canvas, &CanvasWidget::backgroundAdjustFinished, panel, [this]() {
        m_settingsDock->setSettingsWidget(nullptr);
    });

    syncButtons();
    m_settingsDock->setSettingsWidget(panel);
}
