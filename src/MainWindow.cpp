#include "MainWindow.h"
#include "CanvasWidget.h"
#include "Dialogs.h"
// Nowy widżet z listą narzędzi
#include "ToolsDockWidget.h"
// Nowy widżet z wyborem projektu i kontekstem
#include "ProjectNavigatorWidget.h"
// Nowy widżet z ustawieniami narzędzi
#include "ToolSettingsWidget.h"
// Pasek ulubionych narzędzi
#include "FavoritesDockWidget.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
// Dodane nagłówki do widżetów używanych w panelu ustawień pomiarów.
// Bez tych include'ów kompilator MSVC nie zna klas takich jak
// QHBoxLayout, QLabel, QPushButton, QSpinBox, QColorDialog czy QMessageBox,
// co prowadziło do błędów kompilacji.
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QInputDialog>
// Nowe widżety wykorzystywane w panelu wstawiania tekstu
#include <QCheckBox>
#include <QInputDialog>
#include <QFontComboBox>
// Dodane do obsługi rozwijanych list i wariantów
#include <QComboBox>
#include <QVariant>
// std::abs używany do porównywania wartości double
#include <cmath>
// Potrzebne dodatkowe nagłówki dla widżetów i dialogów używanych w dolnym panelu
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QColorDialog>
#include <QMessageBox>
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_canvas = new CanvasWidget(this, &m_settings);
    setCentralWidget(m_canvas);
    setWindowTitle("ElecCad2D (Qt6) — v5 Measurements");
    resize(1280, 860);

    // Stwórz panele dokowalne (lewy, prawy, dolny, górny)
    m_toolsDock = new ToolsDockWidget(this);
    // Tylko możliwość dokowania po lewej i prawo, bez możliwości zamknięcia
    m_toolsDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    m_toolsDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolsDock);

    m_projectDock = new ProjectNavigatorWidget(this);
    m_projectDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_projectDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, m_projectDock);

    m_settingsDock = new ToolSettingsWidget(this);
    m_settingsDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_settingsDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, m_settingsDock);

    m_favoritesDock = new FavoritesDockWidget(this);
    m_favoritesDock->setAllowedAreas(Qt::TopDockWidgetArea);
    // Umożliwiamy przenoszenie i zamykanie panelu ulubionych
    m_favoritesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::TopDockWidgetArea, m_favoritesDock);

    // Ustaw odbiorcę akcji z paska ulubionych tak, aby kliknięcie w akcję
    // wywoływało metodę onToolSelected(QString).  Dzięki temu użytkownik
    // może przeciągać narzędzia z panelu po lewej do ulubionych i
    // natychmiast je wywoływać.
    m_favoritesDock->setDefaultActionReceiver(this, SLOT(onToolSelected(const QString&)));

    // Połączenie sygnału z lewego panelu narzędzi.  W funkcji onToolSelected
    // zinterpretujemy tekst narzędzia i uruchomimy odpowiedni tryb w CanvasWidget.
    connect(m_toolsDock, &ToolsDockWidget::toolSelected,
            this, &MainWindow::onToolSelected);
    connect(m_projectDock, &ProjectNavigatorWidget::currentObjectChanged, this, [this](const QString& obj){
        statusBar()->showMessage(tr("Wybrano obiekt: %1").arg(obj), 3000);
    });
    connect(m_projectDock, &ProjectNavigatorWidget::contextToolSelected, this,
            [this](const QString& category, const QString& item) {
        // Kliknięcie w element w drzewie projektu służy teraz do
        // przełączania widoczności warstw na płótnie.  Wysłane zostają
        // informacje o nazwie kategorii i liścia (warstwy).  Wywołujemy
        // toggleLayerVisibility() na płótnie, aby włączyć/wyłączyć dany
        // element.  Po przełączeniu pokazujemy komunikat w pasku stanu.
        m_canvas->toggleLayerVisibility(item);
        bool vis = m_canvas->isLayerVisible(item);
        if (vis) {
            statusBar()->showMessage(tr("Pokazano warstwę: %1").arg(item), 3000);
        } else {
            statusBar()->showMessage(tr("Ukryto warstwę: %1").arg(item), 3000);
        }
    });

    // Połączenie sygnału z CanvasWidget, które informuje, że rysowanie
    // pomiaru zostało zakończone (np. po wstawieniu drugiego punktu
    // odcinka albo po zatwierdzeniu polilinii/zaawansowanego pomiaru).
    // Gdy to nastąpi, chowamy panel ustawień narzędzia, aby nie pozostał
    // widoczny po zakończeniu rysowania.
    connect(m_canvas, &CanvasWidget::measurementFinished, this, [this](){
        m_settingsDock->setSettingsWidget(nullptr);
    });

    createMenus();
    statusBar()->showMessage("Gotowy");
}
void MainWindow::createMenus() {
    auto fileMenu = menuBar()->addMenu("Plik");
    auto openBg = fileMenu->addAction("Otwórz tło...");
    connect(openBg, &QAction::triggered, this, &MainWindow::onOpenBackground);
    auto projMenu = menuBar()->addMenu("Projekt");
    // Usunięto opcję ustawień z menu Projekt – ustawienia pomiarów są dostępne w menu Pomiary
    auto measMenu = menuBar()->addMenu("Pomiary");
    // W menu pomiarów dodaj opcję otwierającą dialog ustawień pomiarów
    auto measSettings = measMenu->addAction(QString::fromUtf8("Ustawienia..."));
    connect(measSettings, &QAction::triggered, this, &MainWindow::onProjectSettings);
    auto report = measMenu->addAction("Raport...");
    connect(report, &QAction::triggered, this, &MainWindow::onReport);
    auto lin = measMenu->addAction("Pomiar liniowy");
    connect(lin, &QAction::triggered, this, &MainWindow::onMeasureLinear);
    auto poly = measMenu->addAction("Pomiar wieloliniowy (polilinia)");
    connect(poly, &QAction::triggered, this, &MainWindow::onMeasurePolyline);
    auto adv = measMenu->addAction("Pomiar zaawansowany...");
    connect(adv, &QAction::triggered, this, &MainWindow::onMeasureAdvanced);
    auto viewMenu = menuBar()->addMenu("Widok");
    auto vis = viewMenu->addAction("Pokaż/Ukryj tło (H)");
    connect(vis, &QAction::triggered, this, &MainWindow::onToggleBackground);
    auto measLayer = viewMenu->addAction("Warstwy → Pomiary");
    measLayer->setCheckable(true);
    measLayer->setChecked(true);
    connect(measLayer, &QAction::toggled, this, &MainWindow::onToggleMeasuresLayer);
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
void MainWindow::onProjectSettings() {
    // Zapamiętaj aktualne wartości w ustawieniach przed ich modyfikacją
    QColor oldColor   = m_settings.defaultMeasureColor;
    double oldBuf     = m_settings.defaultBuffer;
    int    oldLineW   = m_settings.lineWidthPx;
    ProjectSettingsDialog dlg(this, &m_settings);
    if (dlg.exec() == QDialog::Accepted) {
        // Jeśli kolor został zmieniony i istnieją już pomiary, zapytaj o aktualizację
        if (m_settings.defaultMeasureColor != oldColor && m_canvas->hasAnyMeasure()) {
            if (QMessageBox::question(this,
                                      QString::fromUtf8("Aktualizuj kolor pomiarów"),
                                      QString::fromUtf8("Czy zastosować nowy kolor do wszystkich istniejących pomiarów?")) == QMessageBox::Yes) {
                m_canvas->updateAllMeasureColors();
            }
        }
        // Jeśli domyślny zapas został zmieniony i istnieją pomiary, zapytaj o aktualizację
        if (std::abs(m_settings.defaultBuffer - oldBuf) > 1e-9 && m_canvas->hasAnyMeasure()) {
            if (QMessageBox::question(this,
                                      QString::fromUtf8("Aktualizuj domyślny zapas"),
                                      QString::fromUtf8("Czy zastosować nowy domyślny zapas do wszystkich istniejących pomiarów?"))
                == QMessageBox::Yes) {
                // Aktualizujemy globalny zapas w każdym pomiarze.
                m_canvas->updateAllMeasureGlobalBuffers();
            }
        }
        // Jeśli grubość linii została zmieniona globalnie, zapytaj o aktualizację istniejących pomiarów
        if (m_settings.lineWidthPx != oldLineW && m_canvas->hasAnyMeasure()) {
            if (QMessageBox::question(this,
                                      QString::fromUtf8("Aktualizuj grubość linii"),
                                      QString::fromUtf8("Czy zastosować nową grubość linii do wszystkich istniejących pomiarów?"))
                == QMessageBox::Yes) {
                m_canvas->updateAllMeasureLineWidths();
            }
        }
        // Uaktualnij rysunek oraz komunikat w pasku stanu
        m_canvas->update();
        statusBar()->showMessage(QString::fromUtf8("Zapisano ustawienia pomiarów"), 3000);
    }
}

// Reaguje na kliknięcie elementu drzewa w ToolsDockWidget.  Na podstawie
// tekstu narzędzia uruchamia odpowiedni tryb w CanvasWidget i ustawia
// panel ustawień narzędzia.
void MainWindow::onToolSelected(const QString& tool) {
    // Najpierw pokaż w pasku stanu nazwę narzędzia
    statusBar()->showMessage(tr("Wybrane narzędzie: %1").arg(tool), 3000);
    // Ustal, jakie narzędzie wybrano.  Używamy bezpośredniego porównania
    // z polskimi nazwami kategorii.  Można użyć tr(), ale łańcuch już
    // pochodzi z QTreeWidget i jest przetłumaczony.
    if (tool == QString::fromUtf8("Zaznacz")) {
        m_canvas->startSelect();
        showSelectControls();
    } else if (tool == QString::fromUtf8("Wstaw tekst")) {
        m_canvas->startInsertText(this);
        showInsertTextControls();
    } else if (tool == QString::fromUtf8("Usuń")) {
        m_canvas->startDelete();
        showDeleteControls();
    } else {
        // Inne narzędzia (np. Budynek/Instalacje) są aktualnie nieobsługiwane
        // i nie zmieniają trybu na płótnie.  Można tu w przyszłości dodać
        // logikę dla budowania ścian, okien itp.
        // Na razie czyścimy panel ustawień i kończymy
        m_canvas->cancelCurrentMeasure();
        m_settingsDock->setSettingsWidget(nullptr);
    }
}

// Panel ustawień dla trybu zaznaczania.  Umożliwia zmianę koloru i
// grubości linii zaznaczonego pomiaru oraz jego usunięcie.  Jeśli
// użytkownik naciśnie Anuluj, zaznaczanie zostaje zakończone.
void MainWindow::showSelectControls() {
    // Jeśli zaznaczony jest tekst, wyświetl dedykowany panel
    if (m_canvas->hasSelectedText()) {
        showTextSelectControls();
        return;
    }
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4,2,4,2);
    lay->setSpacing(8);
    // Kolor
    auto colorLabel = new QLabel(QString::fromUtf8("Kolor:"), panel);
    lay->addWidget(colorLabel);
    auto colorBtn = new QPushButton(panel);
    // Funkcja aktualizująca kolor przycisku na podstawie zaznaczonego pomiaru
    auto updateBtn = [this, colorBtn]() {
        // Pobierz kolor zaznaczonego pomiaru; jeśli brak, użyj bieżącego koloru
        QColor col = m_canvas->selectedMeasureColor();
        colorBtn->setStyleSheet(QString("background-color: %1").arg(col.name()));
        colorBtn->setText(col.name());
    };
    updateBtn();
    lay->addWidget(colorBtn);
    connect(colorBtn, &QPushButton::clicked, this, [this, colorBtn, updateBtn]() {
        QColor chosen = QColorDialog::getColor(Qt::white, this, QString::fromUtf8("Wybierz kolor"));
        if (chosen.isValid()) {
            m_canvas->setSelectedMeasureColor(chosen);
            updateBtn();
        }
    });
    // Grubość linii
    auto lwLabel = new QLabel(QString::fromUtf8("Grubość:"), panel);
    lay->addWidget(lwLabel);
    auto lwSpin = new QSpinBox(panel);
    lwSpin->setRange(1, 8);
    // Ustaw spina na grubość zaznaczonego pomiaru lub domyślną z ustawień
    lwSpin->setValue(m_canvas->selectedMeasureLineWidth());
    lay->addWidget(lwSpin);
    connect(lwSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ m_canvas->setSelectedMeasureLineWidth(v); });
    lay->addStretch();
    // Przycisk Usuń
    auto delBtn = new QPushButton(QString::fromUtf8("Usuń"), panel);
    lay->addWidget(delBtn);
    connect(delBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->deleteSelectedMeasure();
        // Po usunięciu schowaj panel
        m_settingsDock->setSettingsWidget(nullptr);
    });
    // Anuluj
    auto cancelBtn = new QPushButton(QString::fromUtf8("Zakończ"), panel);
    lay->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->cancelCurrentMeasure();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    panel->setLayout(lay);
    m_settingsDock->setSettingsWidget(panel);
}

// Panel dla wstawiania tekstu.  Pokazuje tylko przycisk Anuluj, ponieważ
// zawartość tekstu jest wprowadzana w dialogu startInsertText().  Po
// kliknięciu Anuluj tryb wstawiania kończy się.
void MainWindow::showInsertTextControls() {
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4,2,4,2);
    lay->setSpacing(8);
    // Kolor tekstu
    auto colorLbl = new QLabel(QString::fromUtf8("Kolor tekstu:"), panel);
    lay->addWidget(colorLbl);
    auto colorBtn = new QPushButton(panel);
    auto updateColorBtn = [colorBtn](const QColor& c) {
        colorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        colorBtn->setText(c.name());
    };
    updateColorBtn(m_canvas->insertTextColor());
    lay->addWidget(colorBtn);
    connect(colorBtn, &QPushButton::clicked, this, [this, updateColorBtn]() {
        QColor chosen = QColorDialog::getColor(m_canvas->insertTextColor(), this, QString::fromUtf8("Wybierz kolor tekstu"));
        if (chosen.isValid()) {
            m_canvas->setInsertTextColor(chosen);
            updateColorBtn(chosen);
        }
    });
    // Kolor tła dymka
    auto bgLbl = new QLabel(QString::fromUtf8("Tło dymka:"), panel);
    lay->addWidget(bgLbl);
    auto bgBtn = new QPushButton(panel);
    auto updateBgBtn = [bgBtn](const QColor& c) {
        bgBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        bgBtn->setText(c.name());
    };
    updateBgBtn(m_canvas->insertTextBgColor());
    lay->addWidget(bgBtn);
    connect(bgBtn, &QPushButton::clicked, this, [this, updateBgBtn]() {
        QColor chosen = QColorDialog::getColor(m_canvas->insertTextBgColor(), this, QString::fromUtf8("Wybierz kolor tła dymka"));
        if (chosen.isValid()) {
            m_canvas->setInsertTextBgColor(chosen);
            updateBgBtn(chosen);
        }
    });
    // Kolor obramowania dymka
    auto borderLbl = new QLabel(QString::fromUtf8("Obramowanie:"), panel);
    lay->addWidget(borderLbl);
    auto borderBtn = new QPushButton(panel);
    auto updateBorderBtn = [borderBtn](const QColor& c) {
        borderBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        borderBtn->setText(c.name());
    };
    updateBorderBtn(m_canvas->insertTextBorderColor());
    lay->addWidget(borderBtn);
    connect(borderBtn, &QPushButton::clicked, this, [this, updateBorderBtn]() {
        QColor chosen = QColorDialog::getColor(m_canvas->insertTextBorderColor(), this, QString::fromUtf8("Wybierz kolor obramowania"));
        if (chosen.isValid()) {
            m_canvas->setInsertTextBorderColor(chosen);
            updateBorderBtn(chosen);
        }
    });
    // Rodzaj czcionki
    auto fontLbl = new QLabel(QString::fromUtf8("Czcionka:"), panel);
    lay->addWidget(fontLbl);
    auto fontCombo = new QFontComboBox(panel);
    fontCombo->setCurrentFont(m_canvas->insertTextFont());
    lay->addWidget(fontCombo);
    // Rozmiar czcionki
    auto sizeLbl = new QLabel(QString::fromUtf8("Rozmiar:"), panel);
    lay->addWidget(sizeLbl);
    auto sizeSpin = new QSpinBox(panel);
    sizeSpin->setRange(6, 48);
    const QFont baseFont = m_canvas->insertTextFont();
    sizeSpin->setValue(baseFont.pointSize() > 0 ? baseFont.pointSize() : 12);
    lay->addWidget(sizeSpin);
    // Styl czcionki (pogrubienie, kursywa, podkreślenie)
    auto boldCheck = new QCheckBox(QString::fromUtf8("B"), panel);
    boldCheck->setToolTip(QString::fromUtf8("Pogrubienie"));
    boldCheck->setChecked(baseFont.bold());
    lay->addWidget(boldCheck);
    auto italicCheck = new QCheckBox(QString::fromUtf8("I"), panel);
    italicCheck->setToolTip(QString::fromUtf8("Kursywa"));
    italicCheck->setChecked(baseFont.italic());
    lay->addWidget(italicCheck);
    auto underlineCheck = new QCheckBox(QString::fromUtf8("U"), panel);
    underlineCheck->setToolTip(QString::fromUtf8("Podkreślenie"));
    underlineCheck->setChecked(baseFont.underline());
    lay->addWidget(underlineCheck);
    lay->addStretch();
    // Potwierdź
    auto confirmBtn = new QPushButton(QString::fromUtf8("Potwierdź"), panel);
    lay->addWidget(confirmBtn);
    connect(confirmBtn, &QPushButton::clicked, this, [this, fontCombo, sizeSpin, boldCheck, italicCheck, underlineCheck]() {
        // Ustaw czcionkę dla nowego dymka
        QFont font = fontCombo->currentFont();
        font.setPointSize(sizeSpin->value());
        font.setBold(boldCheck->isChecked());
        font.setItalic(italicCheck->isChecked());
        font.setUnderline(underlineCheck->isChecked());
        m_canvas->setInsertTextFont(font);
        // Zatwierdź edycję lub wstawianie (commitTextEdit obsługuje oba przypadki)
        m_canvas->commitTextEdit();
        // Ukryj panel
        m_settingsDock->setSettingsWidget(nullptr);
    });
    // Usuń
    auto cancelBtn = new QPushButton(QString::fromUtf8("Usuń"), panel);
    lay->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        // Anuluj wstawianie lub edycję
        m_canvas->cancelTextEdit();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    panel->setLayout(lay);
    m_settingsDock->setSettingsWidget(panel);
}

// Panel dla trybu usuwania.  Zawiera etykietę informacyjną i przycisk
// Anuluj do wyjścia z trybu.  Użytkownik klika na obiekty, aby je
// usuwać; każde kliknięcie powoduje natychmiastowe skasowanie.
void MainWindow::showDeleteControls() {
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4,2,4,2);
    lay->setSpacing(8);
    auto info = new QLabel(QString::fromUtf8("Kliknij pomiar, aby go usunąć."), panel);
    lay->addWidget(info);
    lay->addStretch();
    auto cancelBtn = new QPushButton(QString::fromUtf8("Anuluj"), panel);
    lay->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->cancelCurrentMeasure();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    panel->setLayout(lay);
    m_settingsDock->setSettingsWidget(panel);
}

// Panel ustawień dla zaznaczonego tekstu.  Umożliwia zmianę treści,
// koloru, czcionki oraz usunięcie.  Użytkownik musi potwierdzić zmiany
// przyciskiem Potwierdź, aby zostały zastosowane.  Anulowanie
// przywraca tryb do None i ukrywa panel.
void MainWindow::showTextSelectControls() {
    QWidget* panel = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(panel);
    lay->setContentsMargins(4,2,4,2);
    lay->setSpacing(8);
    // Tekst edytowany jest bezpośrednio na płótnie, dlatego nie tworzymy pola edycji w panelu
    // Kolor tekstu
    auto colorLbl = new QLabel(QString::fromUtf8("Kolor tekstu:"), panel);
    lay->addWidget(colorLbl);
    auto colorBtn = new QPushButton(panel);
    auto updateColorBtn = [this, colorBtn]() {
        QColor col = m_canvas->selectedTextColor();
        colorBtn->setStyleSheet(QString("background-color: %1").arg(col.name()));
        colorBtn->setText(col.name());
    };
    updateColorBtn();
    lay->addWidget(colorBtn);
    connect(colorBtn, &QPushButton::clicked, this, [this, updateColorBtn]() {
        QColor chosen = QColorDialog::getColor(m_canvas->selectedTextColor(), this, QString::fromUtf8("Wybierz kolor tekstu"));
        if (chosen.isValid()) {
            m_canvas->setSelectedTextColor(chosen);
            updateColorBtn();
        }
    });
    // Kolor tła dymka
    auto bgLbl = new QLabel(QString::fromUtf8("Tło dymka:"), panel);
    lay->addWidget(bgLbl);
    auto bgBtn = new QPushButton(panel);
    auto updateBgBtn = [this, bgBtn]() {
        QColor c = m_canvas->selectedTextBgColor();
        bgBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        bgBtn->setText(c.name());
    };
    updateBgBtn();
    lay->addWidget(bgBtn);
    connect(bgBtn, &QPushButton::clicked, this, [this, updateBgBtn]() {
        QColor chosen = QColorDialog::getColor(m_canvas->selectedTextBgColor(), this, QString::fromUtf8("Wybierz kolor tła dymka"));
        if (chosen.isValid()) {
            m_canvas->setSelectedTextBgColor(chosen);
            updateBgBtn();
        }
    });
    // Kolor obramowania dymka
    auto borderLbl = new QLabel(QString::fromUtf8("Obramowanie:"), panel);
    lay->addWidget(borderLbl);
    auto borderBtn = new QPushButton(panel);
    auto updateBorderBtn = [this, borderBtn]() {
        QColor c = m_canvas->selectedTextBorderColor();
        borderBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        borderBtn->setText(c.name());
    };
    updateBorderBtn();
    lay->addWidget(borderBtn);
    connect(borderBtn, &QPushButton::clicked, this, [this, updateBorderBtn]() {
        QColor chosen = QColorDialog::getColor(m_canvas->selectedTextBorderColor(), this, QString::fromUtf8("Wybierz kolor obramowania"));
        if (chosen.isValid()) {
            m_canvas->setSelectedTextBorderColor(chosen);
            updateBorderBtn();
        }
    });
    // Kierunek strzałki
    auto anchorLbl = new QLabel(QString::fromUtf8("Strzałka:"), panel);
    lay->addWidget(anchorLbl);
    auto anchorCombo = new QComboBox(panel);
    anchorCombo->addItem(QString::fromUtf8("Dół"), QVariant::fromValue((int)CalloutAnchor::Bottom));
    anchorCombo->addItem(QString::fromUtf8("Góra"), QVariant::fromValue((int)CalloutAnchor::Top));
    anchorCombo->addItem(QString::fromUtf8("Lewo"), QVariant::fromValue((int)CalloutAnchor::Left));
    anchorCombo->addItem(QString::fromUtf8("Prawo"), QVariant::fromValue((int)CalloutAnchor::Right));
    int curAnchor = (int)m_canvas->selectedTextAnchor();
    for (int i = 0; i < anchorCombo->count(); ++i) {
        if (anchorCombo->itemData(i).toInt() == curAnchor) {
            anchorCombo->setCurrentIndex(i);
            break;
        }
    }
    lay->addWidget(anchorCombo);
    connect(anchorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, anchorCombo]() {
        CalloutAnchor a = static_cast<CalloutAnchor>(anchorCombo->currentData().toInt());
        m_canvas->setSelectedTextAnchor(a);
    });
    // Czcionka: rozmiar, pogrubienie, kursywa, podkreślenie
    auto fontLbl = new QLabel(QString::fromUtf8("Czcionka:"), panel);
    lay->addWidget(fontLbl);
    QFont curFont = m_canvas->selectedTextFont();
    auto sizeSpin = new QSpinBox(panel);
    sizeSpin->setRange(6, 72);
    sizeSpin->setValue(curFont.pointSize() > 0 ? curFont.pointSize() : 12);
    lay->addWidget(sizeSpin);
    auto boldCheck = new QCheckBox(QString::fromUtf8("B"), panel);
    boldCheck->setToolTip(QString::fromUtf8("Pogrubienie"));
    boldCheck->setChecked(curFont.bold());
    lay->addWidget(boldCheck);
    auto italicCheck = new QCheckBox(QString::fromUtf8("I"), panel);
    italicCheck->setToolTip(QString::fromUtf8("Kursywa"));
    italicCheck->setChecked(curFont.italic());
    lay->addWidget(italicCheck);
    auto underlineCheck = new QCheckBox(QString::fromUtf8("U"), panel);
    underlineCheck->setToolTip(QString::fromUtf8("Podkreślenie"));
    underlineCheck->setChecked(curFont.underline());
    lay->addWidget(underlineCheck);
    lay->addStretch();
    // Potwierdź
    auto confirmBtn = new QPushButton(QString::fromUtf8("Potwierdź"), panel);
    lay->addWidget(confirmBtn);
    connect(confirmBtn, &QPushButton::clicked, this, [this, sizeSpin, boldCheck, italicCheck, underlineCheck]() {
        // Przygotuj czcionkę i ustaw ją dla zaznaczonego dymka
        QFont f;
        f.setPointSize(sizeSpin->value());
        f.setBold(boldCheck->isChecked());
        f.setItalic(italicCheck->isChecked());
        f.setUnderline(underlineCheck->isChecked());
        m_canvas->setSelectedTextFont(f);
        // Zakończ ewentualną edycję tekstu (commitTextEdit obsługuje zarówno
        // wstawianie, jak i edycję istniejącego dymka)
        m_canvas->commitTextEdit();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    // Usuń
    auto delBtn = new QPushButton(QString::fromUtf8("Usuń"), panel);
    lay->addWidget(delBtn);
    connect(delBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->deleteSelectedText();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    // Anuluj
    auto cancelBtn = new QPushButton(QString::fromUtf8("Anuluj"), panel);
    lay->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        // Anuluj edycję dymka lub wstawianie i ukryj panel
        m_canvas->cancelTextEdit();
        m_settingsDock->setSettingsWidget(nullptr);
    });
    panel->setLayout(lay);
    m_settingsDock->setSettingsWidget(panel);
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

    // --- Globalne ustawienia koloru i grubości linii ---
    // Dodaj przyciski pozwalające na zmianę koloru i grubości linii
    // dla wszystkich pomiarów (ustawienie domyślne).  Po wyborze nowej
    // wartości użytkownik jest pytany o aktualizację istniejących pomiarów.
    auto globalColorBtn = new QPushButton(QString::fromUtf8("Zmień kolor linii globalnie"), panel);
    lay->addWidget(globalColorBtn);
    connect(globalColorBtn, &QPushButton::clicked, this, [this, updateColorBtn, colorBtn]() {
        // Wybierz nowy kolor domyślny
        QColor chosen = QColorDialog::getColor(m_settings.defaultMeasureColor, this, QString::fromUtf8("Wybierz domyślny kolor linii"));
        if (!chosen.isValid()) return;
        if (chosen == m_settings.defaultMeasureColor) return;
        QColor old = m_settings.defaultMeasureColor;
        m_settings.defaultMeasureColor = chosen;
        // Zapytaj o aktualizację istniejących pomiarów
        if (m_canvas->hasAnyMeasure()) {
            if (QMessageBox::question(this,
                                      QString::fromUtf8("Aktualizuj kolor pomiarów"),
                                      QString::fromUtf8("Czy zastosować nowy domyślny kolor do wszystkich istniejących pomiarów?"))
                == QMessageBox::Yes) {
                m_canvas->updateAllMeasureColors();
            }
        }
        // Aktualizuj przycisk w panelu i ewentualnie bieżący pomiar
        updateColorBtn();
        // Zmień także kolor bieżącego pomiaru, aby odzwierciedlić nową wartość
        m_canvas->setCurrentColor(chosen);
    });

    auto globalLwBtn = new QPushButton(QString::fromUtf8("Zmień grubość linii globalnie"), panel);
    lay->addWidget(globalLwBtn);
    connect(globalLwBtn, &QPushButton::clicked, this, [this, lwSpin]() {
        bool ok = false;
        int v = QInputDialog::getInt(this,
                                     QString::fromUtf8("Nowa domyślna grubość linii"),
                                     QString::fromUtf8("Grubość (1–8):"),
                                     m_settings.lineWidthPx,
                                     1, 8, 1,
                                     &ok);
        if (!ok) return;
        if (v == m_settings.lineWidthPx) return;
        int old = m_settings.lineWidthPx;
        m_settings.lineWidthPx = v;
        if (m_canvas->hasAnyMeasure()) {
            if (QMessageBox::question(this,
                                      QString::fromUtf8("Aktualizuj grubość linii"),
                                      QString::fromUtf8("Czy zastosować nową domyślną grubość do wszystkich istniejących pomiarów?"))
                == QMessageBox::Yes) {
                m_canvas->updateAllMeasureLineWidths();
            }
        }
        // Ustaw grubość dla bieżącego pomiaru
        m_canvas->setCurrentLineWidth(v);
        // Zaktualizuj spinbox
        lwSpin->setValue(v);
    });

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
