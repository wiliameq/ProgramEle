#include "CalloutItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QPainter>
#include <QTextDocument>
#include <QLineF>
#include <limits>

namespace {
constexpr qreal kCornerRadius = 6.0;
constexpr qreal kPadding = 10.0;
constexpr qreal kHandleSize = 10.0;
constexpr qreal kMinWidth = 80.0;
constexpr qreal kMinHeight = 50.0;
constexpr qreal kTailWidth = 20.0;
}

// ======================================================
//  Konstruktor — BEZPIECZNY dla środowisk bez sceny i GUI
// ======================================================
CalloutItem::CalloutItem(const QPointF &anchorPos, QGraphicsItem *parent)
    : QGraphicsObject(parent),
      m_anchorScenePos(anchorPos) {
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);

    // Utworzenie tekstu z fallbackiem — bez crasha przy braku X11/fontów
    m_textItem = new QGraphicsTextItem(this);

    // Fallback czcionki — unika QFontDatabase::load w headless CI
    QFont safeFont("DejaVu Sans");
    if (safeFont.family().isEmpty())
        safeFont = QFont(); // Qt fallback
    m_textItem->setFont(safeFont);

    // Tekst startowy
    m_textItem->setPlainText("Double-click to add a comment...");
    m_textItem->setDefaultTextColor(m_textColor);

    // Połączenie sygnału: aktualizacja layoutu tylko, gdy obiekt w scenie
    QObject::connect(m_textItem->document(), &QTextDocument::contentsChanged,
                     this, [this]() {
                         if (scene()) updateTextLayout();
                     });
}

// ======================================================
//  Główna geometria i rysowanie
// ======================================================
QRectF CalloutItem::boundingRect() const {
    QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QRectF rect = m_rect.united(QRectF(anchorItemPos, QSizeF(1, 1)).normalized());
    return rect.adjusted(-kHandleSize, -kHandleSize, kHandleSize, kHandleSize);
}

void CalloutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if (!painter)
        return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(m_bubbleFill);
    painter->setPen(QPen(m_bubbleBorder, 2));

    QPainterPath path;
    path.addRoundedRect(m_rect, kCornerRadius, kCornerRadius);

    QPointF anchorItemPos = mapFromScene(m_anchorScenePos);
    QPointF edgePoint = calculateEdgeIntersection(anchorItemPos);

    QPointF tip = anchorItemPos;
    QLineF base(edgePoint, tip);
    QLineF perp = base.normalVector();
    perp.setLength(kTailWidth / 2);
    QPointF left = edgePoint + (perp.p2() - perp.p1());
    QPointF right = edgePoint - (perp.p2() - perp.p1());

    QPolygonF tail;
    tail << left << tip << right;
    path.addPolygon(tail);

    painter->drawPath(path);

    painter->setBrush(Qt::white);
    painter->setPen(Qt::black);
    for (const auto &handle : m_handles) {
        painter->drawEllipse(handle);
    }
}

// ======================================================
//  Układ tekstu — bezpieczny dla środowiska offscreen
// ======================================================
void CalloutItem::updateTextLayout() {
    if (!m_textItem)
        return;
    if (!scene())
        return;
    if (m_textItem->document()->isEmpty())
        return;

    prepareGeometryChange();
    m_rect = m_rect.normalized();
    if (m_rect.width() < kMinWidth) {
        m_rect.setWidth(kMinWidth);
    }
    if (m_rect.height() < kMinHeight) {
        m_rect.setHeight(kMinHeight);
    }

    qreal textWidth = qMax<qreal>(10.0, m_rect.width() - 2 * kPadding);
    m_textItem->setTextWidth(textWidth);
    m_textItem->setPos(m_rect.left() + kPadding, m_rect.top() + kPadding);

    updateHandles();
    update();
}

// ======================================================
//  Funkcje pomocnicze dla uchwytów i wymiarów
// ======================================================
void CalloutItem::applyHandleDrag(const QPointF &delta) {
    switch (m_activeHandle) {
        case Handle::TopLeft:
            m_rect.setTopLeft(m_rect.topLeft() + delta);
            break;
        case Handle::TopRight:
            m_rect.setTopRight(m_rect.topRight() + delta);
            break;
        case Handle::BottomLeft:
            m_rect.setBottomLeft(m_rect.bottomLeft() + delta);
            break;
        case Handle::BottomRight:
            m_rect.setBottomRight(m_rect.bottomRight() + delta);
            break;
        default:
            break;
    }
    enforceMinimumSize(m_activeHandle);
}

void CalloutItem::enforceMinimumSize(Handle handle) {
    if (m_rect.width() < kMinWidth) {
        switch (handle) {
            case Handle::TopLeft:
            case Handle::BottomLeft:
                m_rect.setLeft(m_rect.right() - kMinWidth);
                break;
            case Handle::TopRight:
            case Handle::BottomRight:
                m_rect.setRight(m_rect.left() + kMinWidth);
                break;
            default:
                m_rect.setWidth(kMinWidth);
                break;
        }
    }
    if (m_rect.height() < kMinHeight) {
        switch (handle) {
            case Handle::TopLeft:
            case Handle::TopRight:
                m_rect.setTop(m_rect.bottom() - kMinHeight);
                break;
            case Handle::BottomLeft:
            case Handle::BottomRight:
                m_rect.setBottom(m_rect.top() + kMinHeight);
                break;
            default:
                m_rect.setHeight(kMinHeight);
                break;
        }
    }
    m_rect = m_rect.normalized();
}

void CalloutItem::updateHandles() {
    m_handles = {
        handleRect(Handle::TopLeft),
        handleRect(Handle::TopRight),
        handleRect(Handle::BottomLeft),
        handleRect(Handle::BottomRight),
        handleRect(Handle::Anchor)
    };
}

QRectF CalloutItem::handleRect(Handle h) const {
    QPointF center;
    switch (h) {
        case Handle::TopLeft:
            center = m_rect.topLeft();
            break;
        case Handle::TopRight:
            center = m_rect.topRight();
            break;
        case Handle::BottomLeft:
            center = m_rect.bottomLeft();
            break;
        case Handle::BottomRight:
            center = m_rect.bottomRight();
            break;
        case Handle::Anchor:
            center = mapFromScene(m_anchorScenePos);
            break;
        default:
            return QRectF();
    }
    return QRectF(center.x() - kHandleSize / 2, center.y() - kHandleSize / 2, kHandleSize, kHandleSize);
}

CalloutItem::Handle CalloutItem::handleAt(const QPointF &pos) const {
    const std::array<Handle, 5> order = {
        Handle::TopLeft,
        Handle::TopRight,
        Handle::BottomLeft,
        Handle::BottomRight,
        Handle::Anchor
    };
    for (size_t i = 0; i < m_handles.size(); ++i) {
        if (m_handles[i].contains(pos)) {
            return order[i];
        }
    }
    return Handle::None;
}

// ======================================================
//  Obliczanie grota dymka
// ======================================================
QPointF CalloutItem::calculateEdgeIntersection(const QPointF &anchorItemPos) const {
    QPointF center = m_rect.center();
    QPointF direction = anchorItemPos - center;
    if (m_rect.contains(anchorItemPos)) {
        direction = QPointF(0.0, 1.0);
    } else if (qAbs(direction.x()) < 0.001 && qAbs(direction.y()) < 0.001) {
        direction = QPointF(0.0, 1.0);
    }
    QLineF ray(center, center + direction);
    ray.setLength(qMax(m_rect.width(), m_rect.height()) * 2.0);
    QPointF intersect;
    qreal closestDistance = std::numeric_limits<qreal>::max();
    const QLineF sides[4] = {
        QLineF(m_rect.topLeft(), m_rect.topRight()),
        QLineF(m_rect.topRight(), m_rect.bottomRight()),
        QLineF(m_rect.bottomRight(), m_rect.bottomLeft()),
        QLineF(m_rect.bottomLeft(), m_rect.topLeft())
    };
    for (const auto &side : sides) {
        QPointF point;
        if (ray.intersects(side, &point) == QLineF::BoundedIntersection) {
            qreal distance = QLineF(center, point).length();
            if (distance < closestDistance) {
                closestDistance = distance;
                intersect = point;
            }
        }
    }
    return intersect.isNull() ? center : intersect;
}

// ======================================================
//  Settery, edycja, zdarzenia myszki
// ======================================================
void CalloutItem::setTextColor(const QColor &color) {
    m_textColor = color;
    if (m_textItem) m_textItem->setDefaultTextColor(color);
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
    if (m_textItem) m_textItem->setFont(font);
    if (scene()) updateTextLayout();
}

void CalloutItem::setAnchorPos(const QPointF &scenePos) {
    prepareGeometryChange();
    m_anchorScenePos = scenePos;
    updateHandles();
    update();
}

void CalloutItem::startEditing() {
    m_editing = true;
    if (m_textItem) {
        m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
        m_textItem->setFocus();
    }
}

void CalloutItem::finishEditing() {
    m_editing = false;
    if (m_textItem) {
        m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
    }
    if (scene()) updateTextLayout();
    emit editingFinished(this);
}

bool CalloutItem::isEditing() const {
    return m_editing;
}

// ======================================================
//  Obsługa zdarzeń myszki
// ======================================================
void CalloutItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    Q_UNUSED(event)
    if (!m_editing) startEditing();
    else finishEditing();
}

void CalloutItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    m_activeHandle = handleAt(event->pos());
    if (m_activeHandle != Handle::None) {
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent(event);
}

void CalloutItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_activeHandle == Handle::None) {
        QGraphicsItem::mouseMoveEvent(event);
        return;
    }

    if (m_activeHandle == Handle::Anchor) {
        prepareGeometryChange();
        m_anchorScenePos = mapToScene(event->pos());
        updateHandles();
        update();
        return;
    }

    QPointF delta = event->pos() - event->lastPos();
    applyHandleDrag(delta);
    if (scene()) updateTextLayout();
}

void CalloutItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    m_activeHandle = Handle::None;
    QGraphicsItem::mouseReleaseEvent(event);
}

void CalloutItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    Handle h = handleAt(event->pos());
    switch (h) {
        case Handle::TopLeft:
        case Handle::BottomRight:
            setCursor(QCursor(Qt::SizeFDiagCursor));
            break;
        case Handle::TopRight:
        case Handle::BottomLeft:
            setCursor(QCursor(Qt::SizeBDiagCursor));
            break;
        case Handle::Anchor:
            setCursor(QCursor(Qt::CrossCursor));
            break;
        default:
            unsetCursor();
            break;
    }
    QGraphicsItem::hoverMoveEvent(event);
}
