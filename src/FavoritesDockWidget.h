#pragma once

#include <QDockWidget>

class QToolBar;
class QAction;

/*
 * FavoritesDockWidget
 * -------------------
 * Panel (dokowalny) pozwalający użytkownikowi przypiąć najczęściej używane
 * narzędzia jako skróty. Bazuje na QToolBar umieszczonym wewnątrz
 * QDockWidgeta, co daje możliwość przenoszenia, zwijania i dokowania.
 * W przyszłości można dodać mechanizm drag-and-drop z listy narzędzi
 * lub konfigurowalny manager ulubionych narzędzi. W tej bazowej wersji
 * toolbar zapełniony jest przykładowymi akcjami.
 */
class FavoritesDockWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit FavoritesDockWidget(QWidget* parent = nullptr);
    // Dodaje akcję do paska ulubionych. callback powinien wskazywać na
    // funkcję aktywującą narzędzie. Dla uproszczenia korzystamy z lambdy
    // przy przekazywaniu.
    void addFavoriteAction(const QString& name, const QObject* receiver, const char* slot);

    /**
     * Ustawia odbiorcę i nazwę slotu, do którego będą podłączane akcje
     * utworzone przez upuszczenie narzędzi z panelu narzędzi.  Jeśli
     * parametr receiver jest nullptr, przeciąganie zostanie zignorowane.
     */
    void setDefaultActionReceiver(QObject* receiver, const char* slot);

protected:
    // Obsługa drag-and-drop: zezwalamy na przeciąganie tekstu (nazwy narzędzi)
    // na pasek.  Overriding these methods włącza tryb drag&drop i tworzy
    // nowe akcje z upuszczonych elementów.
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QToolBar* m_toolbar = nullptr;

    // Domyślny odbiorca i slot, używany do podłączania akcji tworzonych
    // poprzez przeciągnięcie narzędzi.  Gdy użytkownik upuści element
    // na pasku ulubionych, zostanie utworzona nowa QAction i połączona
    // z tym odbiorcą/slotem.
    QObject* m_defaultReceiver = nullptr;
    const char* m_defaultSlot = nullptr;
};
