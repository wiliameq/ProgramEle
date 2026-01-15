#pragma once

#include <QDockWidget>

class QStackedWidget;

/*
 * ToolSettingsWidget
 * ------------------
 * Dolny panel wyświetlający aktualne ustawienia aktywnego narzędzia. Panel ten
 * opiera się na QStackedWidget, dzięki czemu każde narzędzie może
 * dostarczyć własny widżet ustawień (np. parametry ściany, kolor, zapasy
 * przewodów). Kiedy brak aktywnego narzędzia, wyświetlany jest widżet
 * placeholder informujący użytkownika o braku aktywnego narzędzia.
 */
class ToolSettingsWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit ToolSettingsWidget(QWidget* parent = nullptr);

    // Zwraca widżet aktualnie wyświetlanych ustawień
    QWidget* currentSettings() const;
    // Ustawia nowy widżet z ustawieniami dla narzędzia. Jeśli nullptr,
    // przywraca placeholder.
    void setSettingsWidget(QWidget* widget);

private:
    QStackedWidget* m_stack = nullptr;
};
