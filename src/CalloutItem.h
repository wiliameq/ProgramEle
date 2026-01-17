#pragma once

#include <QGraphicsObject>
#include <QGraphicsTextItem>
#include <QColor>
#include <QFont>

class CalloutItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum class Handle {
        None,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Anchor
    };

    explicit CalloutItem(const QPointF &anchorPos, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTextColor(const QColor &color);
    void setBubbleFill(const QColor &color);
    void setBubbleBorder(const QColor &color);
    void setFont(const QFont &font);

    QColor textColor() const { return m_textColor; }
    QColor bubbleFill() const { return m_bubbleFill; }
    QColor bubbleBorder() const { return m_bubbleBorder; }
    QFont textFont() const { return m_textFont; }

    void startEditing();
    void finishEditing();
    bool isEditing() const;

    void setAnchorPos(const QPointF &scenePos);
    QPointF anchorPos() const { return m_anchorScenePos; }

signals:
    void editingFinished(CalloutItem *item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    Handle handleAt(const QPointF &scenePos) const;
    void updateTextLayout();
    void ensureAnchorBelowBubble();

    QGraphicsTextItem *m_textItem = nullptr;
    QRectF m_rect{0, 0, 160, 80};
    QPointF m_anchorScenePos;
    QColor m_textColor = Qt::black;
    QColor m_bubbleFill = QColor(255, 255, 255, 200);
    QColor m_bubbleBorder = Qt::black;
    QFont m_textFont;
    bool m_userResized = false;

    Handle m_activeHandle = Handle::None;
    QRectF m_startRect;
    QPointF m_startPos;
    QPointF m_startAnchor;
    QPointF m_dragOffset;
};
