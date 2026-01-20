#pragma once

#include <QGraphicsObject>
#include <QGraphicsTextItem>
#include <QColor>
#include <QFont>

class CalloutItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit CalloutItem(const QPointF &anchorPos, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTextColor(const QColor &color);
    void setBubbleFill(const QColor &color);
    void setBubbleBorder(const QColor &color);
    void setFont(const QFont &font);

    void setAnchorPos(const QPointF &scenePos);
    QPointF anchorPos() const { return m_anchorScenePos; }

    void startEditing();
    void finishEditing();
    bool isEditing() const;

signals:
    void editingFinished(CalloutItem *item);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void updateTextLayout();
    QPointF findNearestEdgePoint(const QPointF &anchorItemPos) const;

    QGraphicsTextItem *m_textItem {nullptr};
    QRectF m_rect {0, 0, 200, 100};
    QPointF m_anchorScenePos;

    QColor m_textColor {Qt::black};
    QColor m_bubbleFill {QColor(255, 255, 220)};
    QColor m_bubbleBorder {Qt::black};
    QFont m_textFont {"Arial", 10};

    bool m_editing {false};
};
