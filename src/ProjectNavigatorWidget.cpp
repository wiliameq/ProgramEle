#include "ProjectNavigatorWidget.h"
#include <QComboBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

/*
 * Implementacja ProjectNavigatorWidget.
 * Drzewo kontekstowe jest zapełniane przykładowymi kategoriami i warstwami.
 * Możliwość dodawania i usuwania obiektów (np. budynków, pięter) jest
 * zasygnalizowana przyciskami, ale szczegółowe działanie tych przycisków
 * pozostawiono do implementacji w przyszłości.
 */
ProjectNavigatorWidget::ProjectNavigatorWidget(QWidget* parent)
    : QDockWidget(tr("Projekt"), parent) {
    QWidget* container = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(2, 2, 2, 2);

    // Górny pasek z listą wyboru obiektu oraz przyciskami dodaj/usuń
    QHBoxLayout* topLayout = new QHBoxLayout();
    m_selector = new QComboBox(container);
    // Dodaj przykładowe obiekty; w przyszłości mogą być wczytywane dynamicznie
    m_selector->addItem(tr("Budynek 1"));
    m_selector->addItem(tr("Piętro 1"));
    m_addBtn = new QPushButton(tr("+"), container);
    m_delBtn = new QPushButton(tr("–"), container);
    topLayout->addWidget(m_selector);
    topLayout->addWidget(m_addBtn);
    topLayout->addWidget(m_delBtn);
    mainLayout->addLayout(topLayout);

    // Drzewo kontekstowe
    m_tree = new QTreeWidget(container);
    m_tree->setHeaderHidden(true);
    populateTree();
    mainLayout->addWidget(m_tree);
    container->setLayout(mainLayout);
    setWidget(container);

    // Sygnał zmiany aktualnego obiektu
    connect(m_selector, &QComboBox::currentTextChanged,
            this, &ProjectNavigatorWidget::currentObjectChanged);

    // Kliknięcie w element drzewa: jeśli liść, emituj contextToolSelected
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) {
                if (!item) return;
                if (item->childCount() == 0 && item->parent()) {
                    emit contextToolSelected(item->parent()->text(0), item->text(0));
                }
            });
    // Przykładowe obsługi klików przycisków; szczegóły należy uzupełnić
    connect(m_addBtn, &QPushButton::clicked, this, [this]() {
        // TODO: implementacja dodawania nowego obiektu (budynku/piętra)
    });
    connect(m_delBtn, &QPushButton::clicked, this, [this]() {
        // TODO: implementacja usuwania aktualnie wybranego obiektu
    });
}

void ProjectNavigatorWidget::populateTree() {
    // Wyczyść bieżące drzewo
    m_tree->clear();
    // Kategoria: Budynek
    auto* building = new QTreeWidgetItem(m_tree, QStringList() << tr("Budynek"));
    new QTreeWidgetItem(building, QStringList() << tr("Ściany"));
    new QTreeWidgetItem(building, QStringList() << tr("Drzwi"));
    new QTreeWidgetItem(building, QStringList() << tr("Okna"));
    building->setExpanded(true);
    // Kategoria: Instalacje elektryczne
    auto* electrical = new QTreeWidgetItem(m_tree, QStringList() << tr("Instalacje elektryczne"));
    new QTreeWidgetItem(electrical, QStringList() << tr("Gniazda"));
    new QTreeWidgetItem(electrical, QStringList() << tr("Oświetlenie"));
    electrical->setExpanded(true);
    // Kategoria: Instalacje teletechniczne
    auto* teletech = new QTreeWidgetItem(m_tree, QStringList() << tr("Instalacje teletechniczne"));
    new QTreeWidgetItem(teletech, QStringList() << tr("Gniazda RJ45"));
    teletech->setExpanded(true);
    // Kategoria "Rozdzielnice" została usunięta.  W przyszłości będzie dodana
    // jako osobny moduł z własnym widżetem, więc nie dodajemy jej do drzewa.

    // Dodatkowa kategoria: Inne - pozwala włączać/wyłączać widoczność
    // tekstów wstawianych na płótno.  Element "Tekst" reprezentuje warstwę
    // tekstową, dzięki czemu można ją włączać i wyłączać z panelu Projekt.
    auto* other = new QTreeWidgetItem(m_tree, QStringList() << tr("Inne"));
    new QTreeWidgetItem(other, QStringList() << tr("Tekst"));
    other->setExpanded(true);
}
