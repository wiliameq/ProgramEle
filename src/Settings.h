#pragma once
#include <QColor>
struct ProjectSettings {
    int decimals = 1;                // e.g., 1 => 0.1 cm
    QColor defaultMeasureColor = QColor(0,155,0);
    int lineWidthPx = 2;
};
