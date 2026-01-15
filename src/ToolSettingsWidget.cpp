#include "ToolSettingsWidget.h"
#include <QStackedWidget>
#include <QLabel>
#include <QVBoxLayout>

/*
 * Implementacja ToolSettingsWidget.
 * Panel tworzy QStackedWidget z pierwszą stroną jako placeholder.
 * Funkcja setSettingsWidget przyjmuje nowy widżet, usuwa poprzedni (jeśli istnieje)
 * i ustawia jako aktualny. Pozwala to na dynamiczne przełączanie się między
 * różnymi zestawami opcji w zależności od wybranego narzędzia.
 */
ToolSettingsWidget::ToolSettingsWidget(QWidget* parent)
    : QDockWidget(tr("Ustawienia narzędzia"), parent) {
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    m_stack = new QStackedWidget(container);
    // Placeholder – informuje, że brak aktywnego narzędzia
    QLabel* placeholder = new QLabel(tr("Brak aktywnego narzędzia"), container);
    placeholder->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(placeholder);
    layout->addWidget(m_stack);
    container->setLayout(layout);
    setWidget(container);
}

QWidget* ToolSettingsWidget::currentSettings() const {
    return m_stack->currentWidget();
}

void ToolSettingsWidget::setSettingsWidget(QWidget* widget) {
    // Usuń poprzedni widżet, jeżeli istnieje poza placeholderem (index 0)
    if (m_stack->count() > 1) {
        QWidget* old = m_stack->widget(1);
        m_stack->removeWidget(old);
        old->deleteLater();
    }
    if (widget) {
        m_stack->addWidget(widget);
        m_stack->setCurrentWidget(widget);
    } else {
        // Przywróć placeholder
        m_stack->setCurrentIndex(0);
    }
}
