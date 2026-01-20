#include "CalloutItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTextDocument>
#include <QLineF>

namespace {
constexpr qreal kCornerRadius = 6.0;
constexpr qreal kPadding = 8.0;
constexpr qreal kTailWidth = 18.0;
constexpr qreal kTailLength = 24.0;
}

CalloutItem::CalloutItem(const QPointF &anchorPos, QGraphicsItem *parent)
    : QGraphicsObject(parent),
      m_anchorScenePos(anchorPos) {

    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    setAcceptHoverEvents(true);

    m_textItem = new QGraphicsTextItem(this);
    m_textItem->setPlainText(QStringLiteral("Double-click to edit"));
    m_textItem->setDefaultTextColor(m_textColor);
    m_textItem->setFont(m_textFont);

    updateTextLayout();
    ensureAnchorBelowBubble();
}

QRectF CalloutItem::boundingRect() const {
    const QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QRectF bounds = m_rect.united(QRectF(anchorItemPos, QSizeF(1, 1)).normalized());
    return bounds.adjusted(-4, -4, 4, 4);
}

void CalloutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing);

    painter->setBrush(m_bubbleFill);
    painter->setPen(QPen(m_bubbleBorder, 2));

    QPainterPath path;
    path.addRoundedRect(m_rect, kCornerRadius, kCornerRadius);

    QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QPointF center = m_rect.center();

    QLineF line(center, anchorItemPos);
    line.setLength(kTailLength);
    QPointF tip = line.p2();

    QPointF left = center + QPointF(-kTailWidth / 2, 0);
    QPointF right = center + QPointF(kTailWidth / 2, 0);

    QPolygonF tail;
    tail << left << tip << right;

    path.addPolygon(tail);
    painter->drawPath(path);
}

void CalloutItem::updateTextLayout() {
    prepareGeometryChange();
    QRectF textRect = m_textItem->boundingRect();
    m_rect = textRect.adjusted(-kPadding, -kPadding, kPadding, kPadding);
    m_textItem->setPos(m_rect.topLeft() + QPointF(kPadding, kPadding));
}

void CalloutItem::ensureAnchorBelowBubble() {
    if (m_anchorScenePos.isNull())
        return;

    QPointF localAnchor = mapFromScene(m_anchorScenePos);
    if (localAnchor.y() > m_rect.bottom())
        return;

    setPos(pos().x(), pos().y() + (m_rect.height() + kTailLength));
}

void CalloutItem::setTextColor(const QColor &color) {
    m_textColor = color;
    m_textItem->setDefaultTextColor(color);
    update();
}

void CalloutItem::setBubbleFill(const QColor &color) {
    m_bubbleFill = color;
    update();
}

void CalloutItem::setBubbleBorder(const QColor &color) {
    m_bubbleBorder = color;
    update();
}

void CalloutItem::setFont(const QFont &font) {
    m_textFont = font;
    m_textItem->setFont(font);
    updateTextLayout();
}

void CalloutItem::startEditing() {
    m_editing = true;
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setFocus();
}

void CalloutItem::finishEditing() {
    m_editing = false;
    m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
    updateTextLayout();
    emit editingFinished(this);
}

bool CalloutItem::isEditing() const {
    return m_editing;
}

void CalloutItem::setAnchorPos(const QPointF &scenePos) {
    m_anchorScenePos = scenePos;
    update();
}

void CalloutItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mousePressEvent(event);
}

void CalloutItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mouseMoveEvent(event);
}

void CalloutItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mouseReleaseEvent(event);
}

void CalloutItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QGraphicsItem::hoverMoveEvent(event);
}
