#include "my_classes.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

static QString money(double v) { return QString::number(v, 'f', 2); }

static void showCenteredMessageBox(QWidget* parent,
                                  QMessageBox::Icon icon,
                                  const QString& title,
                                  const QString& text) {
    QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
    box.setWindowModality(Qt::WindowModal);
    box.adjustSize();
    if (parent) {
        const QPoint center = parent->frameGeometry().center();
        box.move(center - QPoint(box.width() / 2, box.height() / 2));
    }
    box.exec();
}

struct TypeUi {
    QFrame* tile = nullptr;
    QLabel* premium = nullptr;
    QLabel* months = nullptr;
    QLabel* demand = nullptr;
    QLabel* maxCov = nullptr;
    QPushButton* edit = nullptr; // редактировать условия
};

static bool startGameDialog(QWidget* parent, int* monthsOut) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Start game"));

    auto* form = new QFormLayout;
    auto* m = new QSpinBox;
    m->setRange(6, 24);
    m->setValue(*monthsOut);
    form->addRow(QStringLiteral("Количество месяцев (M)"), m);

    auto* startBtn = new QPushButton(QStringLiteral("Старт"));
    auto* cancelBtn = new QPushButton(QStringLiteral("Отмена"));
    startBtn->setDefault(true);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(startBtn);
    buttons->addWidget(cancelBtn);

    auto* v = new QVBoxLayout;
    v->addLayout(form);
    v->addLayout(buttons);
    dlg.setLayout(v);

    QObject::connect(startBtn, &QPushButton::clicked, [&]() {
        m->interpretText();
        if (monthsOut) *monthsOut = m->value();
        dlg.accept();
    });
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    // Центрируем относительно основного окна (как "Победа/Поражение")
    dlg.adjustSize();
    if (parent) {
        const QPoint center = parent->frameGeometry().center();
        dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
    }

    if (dlg.exec() != QDialog::Accepted) return false;
    m->interpretText();
    if (monthsOut) *monthsOut = m->value();
    return true;
}

static bool editOffer(QWidget* parent, Offer* o) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Условия страховки"));

    auto* form = new QFormLayout;

    auto* prem = new QDoubleSpinBox;
    prem->setRange(0, 1e9);
    prem->setDecimals(2);
    prem->setValue(o->premiumPerMonth);

    auto* dur = new QSpinBox;
    dur->setRange(1, 120);
    dur->setValue(o->contractMonths);

    auto* maxC = new QDoubleSpinBox;
    maxC->setRange(1, 1e12);
    maxC->setDecimals(2);
    maxC->setValue(o->maxCoverage);

    auto* ded = new QDoubleSpinBox;
    ded->setRange(0, 1e12);
    ded->setDecimals(2);
    ded->setValue(o->deductible);

    form->addRow(QStringLiteral("Взнос (у.е./мес)"), prem);
    form->addRow(QStringLiteral("Срок договора (мес)"), dur);
    form->addRow(QStringLiteral("Макс. возмещение"), maxC);
    form->addRow(QStringLiteral("Франшиза"), ded);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* lay = new QVBoxLayout;
    lay->addLayout(form);
    lay->addWidget(buttons);
    dlg.setLayout(lay);

    if (dlg.exec() != QDialog::Accepted) return false;

    o->premiumPerMonth = prem->value();
    o->contractMonths = dur->value();
    o->maxCoverage = maxC->value();
    o->deductible = ded->value();
    return true;
}

static TypeUi makeTypeTile(const QString& title, InsuranceType t) {
    TypeUi ui;
    ui.tile = new QFrame;
    ui.tile->setFixedWidth(220);
    ui.tile->setStyleSheet(QStringLiteral(
        "QFrame{border:1px solid #000000; background:#ffffff;}"));

    auto* v = new QVBoxLayout;
    v->setSpacing(6);
    v->setContentsMargins(10, 10, 10, 10);

    auto* head = new QLabel(title);
    head->setAlignment(Qt::AlignLeft);
    head->setStyleSheet(QStringLiteral("font-size:16px; font-weight:700;"));
    v->addWidget(head);

    auto makeRow = [&](const QString& k, QLabel** out) {
        auto* row = new QHBoxLayout;
        auto* key = new QLabel(k);
        key->setMinimumWidth(110);
        auto* val = new QLabel(QStringLiteral("—"));
        val->setAlignment(Qt::AlignRight);
        val->setStyleSheet(QStringLiteral("font-weight:700;"));
        row->addWidget(key);
        row->addWidget(val, 1);
        v->addLayout(row);
        *out = val;
    };

    makeRow(QStringLiteral("premiumPerMonth"), &ui.premium);
    makeRow(QStringLiteral("contractMonths"), &ui.months);
    makeRow(QStringLiteral("curDemand"), &ui.demand);
    makeRow(QStringLiteral("maxCoverage"), &ui.maxCov);

    ui.edit = new QPushButton(QStringLiteral("редактировать"));
    ui.edit->setProperty("type", static_cast<int>(t));
    v->addWidget(ui.edit);

    ui.tile->setLayout(v);
    return ui;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    Config cfg;
    Simulation sim(cfg);
    bool gameStarted = false;

    QWidget window;
    window.setWindowTitle(QStringLiteral("Страховая компания — модель"));
    window.setStyleSheet(QStringLiteral(
        "QGroupBox{font-weight:600;} "
        "QPushButton{padding:4px 8px; border:1px solid #000000; background:#ffffff;}"));

    auto* main = new QVBoxLayout;

    // Top bar
    auto* top = new QHBoxLayout;
    auto* month = new QLabel;
    month->setAlignment(Qt::AlignCenter);
    month->setStyleSheet(QStringLiteral("font-size:28px; font-weight:700;"));

    auto* endEarlyBtn = new QPushButton(QStringLiteral("Досрочно завершить игру"));
    endEarlyBtn->setEnabled(false);
    auto* nextBtn = new QPushButton(QStringLiteral("Следующий месяц"));
    nextBtn->setEnabled(false); // включим после "Старт"
    top->addWidget(endEarlyBtn, 0, Qt::AlignLeft);
    top->addStretch();
    top->addWidget(month);
    top->addStretch();
    top->addWidget(nextBtn, 0, Qt::AlignRight);

    // Center: left offers + right table (как на рисунке)
    auto* center = new QHBoxLayout;

    // Left: 3 insurance panels
    auto* left = new QWidget;
    left->setFixedWidth(720);
    auto* leftV = new QVBoxLayout;
    auto* title = new QLabel(QStringLiteral("Актуальные предложения страхования"));
    title->setAlignment(Qt::AlignCenter);
    leftV->addWidget(title);

    auto* tiles = new QHBoxLayout;
    std::array<TypeUi, 3> ui{
        makeTypeTile(QStringLiteral("Жильё"), InsuranceType::Home),
        makeTypeTile(QStringLiteral("Авто"), InsuranceType::Car),
        makeTypeTile(QStringLiteral("Здоровье"), InsuranceType::Health),
    };
    tiles->addWidget(ui[0].tile);
    tiles->addWidget(ui[1].tile);
    tiles->addWidget(ui[2].tile);
    leftV->addLayout(tiles);

    // Архив (ниже актуальных предложений)
    auto* archiveTitle = new QLabel(QStringLiteral("Архив"));
    archiveTitle->setAlignment(Qt::AlignCenter);
    archiveTitle->setStyleSheet(QStringLiteral("font-size:14px; font-weight:700;"));
    leftV->addWidget(archiveTitle);

    auto* archiveRow = new QHBoxLayout;
    std::array<QVBoxLayout*, 3> archiveCols{};
    std::array<QWidget*, 3> archiveColsHost{};

    for (int i = 0; i < 3; ++i) {
        auto* host = new QFrame;
        host->setFixedWidth(220);
        host->setStyleSheet(QStringLiteral(
            "QFrame{border:1px solid #000000; background:#ffffff;}"));
        auto* v = new QVBoxLayout;
        v->setContentsMargins(6, 6, 6, 6);
        auto* h = new QLabel(QStringLiteral("%1").arg(insuranceTypeName(static_cast<InsuranceType>(i))));
        h->setAlignment(Qt::AlignCenter);
        h->setStyleSheet(QStringLiteral("font-weight:700;"));
        v->addWidget(h);
        host->setLayout(v);
        archiveCols[i] = v;
        archiveColsHost[i] = host;
        archiveRow->addWidget(host);
    }
    leftV->addLayout(archiveRow);
    leftV->addStretch();
    left->setLayout(leftV);

    // Right: table big
    // Номер месяца показываем номерами строк (вертикальный заголовок),
    // поэтому отдельная колонка "Номер месяца" не нужна.
    auto* table = new QTableWidget(0, 3);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("Капитал"), QStringLiteral("Выплаты"), QStringLiteral("Всего договоров")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(28);

    center->addWidget(left, 2);
    center->addWidget(table, 5);

    auto refresh = [&]() {
        const int totalMonths = sim.config().M;
        const int done = sim.currentMonth();
        month->setText(QStringLiteral("%1/%2")
                           .arg(std::min(done + 1, totalMonths))
                           .arg(totalMonths));

        const auto& off = sim.offers();
        for (int i = 0; i < 3; ++i) {
            const Offer& o = off[size_t(i)];
            ui[i].premium->setText(QString::number(o.premiumPerMonth, 'f', 0));
            ui[i].months->setText(QString::number(o.contractMonths));
            ui[i].demand->setText(QString::number(o.curDemand, 'f', 0));
            ui[i].maxCov->setText(QString::number(o.maxCoverage, 'f', 0));
        }

        // Перерисовка архива: кнопка "страховкаN" + количество договоров
        for (int ti = 0; ti < 3; ++ti) {
            // очистим всё, кроме заголовка (первый виджет)
            auto* lay = archiveCols[ti];
            while (lay->count() > 1) {
                QLayoutItem* it = lay->takeAt(1);
                if (!it) break;
                if (auto* w = it->widget()) w->deleteLater();
                delete it;
            }

            const auto t = static_cast<InsuranceType>(ti);
            const auto& arc = sim.offerArchive(t);
            const auto& sold = sim.offerArchiveContractsSold(t);

            for (int j = 0; j < static_cast<int>(arc.size()); ++j) {
                auto* row = new QFrame;
                row->setStyleSheet(QStringLiteral(
                    "QFrame{border:1px solid #000000; background:#ffffff;}"));
                auto* h = new QHBoxLayout;
                h->setContentsMargins(4, 4, 4, 4);

                auto* btn = new QPushButton(QStringLiteral("страховка%1").arg(j + 1));
                btn->setMaximumWidth(110);
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton{border:1px solid #000000; background:#ffffff; padding:2px 6px;}"));

                auto* cnt = new QLabel(QStringLiteral("%1").arg(j < static_cast<int>(sold.size()) ? sold[size_t(j)] : 0));
                cnt->setFrameStyle(QFrame::NoFrame);
                cnt->setAlignment(Qt::AlignCenter);
                cnt->setMinimumWidth(70);
                cnt->setStyleSheet(QStringLiteral(
                    "QLabel{border:1px solid #000000; background:#ffffff; padding:2px;}"));

                const Offer offer = arc[size_t(j)];
                QObject::connect(btn, &QPushButton::clicked, [&window, &sim, offer]() {
                    Offer tmp = offer;
                    // спрос для архива показываем как вычисленный по формуле (с текущим baseDemand)
                    tmp.curDemand = sim.computeDemand(tmp);
                    showCenteredMessageBox(
                        &window, QMessageBox::Information, QStringLiteral("Страховка (архив)"),
                        QStringLiteral("premiumPerMonth: %1\ncurDemand: %2\nmaxCoverage: %3")
                            .arg(QString::number(tmp.premiumPerMonth, 'f', 0))
                            .arg(QString::number(tmp.curDemand, 'f', 0))
                            .arg(QString::number(tmp.maxCoverage, 'f', 0)));
                });

                h->addWidget(btn);
                h->addWidget(cnt);
                row->setLayout(h);
                lay->addWidget(row);
            }
        }

        const bool stop = sim.bankrupt() || sim.finished();
        nextBtn->setEnabled(gameStarted && !stop);
        endEarlyBtn->setEnabled(gameStarted && !stop);
        for (auto& x : ui) x.edit->setEnabled(!stop);
    };

    auto appendRow = [&](const MonthResult& r) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(r.month)));
        auto set = [&](int col, const QString& s) {
            table->setItem(row, col, new QTableWidgetItem(s));
        };
        set(0, money(r.capitalAfter));
        set(1, money(r.totalClaims));
        const int totalSold = r.sold[0] + r.sold[1] + r.sold[2];
        set(2, QString::number(totalSold));
    };

    QObject::connect(nextBtn, &QPushButton::clicked, [&]() {
        MonthResult r;
        const bool ok = sim.step(&r);
        appendRow(r);

        if (!ok && sim.bankrupt()) {
            showCenteredMessageBox(
                &window, QMessageBox::Warning, QStringLiteral("Поражение"),
                QStringLiteral("Разорение в месяце %1.\nИтоговый капитал: %2")
                    .arg(r.month)
                    .arg(money(r.capitalAfter)));
        } else if (sim.finished()) {
            showCenteredMessageBox(&window, QMessageBox::Information, QStringLiteral("Победа"),
                                   QStringLiteral("Игра завершена.\nИтоговый капитал: %1")
                                       .arg(money(r.capitalAfter)));
        }
        refresh();
    });

    QObject::connect(endEarlyBtn, &QPushButton::clicked, [&]() {
        const auto ans = QMessageBox::question(
            &window, QStringLiteral("Досрочное завершение"),
            QStringLiteral("Завершить игру досрочно?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes) return;
        sim.finishEarly();
        showCenteredMessageBox(
            &window, QMessageBox::Information, QStringLiteral("Игра завершена"),
            QStringLiteral("Игра досрочно остановлена.\nТекущий капитал: %1")
                .arg(money(sim.capital())));
        refresh();
    });

    for (int ti = 0; ti < 3; ++ti) {
        QObject::connect(ui[ti].edit, &QPushButton::clicked, [&, ti]() {
            InsuranceType t = static_cast<InsuranceType>(ti);
            Offer o = sim.offers()[size_t(ti)];
            if (!editOffer(&window, &o)) return;
            sim.setOffer(t, o);
            refresh();
        });
    }

    main->addLayout(top);
    main->addLayout(center, 1);
    window.setLayout(main);

    refresh();
    window.resize(1200, 720);
    window.show();

    // Поверх показанного основного окна — "Start game" по центру.
    int months = sim.config().M;
    if (!startGameDialog(&window, &months)) return 0;
    // Берём актуальный Config из симуляции и меняем только M — так не потеряются поля
    Config gameCfg = sim.config();
    gameCfg.M = months;
    sim.setConfig(gameCfg);
    cfg = gameCfg;
    gameStarted = true;
    table->setRowCount(0);
    refresh();

    return app.exec();
}