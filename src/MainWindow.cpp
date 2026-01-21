#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ToolSettingsWidget.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QDockWidget>
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
    auto measMenu = menuBar()->addMenu("Pomiary");
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
