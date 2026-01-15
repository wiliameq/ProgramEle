#pragma once

#include <QDockWidget>
#include <QStringList>
// Forward declarations
class QTreeWidget;
class ToolsTreeWidget;
class QPushButton;

class QTreeWidget;
class QTreeWidgetItem;

/*
 * ToolsDockWidget
 * -----------------
 * Ten dokowalny widżet zawiera listę kategorii narzędzi i elementów, które
 * użytkownik może wykorzystać w projekcie. Wykorzystuje QTreeWidget do
 * prezentacji hierarchicznej: główne gałęzie odpowiadają kategoriom (np.
 * "Budynek", "Instalacje elektryczne"), a ich dzieci to poszczególne
 * elementy (np. "Ściana", "Drzwi", "Gniazdo" itd.). Po kliknięciu w
 * element końcowy (liść) wysyłany jest sygnał toolSelected z nazwą
 * narzędzia, dzięki czemu aplikacja może aktywować odpowiedni moduł.
 * Każde narzędzie powinno być osobnym modułem implementującym interfejs
 * narzędzia (np. ITool), a to drzewo służy jedynie do wyboru.
 */
class ToolsDockWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit ToolsDockWidget(QWidget* parent = nullptr);
signals:
    // Emitted when a leaf node (konkretny element) is clicked.
    void toolSelected(const QString& toolName);

private:
    // Internal representation of a category in the tools tree.  Each category
    // has a name, a list of item names (tools) and a visibility flag.  This
    // structure allows easy reordering and hiding of categories in the
    // settings dialog without rebuilding the entire tree manually.
    struct Category {
        QString name;
        QStringList items;
        bool visible = true;
    };

    /**
     * Rebuilds the QTreeWidget based on the current contents of m_categories.
     * Only categories with visible=true are added.  Categories are added
     * in the order stored in m_categories.  Each category is expanded
     * automatically.
     */
    void rebuildTree();

    /**
     * Shows a modal dialog allowing the user to reorder categories and
     * toggle their visibility.  When the dialog is accepted, m_categories
     * is updated and the tree is rebuilt.
     */
    void editToolSettings();

    // Button that opens the settings dialog.  Placed above the tree.
    QPushButton* m_settingsBtn = nullptr;

    // List of tool categories.  The order in this list determines the order
    // of categories in the tree.  Each entry contains the category name,
    // a list of items, and whether the category is visible.
    QList<Category> m_categories;

    // The underlying tree widget showing categories and tools.  We use a
    // custom subclass (ToolsTreeWidget) so that drag operations emit the
    // tool name as plain text; this allows dragging tools into
    // FavoritesDockWidget as actions.  See ToolsDockWidget.cpp for the
    // implementation of ToolsTreeWidget.
    ToolsTreeWidget* m_tree = nullptr;

    // Helper to populate initial categories.  This fills m_categories
    // with default values on first construction.
    void populateInitialCategories();
};
