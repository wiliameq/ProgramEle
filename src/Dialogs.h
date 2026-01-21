#pragma once
#include <QDialog>
#include <QColor>
#include <vector>
#include "Settings.h"

class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QTableWidget;
class QLabel;

struct Measure;
struct ProjectSettings;

// --- Pomiar zaawansowany (definiowanie szablonu) ---
class AdvancedMeasureDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * Dialog konfiguracji pomiaru zaawansowanego.
     *
     * Pozwala ustawić nazwę, domyślny zapas i kolor linii. Jednostka jest
     * pobierana globalnie z ustawień projektu, dlatego nie jest tutaj
     * wybierana.
     */
    explicit AdvancedMeasureDialog(QWidget* parent, ProjectSettings* settings);
    QString name() const;
    double bufferValue() const;
    QColor color() const;
private:
    ProjectSettings* m_settings = nullptr;
    QLineEdit* m_name = nullptr;
    QDoubleSpinBox* m_buffer = nullptr;
    QPushButton* m_colorBtn = nullptr;
    QColor m_chosen;
};

// --- Zapas końcowy po zakończeniu rysowania ---
class FinalBufferDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * Dialog do ustawiania zapasu końcowego po zakończeniu rysowania.
     * Jednostką projektu są centymetry, więc nie ma wyboru jednostki.
     */
    explicit FinalBufferDialog(QWidget* parent, ProjectSettings* settings);
    double bufferValue() const;
private:
    ProjectSettings* m_settings = nullptr;
    QDoubleSpinBox* m_buffer = nullptr;
};

// --- Edycja istniejącego pomiaru ---
class EditMeasureDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditMeasureDialog(QWidget* parent, ProjectSettings* settings, Measure* measure);
private:
    ProjectSettings* m_settings = nullptr;
    Measure* m = nullptr;
    QLineEdit* m_name = nullptr;
    QDoubleSpinBox* m_bufDefault = nullptr;
    QDoubleSpinBox* m_bufFinal = nullptr;
    QPushButton* m_colorBtn = nullptr;
    QColor m_chosen;
};

// --- Raport z checkboxami, sumami i edycją ---
class ReportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReportDialog(QWidget* parent, ProjectSettings* settings, std::vector<Measure>* measures);
private:
    void recalc();
    ProjectSettings* m_settings = nullptr;
    std::vector<Measure>* m_measures = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_sumLen = nullptr;
    QLabel* m_sumBuf = nullptr;
    QLabel* m_sumTotal = nullptr;
};
