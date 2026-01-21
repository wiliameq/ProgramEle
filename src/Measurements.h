#pragma once

#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QString>
#include <vector>

enum class MeasureType { Linear, Polyline, Advanced };

struct Measure {
    int id = 0;
    MeasureType type = MeasureType::Polyline;
    QString name;
    QColor color = QColor(0,155,0);
    QString unit = "cm";
    // Global buffer (nieużywany, pozostawiony dla zgodności). Ta
    // wartość jest dodawana do każdej długości, ale nie jest wyświetlana w
    // kolumnach "Zapas początkowy" ani "Zapas końcowy" w raporcie.
    double bufferGlobalMeters  = 0.0;
    // Początkowy zapas ("zapas początkowy") przypisany do konkretnego
    // pomiaru. W przypadku pomiarów liniowych i polilinii ta wartość jest
    // domyślnie zerowa. Dla pomiaru zaawansowanego może zostać ustawiona
    // w dialogu konfiguracji pomiaru.
    double bufferDefaultMeters = 0.0;
    // Końcowy zapas ("zapas końcowy"), ustawiany w drugim etapie
    // pomiaru zaawansowanego. Dla pozostałych pomiarów jest równy zero.
    double bufferFinalMeters   = 0.0;
    std::vector<QPointF> pts;
    QDateTime createdAt;
    double lengthMeters = 0.0;
    double totalWithBufferMeters = 0.0;
    bool visible = true;
    // Szerokość linii używana do rysowania tego pomiaru (w pikselach).  Domyślnie
    // przypisana z globalnych ustawień w chwili tworzenia pomiaru. Dzięki temu
    // poszczególne pomiary mogą mieć różne grubości linii bez modyfikowania
    // ustawień globalnych.
    int lineWidthPx = 1;

    // Nazwa warstwy, do której należy ten pomiar.  Warstwy umożliwiają
    // grupowe włączanie i wyłączanie widoczności elementów w zależności od
    // kategorii projektu.  Domyślnie wszystkie pomiary należą do warstwy
    // "Pomiary", ale w przyszłości można ją zmieniać zgodnie z kategorią.
    QString layer = QStringLiteral("Pomiary");
};
