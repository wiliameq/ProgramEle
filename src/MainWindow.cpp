#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ToolSettingsWidget.h"
#include "Dialogs.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QDockWidget>
// Dodane nagłówki do widżetów używanych w panelu ustawień pomiarów.
// Bez tych include'ów kompilator MSVC nie zna klas takich jak
// QHBoxLayout, QLabel, QPushButton, QSpinBox czy QColorDialog,
// co prowadziło do błędów kompilacji.
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QInputDialog>
#include <algorithm>
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_canvas = new CanvasWidget(this, &m_settings);
    setCentralWidget(m_canvas);
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

    // Połączenie sygnału z CanvasWidget, które informuje, że rysowanie
    // pomiaru zostało zakończone (np. po wstawieniu drugiego punktu
    // odcinka albo po zatwierdzeniu polilinii/zaawansowanego pomiaru).
    // Gdy to nastąpi, chowamy panel ustawień narzędzia, aby nie pozostał
    // widoczny po zakończeniu rysowania.
    connect(m_canvas, &CanvasWidget::measurementFinished, this, [this](){
        m_settingsDock->setSettingsWidget(nullptr);
    });

    createMenus();
    setProjectActive(false);
    statusBar()->showMessage("Gotowy");
}
void MainWindow::createMenus() {
    auto fileMenu = menuBar()->addMenu("Plik");
    m_newProjectAction = fileMenu->addAction("Nowy projekt...");
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::onNewProject);
    m_openBackgroundAction = fileMenu->addAction("Otwórz tło...");
    connect(m_openBackgroundAction, &QAction::triggered, this, &MainWindow::onOpenBackground);
    auto measMenu = menuBar()->addMenu("Pomiary");
    m_reportAction = measMenu->addAction("Raport...");
    connect(m_reportAction, &QAction::triggered, this, &MainWindow::onReport);
    m_measureLinearAction = measMenu->addAction("Pomiar liniowy");
    connect(m_measureLinearAction, &QAction::triggered, this, &MainWindow::onMeasureLinear);
    m_measurePolylineAction = measMenu->addAction("Pomiar wieloliniowy (polilinia)");
    connect(m_measurePolylineAction, &QAction::triggered, this, &MainWindow::onMeasurePolyline);
    m_measureAdvancedAction = measMenu->addAction("Pomiar zaawansowany...");
    connect(m_measureAdvancedAction, &QAction::triggered, this, &MainWindow::onMeasureAdvanced);
    auto viewMenu = menuBar()->addMenu("Widok");
    m_toggleBackgroundAction = viewMenu->addAction("Pokaż/Ukryj tło (H)");
    connect(m_toggleBackgroundAction, &QAction::triggered, this, &MainWindow::onToggleBackground);
    m_toggleMeasuresLayerAction = viewMenu->addAction("Warstwy → Pomiary");
    m_toggleMeasuresLayerAction->setCheckable(true);
    m_toggleMeasuresLayerAction->setChecked(true);
    connect(m_toggleMeasuresLayerAction, &QAction::toggled, this, &MainWindow::onToggleMeasuresLayer);
}
void MainWindow::onOpenBackground() {
    QString fn = QFileDialog::getOpenFileName(this, "Wybierz tło", QString(), "Rysunki (*.png *.jpg *.jpeg *.bmp *.pdf)");
    if (!fn.isEmpty()) m_canvas->loadBackgroundFile(fn);
}
void MainWindow::onToggleBackground() { m_canvas->toggleBackgroundVisibility(); }
void MainWindow::onSetScale() { m_canvas->startScaleDefinition(1.0); }
void MainWindow::onToggleMeasuresLayer() { m_canvas->toggleMeasuresVisibility(); }
void MainWindow::onReport() { m_canvas->openReportDialog(this); }
void MainWindow::onMeasureLinear() {
    m_canvas->startMeasureLinear();
    // Pokaż panel kontroli bez funkcji cofnij/przywróć (pojedynczy odcinek)
    showMeasurementControls(false);
}
void MainWindow::onMeasurePolyline() {
    m_canvas->startMeasurePolyline();
    // Pokaż panel z cofnięciem/przywróceniem dla polilinii
    showMeasurementControls(true);
}
void MainWindow::onMeasureAdvanced() {
    // Po otwarciu dialogu pomiaru zaawansowanego CanvasWidget sam ustawia m_advTemplate.
    m_canvas->startMeasureAdvanced(this);
    // Pokaż panel z cofnięciem/przywróceniem tylko jeżeli użytkownik nie anulował. Nie mamy
    // bezpośredniego zwrotu z CanvasWidget::startMeasureAdvanced, więc zakładamy, że
    // jeśli CanvasWidget zmieni tryb, panel powinien się pojawić. W najgorszym wypadku
    // panel można zamknąć przyciskiem Anuluj.
    showMeasurementControls(true);
}

void MainWindow::onNewProject() {
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_buildings.clear();
    Building first;
    first.name = nextBuildingName();
    first.floors.append(nextFloorName(first));
    m_buildings.push_back(first);

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
    building.floors.append(nextFloorName(building));
    m_buildings.push_back(building);
    int buildingIndex = m_buildings.size() - 1;
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
    m_buildings.removeAt(index);
    int nextIndex = std::min(index, m_buildings.size() - 1);
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
    building.floors.append(nextFloorName(building));
    int floorIndex = building.floors.size() - 1;
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
    building.floors.removeAt(floorIndex);
    int nextFloorIndex = std::min(floorIndex, building.floors.size() - 1);
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
    const QString currentName = building.floors[floorIndex];
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
    building.floors[floorIndex] = newName;
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
    m_floorCombo->addItems(m_buildings[index].floors);
    m_floorCombo->setCurrentIndex(0);
    if (m_removeFloorBtn) {
        m_removeFloorBtn->setEnabled(m_buildings[index].floors.size() > 1);
    }
    if (m_renameFloorBtn) {
        m_renameFloorBtn->setEnabled(!m_buildings[index].floors.isEmpty());
    }
}

void MainWindow::setProjectActive(bool active) {
    m_projectActive = active;
    bool enabled = m_projectActive;
    if (m_openBackgroundAction) {
        m_openBackgroundAction->setEnabled(enabled);
    }
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
    if (m_toggleBackgroundAction) {
        m_toggleBackgroundAction->setEnabled(enabled);
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

    m_rightDock->setWidget(panel);
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
        m_floorCombo->addItems(m_buildings[buildingIndex].floors);
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
}

QString MainWindow::createProjectTempFile(const QString& projectName,
                                         const QString& address,
                                         const QString& investor) {
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
            floors.append(floor);
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

// --- Pomocnicza metoda ---
// Ustawia panel dolny na tryb rysowania pomiarów. Pozwala wybrać kolor i grubość linii,
// oraz potwierdzić/anulować/cofnąć/przywrócić rysowanie. Parametr withUndoRedo kontroluje
// obecność przycisków cofania/przywracania (dla polilinii i pomiaru zaawansowanego).
void MainWindow::showMeasurementControls(bool withUndoRedo) {
    // Utwórz nowy widżet ustawień narzędzia. Po ustawieniu zostanie on
    // automatycznie zniszczony przez ToolSettingsWidget (przy kolejnej zmianie).
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4,2,4,2);
    lay->setSpacing(8);

    // --- Wybór koloru (tylko bieżący pomiar) ---
    auto colorLabel = new QLabel(QString::fromUtf8("Kolor:"), panel);
    lay->addWidget(colorLabel);
    auto colorBtn = new QPushButton(panel);
    // Funkcja pomocnicza do aktualizacji wyglądu przycisku zgodnie z aktualnym kolorem
    auto updateColorBtn = [this, colorBtn]() {
        QString hex = m_canvas->currentColor().name();
        colorBtn->setStyleSheet(QString("background-color: %1").arg(hex));
        colorBtn->setText(hex);
    };
    updateColorBtn();
    lay->addWidget(colorBtn);
    // Obsługa kliknięcia: zmiana koloru tylko dla bieżącego pomiaru.  Nie modyfikujemy
    // ustawień globalnych i nie pytamy o aktualizację istniejących pomiarów.
    connect(colorBtn, &QPushButton::clicked, this, [this, updateColorBtn, colorBtn]() {
        // Wybór koloru tylko dla bieżącego pomiaru
        QColor chosen = QColorDialog::getColor(m_canvas->currentColor(), this, QString::fromUtf8("Wybierz kolor pomiaru"));
        if (chosen.isValid()) {
            m_canvas->setCurrentColor(chosen);
            updateColorBtn();
        }
    });

    // --- Grubość linii ---
    auto lwLabel = new QLabel(QString::fromUtf8("Grubość:"), panel);
    lay->addWidget(lwLabel);
    auto lwSpin = new QSpinBox(panel);
    lwSpin->setRange(1, 8);
    // Ustaw wartość początkową z bieżącej grubości linii na płótnie
    lwSpin->setValue(m_canvas->currentLineWidth());
    lay->addWidget(lwSpin);
    connect(lwSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        // Aktualizuj tylko bieżący pomiar, bez modyfikacji ustawień globalnych
        m_canvas->setCurrentLineWidth(v);
    });

    lay->addStretch();

    // --- Przyciski cofnięcia / przywrócenia ---
    if (withUndoRedo) {
        auto undoBtn = new QPushButton(QString::fromUtf8("Cofnij"), panel);
        lay->addWidget(undoBtn);
        connect(undoBtn, &QPushButton::clicked, this, [this](){ m_canvas->undoCurrentMeasure(); });
        auto redoBtn = new QPushButton(QString::fromUtf8("Przywróć"), panel);
        lay->addWidget(redoBtn);
        connect(redoBtn, &QPushButton::clicked, this, [this](){ m_canvas->redoCurrentMeasure(); });
    }

    // --- Przyciski potwierdzenia / anulowania ---
    // Przyciski "Potwierdź" i "Anuluj" są potrzebne tylko dla pomiaru wielosegmentowego
    // (polilinia, pomiar zaawansowany).  W przypadku pomiaru liniowego (z jednym segmentem)
    // potwierdzenie nie jest potrzebne, ponieważ pomiar kończy się automatycznie po
    // wstawieniu dwóch punktów.
    if (withUndoRedo) {
        auto confirmBtn = new QPushButton(QString::fromUtf8("Potwierdź"), panel);
        lay->addWidget(confirmBtn);
        connect(confirmBtn, &QPushButton::clicked, this, [this]() {
            // Zakończ bieżący pomiar i schowaj panel ustawień
            m_canvas->confirmCurrentMeasure(this);
            m_settingsDock->setSettingsWidget(nullptr);
        });
    }
    auto cancelBtn = new QPushButton(QString::fromUtf8("Anuluj"), panel);
    lay->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        // Anuluj bieżący pomiar i schowaj panel
        m_canvas->cancelCurrentMeasure();
        m_settingsDock->setSettingsWidget(nullptr);
    });

    panel->setLayout(lay);
    m_settingsDock->setSettingsWidget(panel);
}
