#include "CalloutItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTextDocument>
#include <QLineF>

#include <algorithm>

namespace {
constexpr qreal kHandleSize = 8.0;
constexpr qreal kHandleHalf = kHandleSize / 2.0;
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

    setPos(anchorPos.x() - m_rect.width() / 2.0,
           anchorPos.y() - m_rect.height() - kTailLength);

    updateTextLayout();
    ensureAnchorBelowBubble();
}

QRectF CalloutItem::boundingRect() const {
    const QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QRectF bounds = m_rect;
    bounds = bounds.united(QRectF(anchorItemPos - QPointF(kHandleSize, kHandleSize),
                                  QSizeF(kHandleSize * 2.0, kHandleSize * 2.0)));
    return bounds.adjusted(-kHandleSize, -kHandleSize, kHandleSize, kHandleSize);
}

void CalloutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QPointF attachPoint = m_rect.center();
    QPointF tailLeft;
    QPointF tailRight;

    if (anchorItemPos.y() > m_rect.bottom()) {
        attachPoint = QPointF(m_rect.center().x(), m_rect.bottom());
        tailLeft = attachPoint + QPointF(-kTailWidth / 2.0, 0.0);
        tailRight = attachPoint + QPointF(kTailWidth / 2.0, 0.0);
    } else if (anchorItemPos.y() < m_rect.top()) {
        attachPoint = QPointF(m_rect.center().x(), m_rect.top());
        tailLeft = attachPoint + QPointF(-kTailWidth / 2.0, 0.0);
        tailRight = attachPoint + QPointF(kTailWidth / 2.0, 0.0);
    } else if (anchorItemPos.x() < m_rect.left()) {
        attachPoint = QPointF(m_rect.left(), m_rect.center().y());
        tailLeft = attachPoint + QPointF(0.0, -kTailWidth / 2.0);
        tailRight = attachPoint + QPointF(0.0, kTailWidth / 2.0);
    } else {
        attachPoint = QPointF(m_rect.right(), m_rect.center().y());
        tailLeft = attachPoint + QPointF(0.0, -kTailWidth / 2.0);
        tailRight = attachPoint + QPointF(0.0, kTailWidth / 2.0);
    }

    QPainterPath bubblePath;
    bubblePath.addRoundedRect(m_rect, kCornerRadius, kCornerRadius);

    QPainterPath tailPath;
    QPolygonF tailPolygon;
    tailPolygon << tailLeft << tailRight << anchorItemPos;
    tailPath.addPolygon(tailPolygon);

    painter->setPen(QPen(m_bubbleBorder, 1.2));
    painter->setBrush(m_bubbleFill);
    painter->drawPath(bubblePath);
    painter->drawPath(tailPath);

    if (isSelected()) {
        painter->setPen(QPen(Qt::black, 1.0, Qt::DashLine));
        painter->setBrush(Qt::white);
        painter->drawRect(m_rect);

        painter->setPen(QPen(Qt::black, 1.0));
        const QPointF tl = m_rect.topLeft();
        const QPointF tr = m_rect.topRight();
        const QPointF bl = m_rect.bottomLeft();
        const QPointF br = m_rect.bottomRight();
        painter->drawRect(QRectF(tl - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize)));
        painter->drawRect(QRectF(tr - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize)));
        painter->drawRect(QRectF(bl - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize)));
        painter->drawRect(QRectF(br - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize)));
        painter->drawEllipse(QRectF(anchorItemPos - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize)));
    }
}

void CalloutItem::setTextColor(const QColor &color) {
    m_textColor = color;
    if (m_textItem) {
        m_textItem->setDefaultTextColor(color);
    }
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
    if (m_textItem) {
        m_textItem->setFont(font);
    }
    updateTextLayout();
}

void CalloutItem::startEditing() {
    if (!m_textItem) {
        return;
    }
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setFocus(Qt::MouseFocusReason);
}

void CalloutItem::finishEditing() {
    if (!m_textItem) {
        return;
    }
    m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
    emit editingFinished(this);
}

bool CalloutItem::isEditing() const {
    if (!m_textItem) {
        return false;
    }
    return m_textItem->textInteractionFlags() != Qt::NoTextInteraction;
}

void CalloutItem::setAnchorPos(const QPointF &scenePos) {
    if (m_anchorScenePos == scenePos) {
        return;
    }
    prepareGeometryChange();
    m_anchorScenePos = scenePos;
    update();
}

void CalloutItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    m_activeHandle = handleAt(event->scenePos());
    m_startRect = m_rect;
    m_startPos = pos();
    m_startAnchor = m_anchorScenePos;
    m_dragOffset = event->scenePos() - pos();

    if (m_activeHandle == Handle::Anchor) {
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }

    if (m_activeHandle != Handle::None) {
        setCursor(Qt::SizeAllCursor);
        event->accept();
        return;
    }

    setCursor(Qt::ClosedHandCursor);
    QGraphicsObject::mousePressEvent(event);
}

void CalloutItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_activeHandle == Handle::Anchor) {
        setAnchorPos(event->scenePos());
        event->accept();
        return;
    }

    if (m_activeHandle != Handle::None) {
        QRectF newRect = m_startRect;
        const QPointF delta = event->scenePos() - (m_startPos + m_startRect.topLeft());

        switch (m_activeHandle) {
            case Handle::TopLeft:
                newRect.setTopLeft(m_startRect.topLeft() + delta);
                break;
            case Handle::TopRight:
                newRect.setTopRight(m_startRect.topRight() + delta);
                break;
            case Handle::BottomLeft:
                newRect.setBottomLeft(m_startRect.bottomLeft() + delta);
                break;
            case Handle::BottomRight:
                newRect.setBottomRight(m_startRect.bottomRight() + delta);
                break;
            default:
                break;
        }

        newRect = newRect.normalized();
        newRect.setWidth(std::max(newRect.width(), 80.0));
        newRect.setHeight(std::max(newRect.height(), 40.0));

        prepareGeometryChange();
        m_rect = newRect;
        m_userResized = true;
        updateTextLayout();
        event->accept();
        return;
    }

    if (event->buttons() & Qt::LeftButton) {
        setPos(event->scenePos() - m_dragOffset);
        event->accept();
        return;
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void CalloutItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    unsetCursor();
    m_activeHandle = Handle::None;
    QGraphicsObject::mouseReleaseEvent(event);
}

void CalloutItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        startEditing();
        event->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

CalloutItem::Handle CalloutItem::handleAt(const QPointF &scenePos) const {
    const QPointF itemPos = mapFromScene(scenePos);
    const QRectF tl(m_rect.topLeft() - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize));
    const QRectF tr(m_rect.topRight() - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize));
    const QRectF bl(m_rect.bottomLeft() - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize));
    const QRectF br(m_rect.bottomRight() - QPointF(kHandleHalf, kHandleHalf), QSizeF(kHandleSize, kHandleSize));

    if (tl.contains(itemPos)) {
        return Handle::TopLeft;
    }
    if (tr.contains(itemPos)) {
        return Handle::TopRight;
    }
    if (bl.contains(itemPos)) {
        return Handle::BottomLeft;
    }
    if (br.contains(itemPos)) {
        return Handle::BottomRight;
    }

    if (QLineF(scenePos, m_anchorScenePos).length() <= kHandleSize) {
        return Handle::Anchor;
    }

    return Handle::None;
}

void CalloutItem::updateTextLayout() {
    if (!m_textItem) {
        return;
    }

    const qreal textWidth = std::max(m_rect.width() - kPadding * 2.0, 40.0);
    m_textItem->setTextWidth(textWidth);
    m_textItem->setPos(m_rect.topLeft() + QPointF(kPadding, kPadding));

    if (!m_userResized) {
        const QSizeF docSize = m_textItem->document()->size();
        prepareGeometryChange();
        m_rect.setHeight(std::max(docSize.height() + kPadding * 2.0, 40.0));
    }

    update();
}

void CalloutItem::ensureAnchorBelowBubble() {
    const QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    if (!m_rect.contains(anchorItemPos)) {
        return;
    }

    const QPointF newAnchorScene = mapToScene(m_rect.center() + QPointF(0.0, m_rect.height() / 2.0 + kTailLength));
    setAnchorPos(newAnchorScene);
}
