#include "ToolsDockWidget.h"
#include <QTreeWidget>
#include <QMimeData>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDialog>
#include <QListWidget>
#include <QHBoxLayout>
#include <QLabel>

// Definicja pomocniczej klasy ToolsTreeWidget.  Ta klasa dziedziczy
// z QTreeWidget i nadpisuje metody mimeData() oraz mimeTypes() tak,
// aby podczas przeciągania liści narzędzi przekazywać nazwę
// narzędzia jako zwykły tekst (text/plain).  Dzięki temu
// FavoritesDockWidget może interpretować przeciągnięte narzędzie i
// utworzyć odpowiednią akcję.
class ToolsTreeWidget : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    // Używamy dokładnie takiego samego podpisu jak w QTreeWidget,
    // aby nie powodować problemów z override w MSVC.  Metoda przyjmuje
    // listę wskaźników do QTreeWidgetItem bez referencji.  Nie używamy
    // słowa kluczowego override, ponieważ sygnatura w niektórych
    // wersjach Qt może się różnić (np. brak const na liście).  Brak
    // specyfikatora override zapobiega błędom C3668 na MSVC.
    QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const
    {
        QMimeData* data = new QMimeData();
        if (items.size() == 1) {
            QTreeWidgetItem* item = items.first();
            // Przekazuj nazwy tylko liści (bez dzieci) z rodzicem
            if (item && item->childCount() == 0 && item->parent()) {
                data->setText(item->text(0));
            }
        }
        return data;
    }
    QStringList mimeTypes() const
    {
        return { QStringLiteral("text/plain") };
    }
};

/*
 * Implementacja ToolsDockWidget
 * ----------------------------
 * Ten panel dokowalny wyświetla drzewo kategorii z narzędziami oraz
 * przycisk „Ustawienia”, który otwiera dialog pozwalający na
 * dostosowanie kolejności i widoczności kategorii.  Kategorie są
 * przechowywane w m_categories; rebuildTree() odtwarza drzewo na
 * podstawie tego wektora.  Gdy użytkownik kliknie liść drzewa,
 * emitowany jest sygnał toolSelected z nazwą narzędzia.
 */
ToolsDockWidget::ToolsDockWidget(QWidget* parent)
    : QDockWidget(tr("Narzędzia"), parent)
{
    // Kontener na zawartość docka
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Przycisk otwierający ustawienia kategorii
    m_settingsBtn = new QPushButton(tr("Ustawienia"), container);
    m_settingsBtn->setToolTip(tr("Dostosuj kolejność i widoczność kategorii"));
    layout->addWidget(m_settingsBtn);
    connect(m_settingsBtn, &QPushButton::clicked, this, &ToolsDockWidget::editToolSettings);

    // Drzewo narzędzi
    // Używamy niestandardowej klasy ToolsTreeWidget (definicja poniżej),
    // aby podczas przeciągania liścia narzędzia generować dane MIME w
    // postaci tekstu zawierającego nazwę narzędzia. Dzięki temu
    // FavoritesDockWidget może łatwo utworzyć akcję po upuszczeniu.
    m_tree = new ToolsTreeWidget(container);
    m_tree->setHeaderHidden(true);
    // Umożliwiamy przeciąganie elementów (drag) – tryb tylko drag, bez drop
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly);
    layout->addWidget(m_tree, 1);

    container->setLayout(layout);
    setWidget(container);

    // Inicjalizuj domyślne kategorie tylko raz
    populateInitialCategories();
    rebuildTree();

    // Reakcja na kliknięcie liścia
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int /*column*/) {
        if (!item) return;
        // Liść to element bez dzieci, który ma rodzica (kategoria)
        if (item->childCount() == 0 && item->parent()) {
            emit toolSelected(item->text(0));
        }
    });
}

// Tworzy domyślną listę kategorii wraz z elementami.  Kategorie są
// dodawane w kolejności odpowiadającej ich domyślnemu układowi.  Ten
// wektor jest wykorzystywany w rebuildTree() do odtwarzania drzewa.
void ToolsDockWidget::populateInitialCategories() {
    if (!m_categories.isEmpty()) return; // już wypełniono
    // Kolejność kategorii: Narzędzia, Budynek, Instalacje elektryczne,
    // Instalacje teletechniczne, Rozdzielnice.  Nazwy są tłumaczone
    // zgodnie z bieżącą lokalizacją.
    Category tools;
    tools.name = tr("Narzędzia");
    tools.items << tr("Zaznacz") << tr("Wstaw komentarz") << tr("Usuń");
    tools.visible = true;
    m_categories.append(tools);

    Category building;
    building.name = tr("Budynek");
    building.items << tr("Ściana") << tr("Drzwi") << tr("Okno");
    building.visible = true;
    m_categories.append(building);

    Category electrical;
    electrical.name = tr("Instalacje elektryczne");
    electrical.items << tr("Gniazdo") << tr("Przełącznik");
    electrical.visible = true;
    m_categories.append(electrical);

    Category teletech;
    teletech.name = tr("Instalacje teletechniczne");
    teletech.items << tr("Gniazdo RJ45");
    teletech.visible = true;
    m_categories.append(teletech);

    // W tej wersji usuwamy kategorię „Rozdzielnice”.  Zostanie ona dodana w przyszłości
    // jako osobny moduł.  Dlatego nie tworzymy kategorii boards.
}

// Przebudowuje drzewo na podstawie m_categories.  Najpierw czyści
// istniejące elementy, a następnie dodaje ponownie widoczne kategorie i ich
// elementy.  Każda kategoria jest rozszerzana domyślnie.
void ToolsDockWidget::rebuildTree() {
    if (!m_tree) return;
    m_tree->clear();
    for (const Category& cat : qAsConst(m_categories)) {
        if (!cat.visible) continue;
        auto* catItem = new QTreeWidgetItem(m_tree, QStringList() << cat.name);
        for (const QString& itemName : cat.items) {
            new QTreeWidgetItem(catItem, QStringList() << itemName);
        }
        catItem->setExpanded(true);
    }
    // Ensure the top-level items are expanded and selection resets
    m_tree->expandAll();
}

// Otwiera dialog konfiguracji, w którym użytkownik może zmienić kolejność
// kategorii oraz ich widoczność.  Po zatwierdzeniu wprowadzone zmiany
// są zapisywane i drzewo jest przebudowywane.
void ToolsDockWidget::editToolSettings() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Ustawienia narzędzi"));
    QVBoxLayout* vlay = new QVBoxLayout(&dlg);
    QLabel* info = new QLabel(tr("Przeciągnij kategorie lub elementy, aby zmienić kolejność. Zaznacz/odznacz kategorię, aby ją ukryć."), &dlg);
    info->setWordWrap(true);
    vlay->addWidget(info);

    // Używamy QTreeWidget, aby wyświetlić kategorie i ich elementy.  Drzewo
    // pozwala na przesuwanie zarówno kategorii (elementów najwyższego
    // poziomu), jak i poszczególnych elementów wewnątrz jednej kategorii.
    QTreeWidget* tree = new QTreeWidget(&dlg);
    tree->setHeaderHidden(true);
    tree->setDragEnabled(true);
    tree->setAcceptDrops(true);
    tree->setDropIndicatorShown(true);
    tree->setDragDropMode(QAbstractItemView::InternalMove);
    // Wypełnij drzewo na podstawie m_categories
    for (const Category& cat : qAsConst(m_categories)) {
        QTreeWidgetItem* catItem = new QTreeWidgetItem(QStringList() << cat.name);
        catItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        catItem->setCheckState(0, cat.visible ? Qt::Checked : Qt::Unchecked);
        tree->addTopLevelItem(catItem);
        for (const QString& itemName : cat.items) {
            QTreeWidgetItem* it = new QTreeWidgetItem(QStringList() << itemName);
            // Elementy można przeciągać i upuszczać.  Dodajemy
            // ItemIsDropEnabled, aby umożliwić zmianę kolejności w obrębie
            // kategorii poprzez przeciągnięcie.
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            catItem->addChild(it);
        }
        catItem->setExpanded(true);
    }
    vlay->addWidget(tree, 1);

    QHBoxLayout* btnLay = new QHBoxLayout;
    btnLay->addStretch();
    QPushButton* okBtn = new QPushButton(tr("OK"), &dlg);
    QPushButton* cancelBtn = new QPushButton(tr("Anuluj"), &dlg);
    btnLay->addWidget(okBtn);
    btnLay->addWidget(cancelBtn);
    vlay->addLayout(btnLay);

    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        // Odczytaj zmiany z drzewa: kolejność i widoczność kategorii
        QList<Category> newCats;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* catItem = tree->topLevelItem(i);
            if (!catItem) continue;
            QString catName = catItem->text(0);
            // Znajdź pierwotną kategorię, aby odzyskać listę elementów i ustawienia
            Category orig;
            bool found = false;
            for (const Category& c : qAsConst(m_categories)) {
                if (c.name == catName) { orig = c; found = true; break; }
            }
            // Jeżeli nie znaleziono (np. przeniesiono element do nowej kategorii), utwórz nową
            if (!found) {
                orig.name = catName;
            }
            orig.visible = (catItem->checkState(0) == Qt::Checked);
            // Odczytaj kolejność elementów w tej kategorii
            QStringList newItems;
            for (int j = 0; j < catItem->childCount(); ++j) {
                QTreeWidgetItem* child = catItem->child(j);
                if (!child) continue;
                newItems << child->text(0);
            }
            orig.items = newItems;
            newCats.append(orig);
        }
        m_categories = newCats;
        rebuildTree();
    }
}
