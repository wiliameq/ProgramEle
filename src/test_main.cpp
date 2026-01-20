#include <QGuiApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QDebug>
#include "CalloutItem.h"

int main(int argc, char *argv[]) {
    // Tryb offscreen
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QGuiApplication app(argc, argv);

    qDebug() << "🧪 Running headless test for ProgramEle...";

    // Utwórz kontekst OpenGL i powierzchnię (bez okna)
    QOpenGLContext glContext;
    glContext.create();
    QOffscreenSurface surface;
    surface.create();
    glContext.makeCurrent(&surface);

    // Utwórz scenę graficzną i pusty widok (nie pokazujemy go)
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.setSceneRect(0, 0, 400, 300);

    // Utwórz obiekt testowy CalloutItem
    CalloutItem *testItem = new CalloutItem(QPointF(100, 100));
    scene.addItem(testItem);

    // Ustawienia testowe
    testItem->setAnchorPos(QPointF(200, 150));
    testItem->setBubbleFill(Qt::yellow);
    testItem->setBubbleBorder(Qt::black);
    testItem->setTextColor(Qt::blue);

    // Wymuszenie przeliczenia geometrii
    QRectF rect = testItem->boundingRect();
    qDebug() << "✅ CalloutItem initialized successfully";
    qDebug() << "Anchor:" << testItem->anchorPos();
    qDebug() << "Bounding rect:" << rect;

    // Render sceny offscreen
    QImage buffer(400, 300, QImage::Format_ARGB32);
    buffer.fill(Qt::transparent);
    QPainter painter(&buffer);
    scene.render(&painter);
    painter.end();

    qDebug() << "✅ Scene rendered offscreen successfully.";
    delete testItem;

    qDebug() << "✅ Headless test completed successfully.";
    return 0;
}
