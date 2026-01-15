#include "FavoritesDockWidget.h"
#include <QToolBar>
#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QContextMenuEvent>
#include <QMenu>


/*
 * Implementacja FavoritesDockWidget.
 * W konstruktorze tworzymy pasek narzędzi (QToolBar) oraz dodajemy kilka
 * przykładowych akcji. Metoda addFavoriteAction pozwala dodać nowe akcje
 * z zewnątrz, wskazując odbiorcę i slot (np. metodę MainWindow
 * aktywującą narzędzie). W późniejszym etapie można dodać edycję paska
 * z poziomu interfejsu użytkownika.
 */
FavoritesDockWidget::FavoritesDockWidget(QWidget* parent)
    : QDockWidget(tr("Ulubione"), parent) {
    m_toolbar = new QToolBar(this);
    // Aby przeciągnięte akcje (bez ikon) wyświetlały tekst zamiast
    // pustych kwadratów, ustaw styl przycisków na wyświetlanie tylko
    // tekstu.  Domyślnie QToolBar oczekuje ikon, co powoduje, że
    // akcje bez ikon są renderowane jako puste kwadraty.
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    // Pozwól użytkownikowi zmieniać kolejność ulubionych poprzez przeciąganie
    m_toolbar->setMovable(true);
    setWidget(m_toolbar);
    // Zezwól na przeciąganie danych na pasek – w szczególności nazw narzędzi
    setAcceptDrops(true);
    // Domyślnie nie dodajemy żadnych akcji.  Użytkownik może dodać je
    // przeciągając element z ToolsDockWidget lub za pomocą funkcji addFavoriteAction().
}

void FavoritesDockWidget::addFavoriteAction(const QString& name, const QObject* receiver, const char* slot) {
    QAction* act = new QAction(name, this);
    connect(act, SIGNAL(triggered()), receiver, slot);
    m_toolbar->addAction(act);
}

void FavoritesDockWidget::setDefaultActionReceiver(QObject* receiver, const char* slot) {
    m_defaultReceiver = receiver;
    m_defaultSlot = slot;
}

// Przyjmujemy przeciągnięcia tylko z tekstem (nazwą narzędzia)
void FavoritesDockWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText() && m_defaultReceiver) {
        event->acceptProposedAction();
    } else {
        QDockWidget::dragEnterEvent(event);
    }
}

void FavoritesDockWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasText() && m_defaultReceiver) {
        const QString name = event->mimeData()->text();
        QAction* act = new QAction(name, this);
        connect(act, SIGNAL(triggered()), m_defaultReceiver, m_defaultSlot);
        m_toolbar->addAction(act);
        event->acceptProposedAction();
    } else {
        QDockWidget::dropEvent(event);
    }
}

// Umożliwiamy usuwanie akcji z paska poprzez menu kontekstowe.  PPM na
// pasku otworzy menu dla akcji pod kursorem z opcją Usuń.
void FavoritesDockWidget::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_toolbar) {
        QDockWidget::contextMenuEvent(event);
        return;
    }
    QAction* act = m_toolbar->actionAt(event->pos());
    if (act) {
        QMenu menu(this);
        QAction* removeAct = menu.addAction(tr("Usuń"));
        QAction* chosen = menu.exec(event->globalPos());
        if (chosen == removeAct) {
            m_toolbar->removeAction(act);
            act->deleteLater();
        }
    } else {
        QDockWidget::contextMenuEvent(event);
    }
}
