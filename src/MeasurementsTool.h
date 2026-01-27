#pragma once

#include "Measurements.h"
#include "ToolModule.h"

#include <QColor>
#include <functional>
#include <vector>

class MeasurementsTool : public ToolModule {
public:
    enum class Mode { None, Linear, Polyline, Advanced };

    MeasurementsTool(ToolHost* host, std::function<void()> onFinished);

    QString name() const override;
    QString layerName() const override;
    bool isActive() const override;
    void activate() override;
    void deactivate() override;

    void draw(QPainter& painter) override;
    void drawOverlay(QPainter& painter, bool hasMouseWorld, const QPointF& mouseWorld) override;

    bool mousePress(QMouseEvent* event) override;
    bool mouseMove(QMouseEvent* event, const QPointF& worldPos) override;
    bool mouseRelease(QMouseEvent* event) override;
    bool mouseDoubleClick(QMouseEvent* event) override;
    bool keyPress(QKeyEvent* event, QWidget* parentForDialogs) override;

    void setVisible(bool visible);
    bool isVisible() const;

    void startLinear();
    void startPolyline();
    void startAdvanced(QWidget* parent);
    void cancelCurrentMeasure();
    void undoCurrentMeasure();
    void redoCurrentMeasure();
    void confirmCurrentMeasure(QWidget* parentForAdvanced = nullptr);
    void openReportDialog(QWidget* parent);

    QColor currentColor() const;
    void setCurrentColor(const QColor& c);
    int currentLineWidth() const;
    void setCurrentLineWidth(int w);

    bool hasAnyMeasure() const;
    void updateAllMeasureColors(const QColor& color);
    void updateAllMeasureLineWidths(int width);
    void recalculateLengths();
    void scaleAllPoints(double factor);

    QColor selectedMeasureColor() const;
    int selectedMeasureLineWidth() const;
    void setSelectedMeasureColor(const QColor& c);
    void setSelectedMeasureLineWidth(int w);
    void deleteSelectedMeasure();
    void clearSelection();

    void scaleContent(double factor) override;

    bool selectMeasureAt(const QPointF& worldPos, double thresholdWorld);
    int selectedMeasureIndex() const;

    const std::vector<Measure>& measures() const;

private:
    double polyLengthCm(const std::vector<QPointF>& pts) const;
    QString fmtLenInProjectUnit(double m) const;
    void finishCurrentMeasure(QWidget* parentForAdvanced = nullptr);

    ToolHost* m_host = nullptr;
    std::function<void()> m_onFinished;

    bool m_visible = true;
    Mode m_mode = Mode::None;
    int m_nextId = 1;
    std::vector<Measure> m_measures;
    std::vector<QPointF> m_currentPts;
    Measure m_advTemplate;
    std::vector<QPointF> m_redoPts;

    int m_selectedMeasureIndex = -1;
    QColor m_currentColor;
    int m_currentLineWidth = 1;
};
