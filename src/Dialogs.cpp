
#include "Dialogs.h"
#include "Settings.h"
#include "Measurements.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QLabel>
#include <QFileDialog>
#include <QPageSize>
#include <QTextCursor>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextTableCell>
#include <QTextTableCellFormat>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QAbstractTextDocumentLayout>
#include <QPrinter>
#include <QPainter>
#include <QTableWidget>
#include <QHeaderView>
#include <QApplication>
#include <QDateTime>
#include <QTextStream>
#include <QCheckBox>
#include <QMessageBox>
#include <QFont>
#include <QInputDialog>
#include <algorithm>
#include <QPdfWriter>
#include <QPageLayout>
#include <QTextDocument>
#include <QRegularExpression>

// -------- AdvancedMeasureDialog --------
AdvancedMeasureDialog::AdvancedMeasureDialog(QWidget* parent, ProjectSettings* settings)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(QString::fromUtf8("Pomiar zaawansowany"));
    auto lay = new QVBoxLayout(this);
    auto form = new QFormLayout();
    // Pole nazwy pomiaru
    m_name = new QLineEdit();
    m_name->setPlaceholderText(QString::fromUtf8("Nazwa pomiaru"));
    // Spinbox zapasu. Dokładność zależy od globalnej liczby miejsc po przecinku.
    m_buffer = new QDoubleSpinBox();
    m_buffer->setRange(0,100000);
    // Ustaw liczbę miejsc po przecinku i wartość startową zgodnie z ustawieniami projektu
    m_buffer->setDecimals(settings->decimals);
    m_buffer->setValue(0.0);
    // Wybór koloru linii
    m_colorBtn = new QPushButton(QString::fromUtf8("Wybierz kolor…"));
    m_chosen = settings->defaultMeasureColor;
    // Składamy formularz: brak wyboru jednostki – jednostką są centymetry
    form->addRow(QString::fromUtf8("Nazwa:"), m_name);
    // W dialogu pomiaru zaawansowanego ta wartość reprezentuje zapas początkowy
    // przypisany do konkretnego pomiaru. Globalny zapas jest ustawiany w opcjach
    // programu, dlatego nazywamy tę etykietę "Zapas początkowy".
    form->addRow(QString::fromUtf8("Zapas początkowy:"), m_buffer);
    form->addRow(QString::fromUtf8("Kolor linii:"), m_colorBtn);
    lay->addLayout(form);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    lay->addWidget(buttons);
    // Obsługa wyboru koloru
    QObject::connect(m_colorBtn, &QPushButton::clicked, this, [this](){
        QColor c = QColorDialog::getColor(m_chosen, this, QString::fromUtf8("Kolor linii"));
        if (c.isValid()) m_chosen = c;
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this](){ accept(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
QString AdvancedMeasureDialog::name() const { return m_name ? m_name->text() : QString(); }
double AdvancedMeasureDialog::bufferValue() const { return m_buffer ? m_buffer->value() : 0.0; }
QColor AdvancedMeasureDialog::color() const { return m_chosen; }

// -------- FinalBufferDialog --------
FinalBufferDialog::FinalBufferDialog(QWidget* parent, ProjectSettings* settings)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(QString::fromUtf8("Zapas końcowy"));
    auto lay = new QVBoxLayout(this);
    auto form = new QFormLayout();
    m_buffer = new QDoubleSpinBox();
    m_buffer->setRange(0,100000);
    // Liczbę miejsc po przecinku dobieramy do ustawień projektu.
    m_buffer->setDecimals(settings ? settings->decimals : 1);
    form->addRow(QString::fromUtf8("Zapas:"), m_buffer);
    lay->addLayout(form);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    lay->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this](){ accept(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
double FinalBufferDialog::bufferValue() const { return m_buffer ? m_buffer->value() : 0.0; }

// -------- EditMeasureDialog --------
EditMeasureDialog::EditMeasureDialog(QWidget* parent, ProjectSettings* settings, Measure* measure)
: QDialog(parent), m_settings(settings), m(measure) {
    setWindowTitle(QString::fromUtf8("Edytuj pomiar"));
    auto lay = new QVBoxLayout(this);
    auto form = new QFormLayout();
    // Pole nazwy z aktualną nazwą pomiaru
    m_name = new QLineEdit(measure->name);
    // Spinboxy zapasu: używamy aktualnej jednostki projektu do skalowania
    m_bufDefault = new QDoubleSpinBox(); m_bufDefault->setRange(0,100000);
    m_bufFinal  = new QDoubleSpinBox(); m_bufFinal->setRange(0,100000);
    // Liczba miejsc po przecinku zależy od ustawień projektu
    m_bufDefault->setDecimals(settings->decimals);
    m_bufFinal->setDecimals(settings->decimals);
    // Ustaw wartości w centymetrach
    m_bufDefault->setValue(measure->bufferDefaultMeters);
    m_bufFinal->setValue(measure->bufferFinalMeters);
    // Wybór koloru
    m_colorBtn = new QPushButton(QString::fromUtf8("Wybierz kolor…"));
    m_chosen = measure->color;
    // Układ formularza
    form->addRow(QString::fromUtf8("Nazwa:"), m_name);
    // Etykieta "Zapas początkowy" wskazuje zapas przypisany do początku pomiaru.
    form->addRow(QString::fromUtf8("Zapas początkowy:"), m_bufDefault);
    form->addRow(QString::fromUtf8("Zapas końcowy:"), m_bufFinal);
    form->addRow(QString::fromUtf8("Kolor:"), m_colorBtn);
    lay->addLayout(form);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    lay->addWidget(buttons);
    QObject::connect(m_colorBtn, &QPushButton::clicked, this, [this](){
        QColor c = QColorDialog::getColor(m_chosen, this, QString::fromUtf8("Kolor linii"));
        if (c.isValid()) m_chosen = c;
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this, settings](){
        // Zapisz zmiany do obiektu Measure
        m->name = m_name->text();
        // Jednostka jest stała (cm) – zapisujemy wartości bez konwersji.
        m->bufferDefaultMeters = m_bufDefault->value();
        m->bufferFinalMeters   = m_bufFinal->value();
        m->color = m_chosen;
        // Po zmianie zapasów oblicz ponownie długość z zapasami.  Całkowita
        // długość obejmuje długość, globalny zapas, zapas początkowy i zapas
        // końcowy.
        m->totalWithBufferMeters = m->lengthMeters + m->bufferGlobalMeters + m->bufferDefaultMeters + m->bufferFinalMeters;
        accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// -------- ReportDialog --------
ReportDialog::ReportDialog(QWidget* parent, ProjectSettings* settings, std::vector<Measure>* measures)
: QDialog(parent), m_settings(settings), m_measures(measures) {
    const int COL_CHECK   = 0;
    const int COL_ID      = 1;
    const int COL_NAME    = 2;
    const int COL_TYPE    = 3;
    const int COL_LEN_M   = 4;
    const int COL_SUM_M   = 5;
    const int COL_BUF_START = 6;
    const int COL_BUF_END   = 7;
    const int COL_COLOR = 8;
    const int COL_DATE = 9;
    const int COL_EDIT = 10;
    const int COL_DEL = 11;

    setWindowTitle(QString::fromUtf8("Raport pomiarów"));
    auto lay = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    // Zbuduj listę nagłówków z jednostką cm
    const QString unitLabel = QStringLiteral("cm");
    QStringList headers = {
        QStringLiteral("Check"),
        QStringLiteral("ID"),
        QString::fromUtf8("Nazwa"),
        QString::fromUtf8("Typ"),
        // Nagłówek długości wraz z jednostką
        QString::fromUtf8("Długość [%1]").arg(unitLabel),
        // Nagłówek sumy (długość + zapasy) z jednostką
        QString::fromUtf8("Suma z zapasami [%1]").arg(unitLabel),
        // Zapas domyślny (początkowy) z jednostką
        QString::fromUtf8("Zapas początkowy [%1]").arg(unitLabel),
        // Zapas końcowy z jednostką
        QString::fromUtf8("Zapas końcowy [%1]").arg(unitLabel),
        QString::fromUtf8("Kolor"),
        QString::fromUtf8("Data"),
        QString::fromUtf8("Edytuj"),
        QString::fromUtf8("Usuń")
    };
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setRowCount((int)measures->size());

    auto typeStr = [](const Measure& m)->QString{
        switch (m.type) {
            case MeasureType::Linear: return QString::fromUtf8("Liniowy");
            case MeasureType::Polyline: return QString::fromUtf8("Polilinia");
            case MeasureType::Advanced: default: return QString::fromUtf8("Zaawansowany");
        }
    };

    for (int r=0;r<(int)measures->size();++r) {
        const auto& m = (*measures)[r];
        auto chk = new QTableWidgetItem();
        chk->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        chk->setCheckState(Qt::Checked);
        m_table->setItem(r, COL_CHECK, chk);
        auto set = [&](int c, const QString& t){ m_table->setItem(r,c, new QTableWidgetItem(t)); };
        set(COL_ID, QString::number(m.id));
        set(COL_NAME, m.name);
        set(COL_TYPE, typeStr(m));
        // Formatowanie długości i sumy w cm
        QString lenStr;
        QString sumStr;
        double lenVal = m.lengthMeters;
        double sumVal = m.totalWithBufferMeters;
        lenStr = QString("%1 cm").arg(lenVal, 0, 'f', m_settings->decimals);
        sumStr = QString("%1 cm").arg(sumVal, 0, 'f', m_settings->decimals);
        QTableWidgetItem* itLen = new QTableWidgetItem(lenStr);
        itLen->setData(Qt::UserRole + 1, lenVal);
        m_table->setItem(r, COL_LEN_M, itLen);
        QTableWidgetItem* itSum = new QTableWidgetItem(sumStr);
        itSum->setData(Qt::UserRole + 1, sumVal);
        m_table->setItem(r, COL_SUM_M, itSum);
        // Zapas początkowy i końcowy również w cm
        double bufStartVal = m.bufferDefaultMeters;
        double bufEndVal   = m.bufferFinalMeters;
        QString bufStartStr;
        QString bufEndStr;
        bufStartStr = QString("%1 cm").arg(bufStartVal, 0, 'f', m_settings->decimals);
        bufEndStr   = QString("%1 cm").arg(bufEndVal,   0, 'f', m_settings->decimals);
        QTableWidgetItem* itBufStart = new QTableWidgetItem(bufStartStr);
        itBufStart->setData(Qt::UserRole + 1, bufStartVal);
        m_table->setItem(r, COL_BUF_START, itBufStart);
        QTableWidgetItem* itBufEnd = new QTableWidgetItem(bufEndStr);
        itBufEnd->setData(Qt::UserRole + 1, bufEndVal);
        m_table->setItem(r, COL_BUF_END, itBufEnd);
        // Kolor: wypełnij tło komórki i zapisz hex w Qt::UserRole
        {
            QString hex = m.color.name();
            QTableWidgetItem *it = new QTableWidgetItem(QString());
            it->setBackground(QBrush(QColor(hex)));
            it->setData(Qt::UserRole, hex);
            m_table->setItem(r, COL_COLOR, it);
        }
        set(COL_DATE, m.createdAt.toString("yyyy-MM-dd hh:mm"));

        // removed unused colorCell widget; the color is now represented by the table item itself
        
        auto btnEdit = new QPushButton(QString::fromUtf8("Edytuj"));
        m_table->setCellWidget(r, COL_EDIT, btnEdit);
        QObject::connect(btnEdit, &QPushButton::clicked, this, [=]() {
            // Pobierz aktualny indeks wiersza przy kliknięciu, aby uniknąć błędu
            int row = m_table->indexAt(btnEdit->pos()).row();
            if (row < 0 || row >= m_table->rowCount()) return;
            Measure& ref = (*m_measures)[row];
            EditMeasureDialog ed(this, m_settings, &ref);
            if (ed.exec()==QDialog::Accepted) {
                // Przelicz całkowitą długość z zapasami.  Obejmuje
                // długość, globalny zapas, zapas początkowy i końcowy.
                ref.totalWithBufferMeters = ref.lengthMeters + ref.bufferGlobalMeters + ref.bufferDefaultMeters + ref.bufferFinalMeters;
                // Aktualizuj widoczne komórki w cm
                m_table->item(row,COL_NAME)->setText(ref.name);
                double lenVal = ref.lengthMeters;
                double sumVal = ref.totalWithBufferMeters;
                QString lenStr;
                QString sumStr;
                lenStr = QString("%1 cm").arg(lenVal, 0, 'f', m_settings->decimals);
                sumStr = QString("%1 cm").arg(sumVal, 0, 'f', m_settings->decimals);
                QTableWidgetItem* itLen = m_table->item(row, COL_LEN_M);
                itLen->setText(lenStr);
                itLen->setData(Qt::UserRole + 1, lenVal);
                QTableWidgetItem* itSum = m_table->item(row, COL_SUM_M);
                itSum->setText(sumStr);
                itSum->setData(Qt::UserRole + 1, sumVal);
                // Zapas początkowy i końcowy
                double bufStartVal = ref.bufferDefaultMeters;
                double bufEndVal   = ref.bufferFinalMeters;
                QString bufStartStr;
                QString bufEndStr;
                bufStartStr = QString("%1 cm").arg(bufStartVal, 0, 'f', m_settings->decimals);
                bufEndStr   = QString("%1 cm").arg(bufEndVal,   0, 'f', m_settings->decimals);
                QTableWidgetItem* itBufStart = m_table->item(row, COL_BUF_START);
                itBufStart->setText(bufStartStr);
                itBufStart->setData(Qt::UserRole + 1, bufStartVal);
                QTableWidgetItem* itBufEnd = m_table->item(row, COL_BUF_END);
                itBufEnd->setText(bufEndStr);
                itBufEnd->setData(Qt::UserRole + 1, bufEndVal);
                // Aktualizuj kolor
                if (QTableWidgetItem *colorItem = m_table->item(row, COL_COLOR)) {
                    colorItem->setBackground(QBrush(ref.color));
                    colorItem->setData(Qt::UserRole, ref.color.name());
                }
                recalc();
            }
        });

        auto btnDel = new QPushButton(QString::fromUtf8("Usuń"));
        m_table->setCellWidget(r, COL_DEL, btnDel);
        QObject::connect(btnDel, &QPushButton::clicked, this, [this, btnDel]() {
            // Ustal faktyczny wiersz klikniętego przycisku
            int row = m_table->indexAt(btnDel->pos()).row();
            if (row < 0 || row >= m_table->rowCount()) return;
            if (QMessageBox::question(this, QString::fromUtf8("Usuń pomiar"), QString::fromUtf8("Na pewno usunąć ten pomiar?")) != QMessageBox::Yes)
                return;
            // Usuń pomiar z listy i usuń wiersz
            if (m_measures && row < (int)m_measures->size()) {
                m_measures->erase(m_measures->begin() + row);
            }
            m_table->removeRow(row);
            recalc();
        });
    }
    lay->addWidget(m_table);

    // --- Panel wyboru widoczności kolumn ---
    // Pozwala użytkownikowi wybrać, które kolumny tabeli mają być widoczne.  Kolumna
    // Check (0) jest zawsze widoczna, a kolumny edycji/usuwania (10 i 11) oraz
    // przyciski nie podlegają eksportowi, więc nie uwzględniamy ich w panelu.
    {
        auto colLay = new QHBoxLayout();
        colLay->addWidget(new QLabel(QString::fromUtf8("Pokaż kolumny:"), this));
        // Zakładamy, że nagłówki zostały już ustawione.  Tworzymy checkboxy dla
        // kolumn od ID (1) do Daty (9).  Stan checkboxa jest powiązany z
        // widocznością kolumny; odznaczenie powoduje ukrycie kolumny.
        for (int c = 1; c <= 9 && c < m_table->columnCount(); ++c) {
            QTableWidgetItem* header = m_table->horizontalHeaderItem(c);
            QString label = header ? header->text() : QStringLiteral("C%1").arg(c);
            QCheckBox* cb = new QCheckBox(label, this);
            cb->setChecked(!m_table->isColumnHidden(c));
            colLay->addWidget(cb);
            QObject::connect(cb, &QCheckBox::toggled, this, [this, c](bool checked) {
                m_table->setColumnHidden(c, !checked);
            });
        }
        colLay->addStretch();
        lay->addLayout(colLay);
    }

    auto btnsLay = new QHBoxLayout();
    auto btnCsv = new QPushButton(QString::fromUtf8("Eksport CSV"));
    auto btnPdf = new QPushButton(QString::fromUtf8("Eksport PDF"));
    auto btnTxt = new QPushButton(QString::fromUtf8("Eksport TXT"));
    btnsLay->addWidget(btnCsv); btnsLay->addWidget(btnPdf); btnsLay->addWidget(btnTxt); btnsLay->addStretch();
    lay->addLayout(btnsLay);

    auto foot = new QHBoxLayout();
    // Etykiety sum długości, zapasów i łącznej.  Jednostki i dokładność
    // zostaną ustawione w recalc(), więc tutaj podajemy wartości domyślne.
    m_sumLen = new QLabel(QString::fromUtf8("Suma długości zmierzonych: 0.0"));
    m_sumBuf = new QLabel(QString::fromUtf8("Suma zapasów: 0.0"));
    m_sumTotal = new QLabel(QString::fromUtf8("Suma łączna: 0.0"));
    foot->addWidget(m_sumLen);
    foot->addSpacing(20);
    foot->addWidget(m_sumBuf);
    foot->addSpacing(20);
    foot->addWidget(m_sumTotal);
    foot->addStretch();
    lay->addLayout(foot);

    QObject::connect(m_table, &QTableWidget::itemChanged, this, [=](QTableWidgetItem* it){
        if (it && it->column()==COL_CHECK) recalc();
    });

    // Umożliw bezpośrednią edycję niektórych pól poprzez kliknięcie w komórkę
    // (nazwa, zapas początkowy, zapas końcowy, kolor).  Kod obsługi edycji
    // został skopiowany z obsługi double-click i stosuje się teraz do
    // pojedynczego kliknięcia.  Wciąż podłączamy również double-click dla
    // kompatybilności, ale logika edycji jest identyczna.
    auto editCellLambda = [=](int row, int col) {
        if (!m_measures || row < 0 || row >= m_table->rowCount()) return;
        Measure &ref = (*m_measures)[row];
        // Stałe indeksy kolumn do edycji – muszą odpowiadać definicjom z początku konstruktora
        const int NAME_COL       = 2;
        const int BUF_START_COL  = 6;
        const int BUF_END_COL    = 7;
        const int COLOR_COL      = 8;
        const int SUM_COL        = 5;
        if (col == NAME_COL) {
            bool ok = false;
            QString newName = QInputDialog::getText(const_cast<ReportDialog*>(this),
                                                   QString::fromUtf8("Edytuj nazwę"),
                                                   QString::fromUtf8("Nazwa:"),
                                                   QLineEdit::Normal,
                                                   ref.name,
                                                   &ok);
            if (ok) {
                ref.name = newName;
                if (QTableWidgetItem* itName = m_table->item(row, NAME_COL)) {
                    itName->setText(newName);
                }
                recalc();
            }
        } else if (col == BUF_START_COL || col == BUF_END_COL) {
            // Edycja zapasu początkowego lub końcowego
            double currentVal = (col == BUF_START_COL) ? ref.bufferDefaultMeters : ref.bufferFinalMeters;
            bool ok = false;
            QString prompt = (col == BUF_START_COL)
                           ? QString::fromUtf8("Zapas początkowy (%1):")
                           : QString::fromUtf8("Zapas końcowy (%1):");
            QString unitLabel = QStringLiteral("cm");
            double newVal = QInputDialog::getDouble(const_cast<ReportDialog*>(this),
                                                    (col == BUF_START_COL)
                                                      ? QString::fromUtf8("Edytuj zapas początkowy")
                                                      : QString::fromUtf8("Edytuj zapas końcowy"),
                                                    prompt.arg(unitLabel),
                                                    currentVal,
                                                    0.0,
                                                    100000.0,
                                                    m_settings->decimals,
                                                    &ok);
            if (ok) {
                if (col == BUF_START_COL) {
                    ref.bufferDefaultMeters = newVal;
                } else {
                    ref.bufferFinalMeters = newVal;
                }
                // Zaktualizuj tekst i dane ukryte komórki
                QString text = QString("%1 cm").arg(newVal, 0, 'f', m_settings->decimals);
                QTableWidgetItem* itemBuf = m_table->item(row, col);
                if (!itemBuf) {
                    itemBuf = new QTableWidgetItem;
                    m_table->setItem(row, col, itemBuf);
                }
                itemBuf->setText(text);
                itemBuf->setData(Qt::UserRole + 1, newVal);
                // Przelicz całkowitą długość z zapasami i zaktualizuj kolumnę sumy
                ref.totalWithBufferMeters = ref.lengthMeters + ref.bufferGlobalMeters + ref.bufferDefaultMeters + ref.bufferFinalMeters;
                double sumMeters = ref.totalWithBufferMeters;
                QString sumStr = QString("%1 cm").arg(sumMeters, 0, 'f', m_settings->decimals);
                QTableWidgetItem* itSum = m_table->item(row, SUM_COL);
                if (!itSum) {
                    itSum = new QTableWidgetItem;
                    m_table->setItem(row, SUM_COL, itSum);
                }
                itSum->setText(sumStr);
                itSum->setData(Qt::UserRole + 1, sumMeters);
                recalc();
            }
        } else if (col == COLOR_COL) {
            // Edycja koloru
            QColor chosen = QColorDialog::getColor(ref.color, const_cast<ReportDialog*>(this), QString::fromUtf8("Wybierz kolor"));
            if (chosen.isValid()) {
                ref.color = chosen;
                QTableWidgetItem* colorItem = m_table->item(row, COLOR_COL);
                if (!colorItem) {
                    colorItem = new QTableWidgetItem;
                    m_table->setItem(row, COLOR_COL, colorItem);
                }
                colorItem->setBackground(QBrush(chosen));
                colorItem->setData(Qt::UserRole, chosen.name());
                // Kolumny sum nie zmieniamy, ale warto odświeżyć widok aby kolor
                // został odzwierciedlony w eksporcie i ewentualnie w recalc
                recalc();
            }
        }
    };
    // Podłącz zarówno kliknięcie, jak i podwójne kliknięcie do lambda edytującej komórkę.
    QObject::connect(m_table, &QTableWidget::cellClicked, this, editCellLambda);
    QObject::connect(m_table, &QTableWidget::cellDoubleClicked, this, editCellLambda);

    // Fit window size to content
    m_table->resizeColumnsToContents();
    int totalW = 0;
    for (int c=0;c<m_table->columnCount();++c) totalW += m_table->columnWidth(c);
    totalW += 40;
    int rowH = m_table->verticalHeader()->defaultSectionSize();
    int totalH = 100 + (rowH * (m_table->rowCount()+1)) + 180;
    totalW = qMin(totalW, 1400);
    totalH = qMin(totalH, 900);
    resize(totalW, totalH);

    
    // CSV export with UTF-8, tick mark and hex color
    // CSV export (UTF-8 + BOM; ';' separator; CRLF; ✓ for Check; hex in Kolor)
QObject::connect(btnCsv, &QPushButton::clicked, this, [this](){
    QString fn = QFileDialog::getSaveFileName(this, QString::fromUtf8("Zapisz CSV"),
                                              QStringLiteral("pomiary.csv"),
                                              QStringLiteral("CSV (*.csv)"));
    if (fn.isEmpty()) return;
    if (!fn.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) fn += QStringLiteral(".csv");

    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;

    QTextStream out(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out.setGenerateByteOrderMark(true);

    auto esc = [](QString s){
        s.replace('"', "\"\"");
        if (s.contains(QLatin1Char(';')) || s.contains(QLatin1Char('"')) ||
            s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r')))
            s = QLatin1Char('"') + s + QLatin1Char('"');
        return s;
    };
    auto writeRow = [&](const QStringList& cells){
        QStringList c = cells;
        for (QString &x : c) x = esc(x);
        out << c.join(QLatin1Char(';')) << "\r\n"; // CRLF for Excel
    };

    // Wyznacz listę widocznych kolumn (maksymalnie do indeksu 9 włącznie).
    QVector<int> visibleCols;
    for (int c = 0; c < m_table->columnCount() && c < 10; ++c) {
        if (!m_table->isColumnHidden(c)) visibleCols.append(c);
    }
    // Jeśli nie ma widocznych kolumn (poza ukrytymi), nic nie zapisujemy
    if (visibleCols.isEmpty()) {
        return;
    }
    // Napisz nagłówek z nazw kolumn
    QStringList header;
    header.reserve(visibleCols.size());
    for (int col : qAsConst(visibleCols)) {
        QTableWidgetItem* h = m_table->horizontalHeaderItem(col);
        header << (h ? h->text() : QStringLiteral("C%1").arg(col));
    }
    writeRow(header);
    // Zlokalizuj indeks kolumny „Kolor” w liście widocznych kolumn (indeks w header)
    int kolorColVisIndex = -1;
    for (int i = 0; i < visibleCols.size(); ++i) {
        QTableWidgetItem* h = m_table->horizontalHeaderItem(visibleCols[i]);
        if (h && h->text().trimmed().toLower() == QString::fromUtf8("kolor")) {
            kolorColVisIndex = i;
            break;
        }
    }
    // Eksportuj tylko te wiersze, które są zaznaczone (checkbox w kolumnie 0)
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* chkItem = m_table->item(r, 0);
        if (!chkItem || chkItem->checkState() != Qt::Checked) continue;
        QStringList row;
        row.reserve(visibleCols.size());
        for (int visIndex = 0; visIndex < visibleCols.size(); ++visIndex) {
            int c = visibleCols[visIndex];
            if (c == 0) {
                row << QString(QChar(0x2713));
            } else if (visIndex == kolorColVisIndex) {
                QTableWidgetItem* item = m_table->item(r, c);
                QString hx = item ? item->data(Qt::UserRole).toString() : QString();
                if (hx.isEmpty() && item) hx = item->text();
                row << hx;
            } else {
                QTableWidgetItem* item = m_table->item(r, c);
                row << (item ? item->text() : QString());
            }
        }
        writeRow(row);
    }
    out.flush();
});

    // PDF export (QPdfWriter, no physical printer involved)
    // PDF export with QPdfWriter (no printer), centered table, colored borders
QObject::connect(btnPdf, &QPushButton::clicked, this, [this](){
    QString fn = QFileDialog::getSaveFileName(this, QString::fromUtf8("Zapisz PDF"),
                                              QStringLiteral("pomiary.pdf"),
                                              QStringLiteral("PDF (*.pdf)"));
    if (fn.isEmpty()) return;
    if (!fn.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) fn += QStringLiteral(".pdf");

    QApplication::setOverrideCursor(Qt::WaitCursor);
    // Use QPdfWriter instead of QPrinter to avoid invoking any printer subsystem.
    QPdfWriter writer(fn);
    QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                       QMarginsF(12,12,12,12), QPageLayout::Millimeter);
    writer.setPageLayout(layout);

QTextDocument doc;
    
    doc.setDocumentMargin(0);
    
    QTextCursor cur(&doc);

    // Title + 2x enter
    {
        QTextBlockFormat bf; bf.setAlignment(Qt::AlignLeft);
        cur.setBlockFormat(bf);
        QTextCharFormat cf; QFont f; f.setBold(true); f.setPointSizeF(f.pointSizeF()+6); cf.setFont(f);
        cur.setCharFormat(cf);
        cur.insertText(QString::fromUtf8("Raport pomiarów — %1")
                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
        cur.insertBlock(); QTextBlockFormat gap; gap.setTopMargin(20); cur.setBlockFormat(gap); cur.insertBlock();
    }

    // Budowanie tabeli z uwzględnieniem tylko widocznych kolumn (maksymalnie 10 pierwszych)
    QVector<int> visibleCols;
    for (int c = 0; c < m_table->columnCount() && c < 10; ++c) {
        if (!m_table->isColumnHidden(c)) visibleCols.append(c);
    }
    int colCount = visibleCols.size();
    // Zlicz ile wierszy jest zaznaczonych (checkbox w kolumnie 0)
    QVector<int> selectedRows;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* chk = m_table->item(r, 0);
        if (chk && chk->checkState() == Qt::Checked) selectedRows.append(r);
    }
    int rows = selectedRows.size();

    // Jeśli nie ma kolumn do wyświetlenia lub żadnych zaznaczonych wierszy,
    // nadal rysujemy pustą tabelę z samym nagłówkiem
    QTextTable* table = nullptr;
    if (colCount > 0) {
        QTextTableFormat tf;
        tf.setAlignment(Qt::AlignHCenter);
        tf.setWidth(QTextLength(QTextLength::PercentageLength, 100.0));
        tf.setBorder(0.8);
        tf.setBorderBrush(QBrush(QColor(QStringLiteral("#666666")))); // visible outline
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        tf.setCellPadding(0);
        tf.setCellSpacing(0);
        QList<QTextLength> widths;
        for (int i = 0; i < colCount; ++i) {
            widths << QTextLength(QTextLength::PercentageLength, 100.0 / colCount);
        }
        tf.setColumnWidthConstraints(widths);
        // Stwórz tabelę: liczba wierszy to liczba zaznaczonych wierszy + nagłówek
        table = cur.insertTable(rows + 1, colCount, tf);

        // Grid format dla każdej komórki
        QTextTableCellFormat gridFmt;
        gridFmt.setBorder(0.5);
        gridFmt.setBorderBrush(QBrush(QColor(QStringLiteral("#999999"))));
        gridFmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);

        // Funkcja pomocnicza do wstawiania tekstu
        auto put = [&](int r, int c, const QString& s) {
            QTextTableCell cell = table->cellAt(r, c);
            cell.setFormat(gridFmt);
            QTextCursor cc = cell.firstCursorPosition();
            QTextBlockFormat bf;
            bf.setAlignment(Qt::AlignHCenter);
            bf.setLeftMargin(4);
            bf.setRightMargin(4);
            cc.setBlockFormat(bf);
            cc.insertText(s);
        };

        // Nagłówki tylko dla widocznych kolumn
        for (int i = 0; i < colCount; ++i) {
            int c = visibleCols[i];
            QTableWidgetItem* h = m_table->horizontalHeaderItem(c);
            put(0, i, h ? h->text() : QStringLiteral("C%1").arg(c));
        }

        // Indeks kolumny koloru w liście widocznych kolumn
        int kolorColVisIndex = -1;
        for (int i = 0; i < colCount; ++i) {
            int c = visibleCols[i];
            QTableWidgetItem* h = m_table->horizontalHeaderItem(c);
            if (h && h->text().trimmed().toLower() == QString::fromUtf8("kolor")) {
                kolorColVisIndex = i;
                break;
            }
        }

        // Wiersze z danymi – tylko zaznaczone wiersze
        for (int rowIndex = 0; rowIndex < selectedRows.size(); ++rowIndex) {
            int r = selectedRows[rowIndex];
            for (int i = 0; i < colCount; ++i) {
                int c = visibleCols[i];
                QTableWidgetItem* it = m_table->item(r, c);
                QString s;
                if (c == 0) {
                    // Kolumna Check – zawsze zaznaczona w eksporcie
                    s = QString(QChar(0x2713));
                } else {
                    s = it ? it->text() : QString();
                }
                put(rowIndex + 1, i, s);
                // Wypełnij tło dla kolumny koloru
                if (i == kolorColVisIndex && it) {
                    QString hx = it->data(Qt::UserRole).toString();
                    if (hx.isEmpty()) hx = it->text();
                    if (!hx.isEmpty()) {
                        QTextTableCell cell = table->cellAt(rowIndex + 1, i);
                        QTextCharFormat fmt = cell.format();
                        fmt.setBackground(QBrush(QColor(hx)));
                        cell.setFormat(fmt);
                    }
                }
            }
        }
    }

    // Bottom sums
    cur.movePosition(QTextCursor::End); cur.insertBlock();
    { QTextBlockFormat gap2; gap2.setTopMargin(20); cur.setBlockFormat(gap2); cur.insertBlock(); }
    {
        QTextCharFormat scf; QFont sf; sf.setPointSizeF(sf.pointSizeF()+2); sf.setBold(true); scf.setFont(sf);
        cur.setCharFormat(scf);
        const QString sums = QStringList{ m_sumLen->text(), m_sumBuf->text(), m_sumTotal->text() }
                             .join(QLatin1Char('\n'));
        cur.insertText(sums);
    }

    // Automatic multi-page print
    doc.setPageSize(layout.paintRectPoints().size());
    doc.print(&writer);

    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, QString::fromUtf8("Eksport zakończony"),
                             QString::fromUtf8("Plik PDF został poprawnie utworzony."));
});


    // TXT export (UTF-8, CRLF, tab-separated, ✓ for selected rows and hex color)
    QObject::connect(btnTxt, &QPushButton::clicked, this, [this](){
        // Choose output filename; default to pomiary.txt
        QString fn = QFileDialog::getSaveFileName(this,
                                                  QString::fromUtf8("Zapisz TXT"),
                                                  QStringLiteral("pomiary.txt"),
                                                  QStringLiteral("Text (*.txt)"));
        if (fn.isEmpty()) return;
        if (!fn.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive))
            fn += QStringLiteral(".txt");

        QFile f(fn);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return;
        }
        QTextStream out(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        out.setEncoding(QStringConverter::Utf8);
#else
        out.setCodec("UTF-8");
#endif
        // Wyznacz listę widocznych kolumn (maksymalnie 10 pierwszych) oraz jednostkę
        QVector<int> visibleCols;
        for (int c = 0; c < m_table->columnCount() && c < 10; ++c) {
            if (!m_table->isColumnHidden(c)) visibleCols.append(c);
        }
        // Buduj nagłówek na podstawie widocznych kolumn
        QStringList header;
        header.reserve(visibleCols.size());
        const QString unitLabel = QStringLiteral("cm");
        for (int idx = 0; idx < visibleCols.size(); ++idx) {
            int c = visibleCols[idx];
            if (c == 0) {
                header << QString::fromUtf8("✓");
            } else {
                QTableWidgetItem* h = m_table->horizontalHeaderItem(c);
                QString label = h ? h->text() : QStringLiteral("C%1").arg(c);
                // Jeżeli nagłówek zawiera w treści nazwę jednostki w nawiasach kwadratowych,
                // zastąp go dynamicznie bieżącą jednostką.  Dotyczy to kolumn długości,
                // sumy z zapasami oraz zapasów.
                label = label.replace(QStringLiteral("[cm]"), QStringLiteral("[%1]").arg(unitLabel))
                             .replace(QStringLiteral("[m]"), QStringLiteral("[%1]").arg(unitLabel));
                header << label;
            }
        }
        // Zapisz nagłówek
        out << header.join(QLatin1Char('\t')) << "\r\n";

        // Eksportuj tylko zaznaczone wiersze
        for (int r = 0; r < m_table->rowCount(); ++r) {
            QTableWidgetItem* chkItem = m_table->item(r, 0);
            if (!chkItem || chkItem->checkState() != Qt::Checked)
                continue;

            QStringList fields;
            fields.reserve(visibleCols.size());
            for (int idx = 0; idx < visibleCols.size(); ++idx) {
                int c = visibleCols[idx];
                if (c == 0) {
                    fields << QString(QChar(0x2713));
                } else if (c == 8) {
                    // Kolor – pobierz hex z UserRole lub tekst
                    QString hx;
                    if (QTableWidgetItem* colItem = m_table->item(r, c)) {
                        hx = colItem->data(Qt::UserRole).toString();
                        if (hx.isEmpty())
                            hx = colItem->text();
                    }
                    fields << hx;
                } else {
                    QTableWidgetItem* it = m_table->item(r, c);
                    fields << (it ? it->text() : QString());
                }
            }
            out << fields.join(QLatin1Char('\t')) << "\r\n";
        }
        // Summary lines
        out << m_sumLen->text()  << "\r\n";
        out << m_sumBuf->text()  << "\r\n";
        out << m_sumTotal->text() << "\r\n";
        out.flush();
    });


    recalc();
}

void ReportDialog::recalc(){
    if (!m_table || m_table->rowCount()==0) {
        // Brak pomiarów – wyświetl zera w cm
        QString zeroStr = QString::number(0.0, 'f', m_settings->decimals) + QStringLiteral(" cm");
        m_sumLen->setText(QString::fromUtf8("Suma długości zmierzonych: ") + zeroStr);
        m_sumBuf->setText(QString::fromUtf8("Suma zapasów: ") + zeroStr);
        m_sumTotal->setText(QString::fromUtf8("Suma łączna: ") + zeroStr);
        return;
    }
    const int COL_CHECK     = 0;
    const int COL_LEN_M     = 4; // Długość [cm]
    const int COL_BUF_START = 6; // cm
    const int COL_BUF_END   = 7; // cm

    double sumLenM = 0.0;
    double sumBufM = 0.0;

    for (int r=0; r<m_table->rowCount(); ++r) {
        auto chk = m_table->item(r, COL_CHECK);
        if (!chk || chk->checkState() != Qt::Checked) continue;

        // --- Długość w metrach z danych ukrytych ---
        double lenM = 0.0;
        if (auto itLen = m_table->item(r, COL_LEN_M)) {
            QVariant v = itLen->data(Qt::UserRole + 1);
            if (v.isValid()) lenM = v.toDouble();
        }
        // --- Zapas początkowy i końcowy w metrach z danych ukrytych ---
        double bufStartM = 0.0;
        if (auto itStart = m_table->item(r, COL_BUF_START)) {
            QVariant v = itStart->data(Qt::UserRole + 1);
            if (v.isValid()) bufStartM = v.toDouble();
        }
        double bufEndM = 0.0;
        if (auto itEnd = m_table->item(r, COL_BUF_END)) {
            QVariant v = itEnd->data(Qt::UserRole + 1);
            if (v.isValid()) bufEndM = v.toDouble();
        }
        sumLenM += lenM;
        sumBufM += (bufStartM + bufEndM);
    }

    double sumTotalM = sumLenM + sumBufM;
    QString sumLenStr   = QString("%1 cm").arg(sumLenM, 0, 'f', m_settings->decimals);
    QString sumBufStr   = QString("%1 cm").arg(sumBufM, 0, 'f', m_settings->decimals);
    QString sumTotalStr = QString("%1 cm").arg(sumTotalM, 0, 'f', m_settings->decimals);
    // Ustaw etykiety sum.  Prefiks jest ustawiany tutaj, zaś wartości są
    // już sformatowane powyżej.
    m_sumLen->setText(QString::fromUtf8("Suma długości zmierzonych: ") + sumLenStr);
    m_sumBuf->setText(QString::fromUtf8("Suma zapasów: ") + sumBufStr);
    m_sumTotal->setText(QString::fromUtf8("Suma łączna: ") + sumTotalStr);
}
