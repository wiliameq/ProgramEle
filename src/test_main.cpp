#include <QGuiApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QDebug>
#include "CalloutItem.h"

// Ten test działa w trybie headless (bez wyświetlania GUI),
// ale tworzy scenę graficzną, aby QGraphicsObject nie crashował.

int main(int argc, char *argv[]) {
    // QGuiApplication pozwala na użycie klas graficznych (QGraphicsItem itp.)
    QGuiApplication app(argc, argv);

    qDebug() << "🧪 Running headless test for ProgramEle...";

    // Utwórz pustą scenę (bez wyświetlania)
    QGraphicsScene scene;

    // Utwórz CalloutItem z poprawnym parentem
    auto *testItem = new CalloutItem(QPointF(100, 100));
    scene.addItem(testItem);

    // Ustaw testowe parametry
    testItem->setAnchorPos(QPointF(250, 150));
    testItem->setBubbleFill(Qt::yellow);
    testItem->setBubbleBorder(Qt::black);
    testItem->setTextColor(Qt::blue);

    // Wypisz podstawowe informacje diagnostyczne
    qDebug() << "✅ CalloutItem initialized successfully";
    qDebug() << "Anchor:" << testItem->anchorPos();
    qDebug() << "Bounding rect:" << testItem->boundingRect();

    // Usuń obiekt, żeby sprawdzić poprawne niszczenie
    delete testItem;

    qDebug() << "✅ Headless test completed successfully.";
    return 0;
}
