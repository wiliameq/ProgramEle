#pragma once
#include <QString>
#include <QList>
#include <QWidget>
#include <QPdfWriter>
#include <QPainter>
#include <QFile>
#include <QTextStream>
#include "CanvasWidget.h"

class ExportManager {
public:
    static bool exportToCSV(const QString& path, const QList<Measure>& measures);
    static bool exportToTXT(const QString& path, const QList<Measure>& measures);
    static bool exportToPDF(const QString& path, const QList<Measure>& measures, QWidget* parent);
};
