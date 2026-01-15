#pragma once
#include <QColor>
struct ProjectSettings {
    enum class Unit { Cm, M };
    Unit defaultUnit = Unit::Cm;     // default: centimeters
    int decimals = 1;                // e.g., 1 => 0.1 cm
    /**
     * Domyślny zapas dla nowych pomiarów.
     *
     * Wartość ta jest interpretowana zgodnie z wybraną w projekcie jednostką
     * (cm lub m). Na przykład, jeśli domyślna jednostka to centymetry (Cm),
     * wartość 50 oznacza 50 cm. Jeśli jednostka to metry (M), ta sama wartość
     * 50 oznacza 50 m. Ustawienie to służy jako „startowy” zapas dodawany do
     * każdego nowego pomiaru liniowego, wieloliniowego oraz zaawansowanego.
     */
    double defaultBuffer = 0.0;
    QColor defaultMeasureColor = QColor(0,155,0);
    int lineWidthPx = 2;
};
