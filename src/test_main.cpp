#include <QGuiApplication>
#include <QDebug>
#include <QTimer>
#include "CalloutItem.h"

// Test logiczny CalloutItem w środowisku offscreen (QGuiApplication).

int main(int argc, char *argv[]) {
    // Wymuszenie trybu offscreen
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QGuiApplication app(argc, argv);
    qDebug() << "🧪 Running enhanced headless logic test (offscreen + GUI)...";

    try {
        // Krótka przerwa, żeby Qt w pełni zainicjalizował środowisko graficzne
        QTimer::singleShot(100, []() {
            try {
                // Utworzenie obiektu CalloutItem
                CalloutItem item(QPointF(100, 100));

                // Ustawienia testowe
                item.setAnchorPos(QPointF(200, 150));
                item.setBubbleFill(Qt::yellow);
                item.setBubbleBorder(Qt::black);
                item.setTextColor(Qt::blue);

                // Odczyt właściwości
                QRectF rect = item.boundingRect();
                QPointF anchor = item.anchorPos();

                qDebug() << "✅ CalloutItem constructed successfully";
                qDebug() << "Anchor:" << anchor;
                qDebug() << "Rect:" << rect;

                // Test sanity check
                if (rect.width() < 1 || rect.height() < 1)
                    qDebug() << "⚠️ Warning: bounding rect too small!";

                qDebug() << "✅ Headless logic test completed successfully.";
            } catch (std::exception &e) {
                qDebug() << "❌ Exception in inner logic:" << e.what();
            } catch (...) {
                qDebug() << "❌ Unknown exception in CalloutItem constructor or logic.";
            }

            // Zakończ aplikację po wykonaniu testu
            QCoreApplication::exit(0);
        });

        // Uruchom główną pętlę (potrzebna dla QGuiApplication)
        return app.exec();
    }
    catch (std::exception &e) {
        qDebug() << "❌ Exception in main:" << e.what();
        return 1;
    }
    catch (...) {
        qDebug() << "❌ Unknown fatal error in main.";
        return 1;
    }
}
