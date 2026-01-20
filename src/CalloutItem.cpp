#include "CalloutItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTextDocument>
#include <QLineF>

namespace {
constexpr qreal kCornerRadius = 6.0;
constexpr qreal kPadding = 10.0;
constexpr qreal kTailWidth = 20.0;
constexpr qreal kTailLength = 30.0;
}

CalloutItem::CalloutItem(const QPointF &anchorPos, QGraphicsItem *parent)
    : QGraphicsObject(parent),
      m_anchorScenePos(anchorPos) {
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);

    m_textItem = new QGraphicsTextItem(this);
    m_textItem->setPlainText("Double-click to add a comment...");
    m_textItem->setDefaultTextColor(m_textColor);
    m_textItem->setFont(m_textFont);
    m_textItem->setTextWidth(200);
    updateTextLayout();
}

QRectF CalloutItem::boundingRect() const {
    QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QRectF rect = m_rect.united(QRectF(anchorItemPos, QSizeF(1, 1)).normalized());
    return rect.adjusted(-5, -5, 5, 5);
}

void CalloutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(m_bubbleFill);
    painter->setPen(QPen(m_bubbleBorder, 2));

    QPainterPath path;
    path.addRoundedRect(m_rect, kCornerRadius, kCornerRadius);

    QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QPointF edgePoint = findNearestEdgePoint(anchorItemPos);

    QLineF tailLine(edgePoint, anchorItemPos);
    tailLine.setLength(kTailLength);
    QPointF tip = tailLine.p2();

    QLineF base(edgePoint, tip);
    QLineF perp = base.normalVector();
    perp.setLength(kTailWidth / 2);
    QPointF left = edgePoint + (perp.p2() - perp.p1());
    QPointF right = edgePoint - (perp.p2() - perp.p1());

    QPolygonF tail;
    tail << left << tip << right;
    path.addPolygon(tail);

    painter->drawPath(path);
}

void CalloutItem::updateTextLayout() {
    prepareGeometryChange();
    QSizeF textSize = m_textItem->document()->size();
    m_rect = QRectF(0, 0, textSize.width() + 2 * kPadding, textSize.height() + 2 * kPadding);
    m_textItem->setPos(kPadding, kPadding);
    update();
}

QPointF CalloutItem::findNearestEdgePoint(const QPointF &anchorItemPos) const {
    QPointF p = m_rect.center();
    if (anchorItemPos.x() < m_rect.left()) p.setX(m_rect.left());
    if (anchorItemPos.x() > m_rect.right()) p.setX(m_rect.right());
    if (anchorItemPos.y() < m_rect.top()) p.setY(m_rect.top());
    if (anchorItemPos.y() > m_rect.bottom()) p.setY(m_rect.bottom());
    return p;
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

void CalloutItem::setAnchorPos(const QPointF &scenePos) {
    m_anchorScenePos = scenePos;
    update();
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

void CalloutItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    Q_UNUSED(event)
    if (!m_editing) startEditing();
    else finishEditing();
}

void CalloutItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mouseMoveEvent(event);
}

void CalloutItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mouseReleaseEvent(event);
    updateTextLayout();
}
