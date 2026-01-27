#include "MeasurementsTool.h"

#include "Dialogs.h"
#include "Settings.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
double safePixelsPerMeter(double pixelsPerMeter, double zoom) {
    double value = pixelsPerMeter * zoom;
    if (value <= 0.0) {
        return 1.0;
    }
    return value;
}
} // namespace

MeasurementsTool::MeasurementsTool(ToolHost* host, std::function<void()> onFinished)
    : m_host(host), m_onFinished(std::move(onFinished)) {
    if (m_host && m_host->settings()) {
        m_currentColor = m_host->settings()->defaultMeasureColor;
        m_currentLineWidth = m_host->settings()->lineWidthPx;
    }
}

QString MeasurementsTool::name() const { return QStringLiteral("Pomiary"); }

QString MeasurementsTool::layerName() const { return QStringLiteral("Pomiary"); }

bool MeasurementsTool::isActive() const { return m_mode != Mode::None; }

void MeasurementsTool::activate() { }

void MeasurementsTool::deactivate() {
    cancelCurrentMeasure();
    m_mode = Mode::None;
}

void MeasurementsTool::draw(QPainter& p) {
    if (!m_visible || !m_host) return;
    if (!m_host->isLayerVisible(layerName())) return;
    p.setRenderHint(QPainter::Antialiasing, true);
    for (size_t idx = 0; idx < m_measures.size(); ++idx) {
        const auto &m = m_measures[idx];
        if (!m.visible || !m_host->isLayerVisible(m.layer)) continue;
        if (m.pts.size() < 2) continue;
        QPen pen(m.color);
        pen.setWidth(m.lineWidthPx);
        pen.setCosmetic(true);
        p.setPen(pen);
        for (size_t i = 1; i < m.pts.size(); ++i) {
            p.drawLine(m.pts[i - 1], m.pts[i]);
        }
        QPointF labelPos = m.pts.back();
        QString text = fmtLenInProjectUnit(m.totalWithBufferMeters);
        QFontMetrics fm(p.font());
        int textW = fm.horizontalAdvance(text) + 10;
        int textH = fm.height() + 4;
        QRectF box(labelPos + QPointF(8, -textH - 4), QSizeF(textW, textH));
        p.setPen(QPen(Qt::black));
        p.fillRect(box, QColor(255,255,255,200));
        p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        const auto &mSel = m_measures[m_selectedMeasureIndex];
        if (mSel.visible && m_host->isLayerVisible(mSel.layer) && mSel.pts.size() >= 2) {
            QPen pen(Qt::black);
            pen.setWidth(mSel.lineWidthPx + 2);
            pen.setStyle(Qt::DashLine);
            pen.setCosmetic(true);
            p.setPen(pen);
            for (size_t i = 1; i < mSel.pts.size(); ++i) {
                p.drawLine(mSel.pts[i - 1], mSel.pts[i]);
            }
        }
    }
}

void MeasurementsTool::drawOverlay(QPainter& p, bool hasMouseWorld, const QPointF& mouseWorld) {
    if (!isActive()) return;
    if (!m_visible || !m_host) return;
    if (m_currentPts.empty()) return;
    QPen pen(Qt::DashLine);
    pen.setColor(m_currentColor);
    pen.setWidth(m_currentLineWidth);
    pen.setCosmetic(true);
    p.setPen(pen);
    for (size_t i = 1; i < m_currentPts.size(); ++i) {
        p.drawLine(m_currentPts[i - 1], m_currentPts[i]);
    }
    double L = polyLengthCm(m_currentPts);
    if (hasMouseWorld) {
        p.drawLine(m_currentPts.back(), mouseWorld);
        double dx = mouseWorld.x() - m_currentPts.back().x();
        double dy = mouseWorld.y() - m_currentPts.back().y();
        L += std::hypot(dx, dy) / safePixelsPerMeter(m_host->pixelsPerMeter(), 1.0);
    }
    QPointF at = hasMouseWorld ? mouseWorld : m_currentPts.back();
    QString text = fmtLenInProjectUnit(L);
    QFontMetrics fm(p.font());
    int textW = fm.horizontalAdvance(text) + 10;
    int textH = fm.height() + 4;
    QRectF box(at + QPointF(8, -textH - 4), QSizeF(textW, textH));
    p.setPen(QPen(Qt::black));
    p.fillRect(box, QColor(255,255,255,200));
    p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
}

bool MeasurementsTool::mousePress(QMouseEvent* event) {
    if (!m_host) return false;
    if (event->button() != Qt::LeftButton) return false;
    if (!isActive()) return false;
    QPointF pos = m_host->toWorld(event->position());
    if (m_mode == Mode::Linear) {
        m_currentPts.push_back(pos);
        m_redoPts.clear();
        if (m_currentPts.size() == 2) {
            finishCurrentMeasure();
        }
        m_host->requestUpdate();
        return true;
    }
    if (m_mode == Mode::Polyline || m_mode == Mode::Advanced) {
        m_currentPts.push_back(pos);
        m_redoPts.clear();
        m_host->requestUpdate();
        return true;
    }
    return false;
}

bool MeasurementsTool::mouseMove(QMouseEvent* event, const QPointF& worldPos) {
    Q_UNUSED(event);
    Q_UNUSED(worldPos);
    if (!isActive() || !m_host) return false;
    m_host->requestUpdate();
    return false;
}

bool MeasurementsTool::mouseRelease(QMouseEvent* event) {
    Q_UNUSED(event);
    return false;
}

bool MeasurementsTool::mouseDoubleClick(QMouseEvent* event) {
    Q_UNUSED(event);
    return false;
}

bool MeasurementsTool::keyPress(QKeyEvent* event, QWidget* parentForDialogs) {
    if (!isActive()) return false;
    if (!m_host) return false;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_mode == Mode::Polyline || m_mode == Mode::Advanced) {
            finishCurrentMeasure(parentForDialogs);
            return true;
        }
        return false;
    case Qt::Key_Backspace:
        if (!m_currentPts.empty()) {
            m_currentPts.pop_back();
            m_host->requestUpdate();
            return true;
        }
        return false;
    case Qt::Key_Escape:
        cancelCurrentMeasure();
        return true;
    default:
        return false;
    }
}

void MeasurementsTool::setVisible(bool visible) {
    m_visible = visible;
    if (m_host) {
        m_host->requestUpdate();
    }
}

bool MeasurementsTool::isVisible() const { return m_visible; }

void MeasurementsTool::startLinear() {
    m_mode = Mode::Linear;
    m_currentPts.clear();
    m_redoPts.clear();
    m_selectedMeasureIndex = -1;
    if (m_host && m_host->settings()) {
        m_currentColor = m_host->settings()->defaultMeasureColor;
        m_currentLineWidth = m_host->settings()->lineWidthPx;
    }
}

void MeasurementsTool::startPolyline() {
    m_mode = Mode::Polyline;
    m_currentPts.clear();
    m_redoPts.clear();
    m_selectedMeasureIndex = -1;
    if (m_host && m_host->settings()) {
        m_currentColor = m_host->settings()->defaultMeasureColor;
        m_currentLineWidth = m_host->settings()->lineWidthPx;
    }
}

void MeasurementsTool::startAdvanced(QWidget* parent) {
    if (!m_host) return;
    AdvancedMeasureDialog dlg(parent, m_host->settings());
    if (dlg.exec() != QDialog::Accepted) return;
    m_advTemplate = Measure{};
    m_advTemplate.type = MeasureType::Advanced;
    m_advTemplate.name = dlg.name();
    m_advTemplate.color = dlg.color();
    m_advTemplate.unit = QStringLiteral("cm");
    m_advTemplate.bufferDefaultMeters = dlg.bufferValue();
    m_currentColor = dlg.color();
    if (m_host->settings()) {
        m_currentLineWidth = m_host->settings()->lineWidthPx;
    }
    m_advTemplate.lineWidthPx = m_currentLineWidth;
    m_mode = Mode::Advanced;
    m_currentPts.clear();
    m_selectedMeasureIndex = -1;
}

void MeasurementsTool::cancelCurrentMeasure() {
    m_currentPts.clear();
    m_redoPts.clear();
    m_mode = Mode::None;
    m_selectedMeasureIndex = -1;
    if (m_host) {
        m_host->requestUpdate();
    }
}

void MeasurementsTool::undoCurrentMeasure() {
    if (!m_currentPts.empty()) {
        m_redoPts.push_back(m_currentPts.back());
        m_currentPts.pop_back();
        if (m_host) {
            m_host->requestUpdate();
        }
    }
}

void MeasurementsTool::redoCurrentMeasure() {
    if (!m_redoPts.empty()) {
        m_currentPts.push_back(m_redoPts.back());
        m_redoPts.pop_back();
        if (m_host) {
            m_host->requestUpdate();
        }
    }
}

void MeasurementsTool::confirmCurrentMeasure(QWidget* parentForAdvanced) {
    finishCurrentMeasure(parentForAdvanced);
}

void MeasurementsTool::openReportDialog(QWidget* parent) {
    if (!m_host) return;
    ReportDialog dlg(parent, m_host->settings(), &m_measures);
    dlg.exec();
    m_host->requestUpdate();
}

QColor MeasurementsTool::currentColor() const { return m_currentColor; }

void MeasurementsTool::setCurrentColor(const QColor& c) {
    m_currentColor = c;
    if (m_mode == Mode::Advanced) {
        m_advTemplate.color = c;
    }
    if (m_host) {
        m_host->requestUpdate();
    }
}

int MeasurementsTool::currentLineWidth() const { return m_currentLineWidth; }

void MeasurementsTool::setCurrentLineWidth(int w) {
    m_currentLineWidth = qBound(1, w, 8);
    if (m_mode == Mode::Advanced) {
        m_advTemplate.lineWidthPx = m_currentLineWidth;
    }
    if (m_host) {
        m_host->requestUpdate();
    }
}

bool MeasurementsTool::hasAnyMeasure() const { return !m_measures.empty(); }

void MeasurementsTool::updateAllMeasureColors(const QColor& color) {
    for (auto &m : m_measures) {
        m.color = color;
    }
    if (m_host) {
        m_host->requestUpdate();
    }
}

void MeasurementsTool::updateAllMeasureLineWidths(int width) {
    int bounded = qBound(1, width, 8);
    for (auto &m : m_measures) {
        m.lineWidthPx = bounded;
    }
    if (m_host) {
        m_host->requestUpdate();
    }
}

void MeasurementsTool::recalculateLengths() {
    for (auto &m : m_measures) {
        m.lengthMeters = polyLengthCm(m.pts);
        m.totalWithBufferMeters = m.lengthMeters + m.bufferGlobalMeters
            + m.bufferDefaultMeters + m.bufferFinalMeters;
    }
    if (m_host) {
        m_host->requestUpdate();
    }
}

void MeasurementsTool::scaleAllPoints(double factor) {
    if (factor == 1.0) {
        return;
    }
    for (auto &m : m_measures) {
        for (auto &pt : m.pts) {
            pt.setX(pt.x() * factor);
            pt.setY(pt.y() * factor);
        }
    }
    for (auto &pt : m_currentPts) {
        pt.setX(pt.x() * factor);
        pt.setY(pt.y() * factor);
    }
    for (auto &pt : m_redoPts) {
        pt.setX(pt.x() * factor);
        pt.setY(pt.y() * factor);
    }
    for (auto &pt : m_advTemplate.pts) {
        pt.setX(pt.x() * factor);
        pt.setY(pt.y() * factor);
    }
    recalculateLengths();
}

void MeasurementsTool::scaleContent(double factor) {
    scaleAllPoints(factor);
}

QColor MeasurementsTool::selectedMeasureColor() const {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        return m_measures[m_selectedMeasureIndex].color;
    }
    return QColor();
}

int MeasurementsTool::selectedMeasureLineWidth() const {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        return m_measures[m_selectedMeasureIndex].lineWidthPx;
    }
    return 1;
}

void MeasurementsTool::setSelectedMeasureColor(const QColor& c) {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        m_measures[m_selectedMeasureIndex].color = c;
        if (m_host) {
            m_host->requestUpdate();
        }
    }
}

void MeasurementsTool::setSelectedMeasureLineWidth(int w) {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        int bounded = qBound(1, w, 8);
        m_measures[m_selectedMeasureIndex].lineWidthPx = bounded;
        if (m_host) {
            m_host->requestUpdate();
        }
    }
}

void MeasurementsTool::deleteSelectedMeasure() {
    if (m_selectedMeasureIndex >= 0 && m_selectedMeasureIndex < (int)m_measures.size()) {
        m_measures.erase(m_measures.begin() + m_selectedMeasureIndex);
        m_selectedMeasureIndex = -1;
        if (m_host) {
            m_host->requestUpdate();
        }
    }
}

void MeasurementsTool::clearSelection() {
    m_selectedMeasureIndex = -1;
    if (m_host) {
        m_host->requestUpdate();
    }
}

bool MeasurementsTool::selectMeasureAt(const QPointF& worldPos, double thresholdWorld) {
    int idx = -1;
    double bestDist = thresholdWorld;
    for (size_t i = 0; i < m_measures.size(); ++i) {
        const auto &m = m_measures[i];
        if (!m.visible || m.pts.size() < 2) continue;
        for (size_t j = 1; j < m.pts.size(); ++j) {
            QPointF a = m.pts[j - 1];
            QPointF b = m.pts[j];
            QPointF ab = b - a;
            double ab2 = ab.x()*ab.x() + ab.y()*ab.y();
            if (ab2 == 0.0) continue;
            double t = ((worldPos - a).x()*ab.x() + (worldPos - a).y()*ab.y()) / ab2;
            t = std::max(0.0, std::min(1.0, t));
            QPointF proj = a + t * ab;
            double dx = proj.x() - worldPos.x();
            double dy = proj.y() - worldPos.y();
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist <= bestDist) {
                bestDist = dist;
                idx = (int)i;
            }
        }
    }
    m_selectedMeasureIndex = idx;
    if (m_host) {
        m_host->requestUpdate();
    }
    return idx >= 0;
}

int MeasurementsTool::selectedMeasureIndex() const { return m_selectedMeasureIndex; }

const std::vector<Measure>& MeasurementsTool::measures() const { return m_measures; }

double MeasurementsTool::polyLengthCm(const std::vector<QPointF>& pts) const {
    if (pts.size() < 2) return 0.0;
    double px = 0.0;
    for (size_t i = 1; i < pts.size(); ++i) {
        const double dx = pts[i].x() - pts[i-1].x();
        const double dy = pts[i].y() - pts[i-1].y();
        px += std::hypot(dx, dy);
    }
    return px / safePixelsPerMeter(m_host ? m_host->pixelsPerMeter() : 1.0, 1.0);
}

QString MeasurementsTool::fmtLenInProjectUnit(double m) const {
    if (!m_host || !m_host->settings()) {
        return QString("%1 cm").arg(m, 0, 'f', 2);
    }
    return QString("%1 cm").arg(m, 0, 'f', m_host->settings()->decimals);
}

void MeasurementsTool::finishCurrentMeasure(QWidget* parentForAdvanced) {
    if (!m_host) return;
    if (m_currentPts.size() < 2) {
        m_currentPts.clear();
        m_mode = Mode::None;
        m_host->requestUpdate();
        return;
    }
    Measure mm;
    mm.createdAt = QDateTime::currentDateTime();
    mm.pts = m_currentPts;
    if (m_mode == Mode::Linear) {
        mm.type = MeasureType::Linear;
        mm.unit = QStringLiteral("cm");
        mm.color = m_currentColor;
        mm.lineWidthPx = m_currentLineWidth;
        mm.bufferGlobalMeters  = 0.0;
        mm.bufferDefaultMeters = 0.0;
        mm.bufferFinalMeters   = 0.0;
    } else if (m_mode == Mode::Polyline) {
        mm.type = MeasureType::Polyline;
        mm.unit = QStringLiteral("cm");
        mm.color = m_currentColor;
        mm.lineWidthPx = m_currentLineWidth;
        mm.bufferGlobalMeters  = 0.0;
        mm.bufferDefaultMeters = 0.0;
        mm.bufferFinalMeters   = 0.0;
    } else {
        mm = m_advTemplate;
        mm.createdAt = QDateTime::currentDateTime();
        mm.pts = m_currentPts;
        mm.bufferGlobalMeters = 0.0;
        FinalBufferDialog fd(parentForAdvanced, m_host->settings());
        if (fd.exec() == QDialog::Accepted) {
            double val = fd.bufferValue();
            mm.bufferFinalMeters = val;
        } else {
            mm.bufferFinalMeters = 0.0;
        }
    }
    mm.id = m_nextId++;
    if (mm.name.isEmpty()) mm.name = QString("Pomiar %1").arg(mm.id);
    mm.lengthMeters = polyLengthCm(mm.pts);
    mm.totalWithBufferMeters = mm.lengthMeters + mm.bufferGlobalMeters + mm.bufferDefaultMeters + mm.bufferFinalMeters;
    m_measures.push_back(mm);
    m_currentPts.clear();
    m_mode = Mode::None;
    m_host->requestUpdate();
    if (m_onFinished) {
        m_onFinished();
    }
}
