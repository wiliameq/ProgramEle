#include <QCoreApplication>
#include <QDebug>
#include "CalloutItem.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qDebug() << "🧪 Running headless test for ProgramEle...";

    // Utworzenie testowego dymka bez GUI
    CalloutItem testItem(QPointF(100, 100));
    testItem.setAnchorPos(QPointF(250, 150));
    testItem.setBubbleFill(Qt::yellow);
    testItem.setBubbleBorder(Qt::black);
    testItem.setTextColor(Qt::blue);

    qDebug() << "✅ CalloutItem initialized successfully";
    qDebug() << "Anchor:" << testItem.anchorPos();
    qDebug() << "Bounding rect:" << testItem.boundingRect();

    qDebug() << "✅ Headless test completed successfully.";
    return 0;
}
