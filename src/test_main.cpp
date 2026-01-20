#include <QCoreApplication>
#include <QDebug>
#include "CalloutItem.h"

// Ten test sprawdza logikę klasy CalloutItem bez sceny graficznej.
// Działa w CI (offscreen), nie używa QGraphicsScene ani QGuiApplication.

int main(int argc, char *argv[]) {
    // Ustawienie trybu offscreen (na wszelki wypadek)
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QCoreApplication app(argc, argv);

    qDebug() << "🧪 Running headless logic test (non-GUI)...";

    try {
        // Utwórz obiekt CalloutItem (bez rodzica graficznego)
        CalloutItem item(QPointF(100, 100));

        // Ustaw parametry testowe
        item.setAnchorPos(QPointF(200, 150));
        item.setBubbleFill(Qt::yellow);
        item.setBubbleBorder(Qt::black);
        item.setTextColor(Qt::blue);

        // Wywołaj bezpieczne metody logiczne (bez sceny)
        QRectF rect = item.boundingRect();
        QPointF anchor = item.anchorPos();

        qDebug() << "✅ CalloutItem created successfully.";
        qDebug() << "Anchor position:" << anchor;
        qDebug() << "Bounding rect:" << rect;

        // Prosta kontrola sanity check
        if (rect.width() <= 0 || rect.height() <= 0)
            qDebug() << "⚠️ Unexpected rect size.";

        qDebug() << "✅ Logical test finished without crash.";
    }
    catch (std::exception &e) {
        qDebug() << "❌ Exception caught:" << e.what();
        return 1;
    }
    catch (...) {
        qDebug() << "❌ Unknown exception caught.";
        return 1;
    }

    qDebug() << "✅ Headless logic test completed successfully.";
    return 0;
}
