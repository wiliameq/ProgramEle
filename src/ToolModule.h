#pragma once

#include <QPointF>
#include <QString>

class QPainter;
class QMouseEvent;
class QKeyEvent;
class QWidget;
struct ProjectSettings;

class ToolHost {
public:
    virtual ~ToolHost() = default;
    virtual QPointF toWorld(const QPointF& screen) const = 0;
    virtual QPointF toScreen(const QPointF& world) const = 0;
    virtual double zoom() const = 0;
    virtual double pixelsPerMeter() const = 0;
    virtual ProjectSettings* settings() const = 0;
    virtual bool isLayerVisible(const QString& layer) const = 0;
    virtual void requestUpdate() = 0;
};

class ToolModule {
public:
    virtual ~ToolModule() = default;
    virtual QString name() const = 0;
    virtual QString layerName() const = 0;
    virtual bool isActive() const = 0;
    virtual void activate() = 0;
    virtual void deactivate() = 0;

    virtual void draw(QPainter& painter) = 0;
    virtual void drawOverlay(QPainter& painter, bool hasMouseWorld, const QPointF& mouseWorld) = 0;

    virtual bool mousePress(QMouseEvent* event) = 0;
    virtual bool mouseMove(QMouseEvent* event, const QPointF& worldPos) = 0;
    virtual bool mouseRelease(QMouseEvent* event) = 0;
    virtual bool mouseDoubleClick(QMouseEvent* event) = 0;
    virtual bool keyPress(QKeyEvent* event, QWidget* parentForDialogs) = 0;
};
