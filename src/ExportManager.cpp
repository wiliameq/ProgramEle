#include "ExportManager.h"
#include <QMessageBox>
#include <QDateTime>
#include <QFont>
#include <QPageSize>

bool ExportManager::exportToCSV(const QString& path, const QList<Measure>& measures) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "ID,Name,Length,Unit\n";
    for (const auto& m : measures) {
        out << m.id << "," << m.name << "," << m.lengthMeters << "," << m.unit << "\n";
    }
    return true;
}

bool ExportManager::exportToTXT(const QString& path, const QList<Measure>& measures) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "Raport pomiarów\n==================\n";
    for (const auto& m : measures) {
        out << QString("ID: %1 | %2 | %3 %4\n").arg(m.id).arg(m.name).arg(m.lengthMeters).arg(m.unit);
    }
    return true;
}

bool ExportManager::exportToPDF(const QString& path, const QList<Measure>& measures, QWidget* parent) {
    if (path.isEmpty()) return false;

    QString fixedPath = path;
    if (!fixedPath.endsWith(".pdf", Qt::CaseInsensitive))
        fixedPath += ".pdf";

    QPdfWriter pdf(fixedPath);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(300);
    pdf.setTitle("Raport pomiarów");

    QPainter painter(&pdf);
    if (!painter.isActive()) {
        QMessageBox::critical(parent, "Błąd eksportu", "Nie udało się utworzyć pliku PDF.");
        return false;
    }

    const int margin = 100;
    int y = margin;

    // Nagłówek raportu
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    painter.drawText(margin, y, "Raport pomiarów");
    y += 40;
    painter.setFont(QFont("Arial", 10));
    painter.drawText(margin, y, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    y += 50;

    // Nagłówki kolumn
    int colWidth = (pdf.width() - 2 * margin) / 5;
    int rowHeight = 30;
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawRect(margin, y, pdf.width() - 2 * margin, rowHeight);
    painter.drawText(margin + 10, y + 20, "ID");
    painter.drawText(margin + colWidth + 10, y + 20, "Nazwa");
    painter.drawText(margin + 2 * colWidth + 10, y + 20, "Długość [m]");
    painter.drawText(margin + 3 * colWidth + 10, y + 20, "Bufor [m]");
    painter.drawText(margin + 4 * colWidth + 10, y + 20, "Razem [m]");
    y += rowHeight;

    painter.setFont(QFont("Arial", 9));

    // Dane
    for (const auto& m : measures) {
        painter.drawRect(margin, y, pdf.width() - 2 * margin, rowHeight);
        painter.drawText(margin + 10, y + 20, QString::number(m.id));
        painter.drawText(margin + colWidth + 10, y + 20, m.name);
        painter.drawText(margin + 2 * colWidth + 10, y + 20, QString::number(m.lengthMeters, 'f', 2));
        painter.drawText(margin + 3 * colWidth + 10, y + 20, QString::number(m.bufferFinalMeters, 'f', 2));
        painter.drawText(margin + 4 * colWidth + 10, y + 20, QString::number(m.totalWithBufferMeters, 'f', 2));
        y += rowHeight;

        if (y > pdf.height() - margin - rowHeight) {
            pdf.newPage();
            y = margin;
        }
    }

    painter.end();
    QMessageBox::information(parent, "Eksport zakończony", "Plik PDF został poprawnie utworzony.");
    return true;
}
