#pragma once
#include <QString>
#include <QList>

#include "Measurements.h"
#include <QWidget>
#include <QPdfWriter>
#include <QPainter>
#include <QFile>
#include <QTextStream>

class ExportManager {
public:
    static bool exportToCSV(const QString& path, const QList<Measure>& measures);
    static bool exportToTXT(const QString& path, const QList<Measure>& measures);
    static bool exportToPDF(const QString& path, const QList<Measure>& measures, QWidget* parent);
};
