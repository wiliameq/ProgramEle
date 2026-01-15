#pragma once

#include <QDockWidget>

class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

/*
 * ProjectNavigatorWidget
 * ----------------------
 * Ten dokowalny widżet umożliwia wybór aktualnego kontekstu projektowego
 * (np. budynek, piętro) oraz prezentuje strukturę elementów przypisanych
 * do wybranego obiektu. W górnej części znajduje się rozwijana lista
 * wyboru (QComboBox) wraz z przyciskami dodawania i usuwania
 * obiektów. W dolnej części znajduje się drzewo kontekstowe (QTreeWidget),
 * którego gałęzie odpowiadają kategoriom (np. "Budynek", "Instalacje
 * elektryczne"), a liście – warstwom lub podkategoriom. Kliknięcie w liść
 * powoduje emisję sygnału contextToolSelected z nazwą kategorii i elementu.
 */
class ProjectNavigatorWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit ProjectNavigatorWidget(QWidget* parent = nullptr);
signals:
    // Zmiana aktualnego obiektu (np. budynku lub piętra)
    void currentObjectChanged(const QString& objectName);
    // Kliknięcie w element (liść) w drzewie kontekstowym
    void contextToolSelected(const QString& category, const QString& item);

private:
    QComboBox* m_selector = nullptr;
    QTreeWidget* m_tree = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_delBtn = nullptr;
    void populateTree();
};
