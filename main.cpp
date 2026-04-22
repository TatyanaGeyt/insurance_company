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
#include <QBrush>
#include <QColor>

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

static QString expiredOffersMessage(const std::array<bool, 3>& expired) {
    QStringList types;
    if (expired[0]) types << QStringLiteral("Жильё");
    if (expired[1]) types << QStringLiteral("Авто");
    if (expired[2]) types << QStringLiteral("Здоровье");

    if (types.isEmpty()) return QStringLiteral("Добавьте актуальное предложение страхования!");
    if (types.size() == 1) {
        return QStringLiteral("Вышел срок действия предложения для страховки типа \"%1\".\n\nДобавьте актуальное предложение страхования!")
            .arg(types[0]);
    }
    return QStringLiteral("Вышел срок действия предложения для страховки типов \"%1\".\n\nДобавьте актуальное предложение страхования!")
        .arg(types.join(QStringLiteral(", ")));
}

struct TypeUi {
    QFrame* tile = nullptr;
    QLabel* premium = nullptr;
    QLabel* months = nullptr;
    QLabel* demand = nullptr;
    QLabel* maxCov = nullptr;
    QLabel* baseDemand = nullptr;
    QLabel* offerValid = nullptr;
    QPushButton* edit = nullptr;
};

static void addKeyValueRow(QVBoxLayout* layout, const QString& keyText, QLabel** outValue) {
    auto* row = new QHBoxLayout;
    auto* key = new QLabel(keyText);
    key->setMinimumWidth(110);
    auto* val = new QLabel(QStringLiteral("—"));
    val->setAlignment(Qt::AlignRight);
    val->setStyleSheet(QStringLiteral("font-weight:700;"));
    row->addWidget(key);
    row->addWidget(val, 1);
    layout->addLayout(row);
    *outValue = val;
}

static void appendMonthRow(QTableWidget* table, const MonthResult& r) {
    const int row = table->rowCount();
    table->insertRow(row);
    table->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(r.month)));
    auto set = [&](int col, const QString& s) { table->setItem(row, col, new QTableWidgetItem(s)); };
    set(0, money(r.capitalAfter));
    set(1, money(r.totalClaims));
    const int totalClaimsCount = r.claimsCount[0] + r.claimsCount[1] + r.claimsCount[2];
    set(2, QString::number(totalClaimsCount));
    const int totalSold = r.sold[0] + r.sold[1] + r.sold[2];
    set(3, QString::number(totalSold));
}

static void highlightTableLastRow(QTableWidget* table, const QColor& bg) {
    const int row = table->rowCount() - 1;
    if (row < 0) return;
    for (int col = 0; col < table->columnCount(); ++col) {
        if (auto* it = table->item(row, col)) {
            it->setBackground(QBrush(bg));
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
    }
    if (auto* vh = table->verticalHeaderItem(row)) {
        vh->setBackground(QBrush(bg));
        QFont f = vh->font();
        f.setBold(true);
        vh->setFont(f);
    }
}

struct ArchiveRowUi {
    QFrame* row = nullptr;
    QPushButton* btn = nullptr;
    QLabel* cnt = nullptr;
};

static void refreshUi(Simulation& sim,
                      QLabel* monthLabel,
                      QLabel* expectedClaimsLabel,
                      std::array<TypeUi, 3>& ui,
                      const std::array<QVBoxLayout*, 3>& archiveCols,
                      std::array<std::vector<ArchiveRowUi>, 3>& archiveUi,
                      QPushButton* nextBtn,
                      QPushButton* endEarlyBtn,
                      bool gameStarted,
                      QWidget* window) {
    const int totalMonths = sim.config().M;
    const int done = sim.currentMonth();
    monthLabel->setText(QStringLiteral("%1/%2")
                            .arg(std::min(done + 1, totalMonths))
                            .arg(totalMonths));
    const int minC = sim.config().minClaimsPerMonth;
    const int maxC = sim.config().maxClaimsPerMonth;
    const int nextMonth = sim.currentMonth() + 1;
    const auto activeNow = sim.activeContractsByType(nextMonth);
    const double expectedNoise = (sim.config().demandNoiseMax > 0)
                                     ? (static_cast<double>(sim.config().demandNoiseMax) / 2.0)
                                     : 0.0;

    double expectedTotal = 0.0;
    const auto& off = sim.offers();
    for (int i = 0; i < 3; ++i) {
        Offer tmp = off[size_t(i)];
        const double demand = sim.computeDemand(tmp);
        const int expectedSold = std::max(0, static_cast<int>(std::lround(demand + expectedNoise)));
        const int expectedContracts = activeNow[size_t(i)] + expectedSold;
        const int effectiveMin = std::min(minC, expectedContracts);
        const int effectiveMax = std::min(maxC, expectedContracts);
        expectedTotal += (static_cast<double>(effectiveMin) + static_cast<double>(effectiveMax)) / 2.0;
    }

    expectedClaimsLabel->setText(
        QStringLiteral("Ожидаемое число страховых случаев в месяц: %1")
            .arg(QString::number(expectedTotal, 'f', 1)));

    for (int i = 0; i < 3; ++i) {
        const Offer& o = off[size_t(i)];
        ui[i].premium->setText(QString::number(o.getPremiumPerMonth(), 'f', 0));
        ui[i].months->setText(QString::number(o.getContractMonths()));
        ui[i].demand->setText(QString::number(o.getCurDemand(), 'f', 0));
        ui[i].maxCov->setText(QString::number(o.getMaxCoverage(), 'f', 0));
        ui[i].baseDemand->setText(QString::number(o.getBaseDemand()));
        ui[i].offerValid->setText(QString::number(o.getOfferValidMonths()));
    }

    for (int ti = 0; ti < 3; ++ti) {
        auto* lay = archiveCols[ti];
        const auto t = static_cast<InsuranceType>(ti);
        const auto& arc = sim.offerArchive(t);
        const auto& sold = sim.offerArchiveContractsSold(t);

        int shown = 0; 
        for (int j = 0; j < static_cast<int>(arc.size()); ++j) {
            const int cntSold = (j < static_cast<int>(sold.size()) ? sold[size_t(j)] : 0);
            if (cntSold <= 0) continue; 
            shown += 1;

            const int k = shown - 1;
            if (k >= static_cast<int>(archiveUi[size_t(ti)].size())) {
                ArchiveRowUi x;
                x.row = new QFrame;
                x.row->setStyleSheet(QStringLiteral(
                    "QFrame{border:1px solid #000000; background:#ffffff;}"));
                auto* h = new QHBoxLayout;
                h->setContentsMargins(4, 4, 4, 4);

                x.btn = new QPushButton;
                x.btn->setMaximumWidth(110);
                x.btn->setStyleSheet(QStringLiteral(
                    "QPushButton{border:1px solid #000000; background:#ffffff; padding:2px 6px;}"));

                x.cnt = new QLabel(QStringLiteral("0"));
                x.cnt->setFrameStyle(QFrame::NoFrame);
                x.cnt->setAlignment(Qt::AlignCenter);
                x.cnt->setMinimumWidth(70);
                x.cnt->setStyleSheet(QStringLiteral(
                    "QLabel{border:1px solid #000000; background:#ffffff; padding:2px;}"));

                QObject::connect(x.btn, &QPushButton::clicked, [window, &sim, btn = x.btn]() {
                    const int ti = btn->property("ti").toInt();
                    const int idx = btn->property("arcIdx").toInt();
                    const auto t = static_cast<InsuranceType>(ti);
                    const auto& arc = sim.offerArchive(t);
                    if (idx < 0 || idx >= static_cast<int>(arc.size())) return;
                    Offer tmp = arc[size_t(idx)];
                    tmp.setCurDemand(sim.computeDemand(tmp));
                    showCenteredMessageBox(
                        window, QMessageBox::Information, QStringLiteral("Страховка (архив)"),
                        QStringLiteral("premiumPerMonth: %1\ncurDemand: %2\nmaxCoverage: %3")
                            .arg(QString::number(tmp.getPremiumPerMonth(), 'f', 0))
                            .arg(QString::number(tmp.getCurDemand(), 'f', 0))
                            .arg(QString::number(tmp.getMaxCoverage(), 'f', 0)));
                });

                h->addWidget(x.btn);
                h->addWidget(x.cnt);
                x.row->setLayout(h);
                lay->addWidget(x.row);
                archiveUi[size_t(ti)].push_back(x);
            }

            auto& x = archiveUi[size_t(ti)][size_t(k)];
            x.row->setVisible(true);
            x.btn->setText(QStringLiteral("страховка%1").arg(shown));
            x.btn->setProperty("ti", ti);
            x.btn->setProperty("arcIdx", j);
            x.cnt->setText(QStringLiteral("%1").arg(cntSold));
        }

        for (int k = shown; k < static_cast<int>(archiveUi[size_t(ti)].size()); ++k) {
            archiveUi[size_t(ti)][size_t(k)].row->setVisible(false);
        }
    }

    const bool stop = sim.bankrupt() || sim.finished();
    nextBtn->setEnabled(gameStarted && !stop);
    endEarlyBtn->setEnabled(gameStarted && !stop);
    for (auto& x : ui) x.edit->setEnabled(!stop);
}

static bool startGameDialog(QWidget* parent, Config* cfg) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Start game"));

    auto* form = new QFormLayout;
    auto* m = new QSpinBox;
    m->setRange(6, 24);
    m->setValue(cfg ? cfg->M : 12);
    form->addRow(QStringLiteral("Количество месяцев (M)"), m);

    auto* cap = new QDoubleSpinBox;
    cap->setRange(0, 1e12);
    cap->setDecimals(2);
    cap->setValue(cfg ? cfg->initialCapital : 30000.0);
    form->addRow(QStringLiteral("Начальный капитал компании"), cap);

    auto* tax = new QDoubleSpinBox;
    tax->setRange(0.0, 1.0);
    tax->setDecimals(4);
    tax->setSingleStep(0.01);
    tax->setValue(cfg ? cfg->taxRate : 0.09);
    form->addRow(QStringLiteral("Налог на капитал (taxRate, 0..1)"), tax);

    auto* bHome = new QSpinBox;
    auto* bCar = new QSpinBox;
    auto* bHealth = new QSpinBox;
    for (auto* b : {bHome, bCar, bHealth}) b->setRange(0, 1000000);
    if (cfg) {
        bHome->setValue(cfg->baseDemand[0]);
        bCar->setValue(cfg->baseDemand[1]);
        bHealth->setValue(cfg->baseDemand[2]);
    }
    form->addRow(QStringLiteral("Базовый спрос (Жильё)"), bHome);
    form->addRow(QStringLiteral("Базовый спрос (Авто)"), bCar);
    form->addRow(QStringLiteral("Базовый спрос (Здоровье)"), bHealth);

    auto* noise = new QSpinBox;
    noise->setRange(0, 1000000);
    noise->setValue(cfg ? cfg->demandNoiseMax : 10);
    form->addRow(QStringLiteral("Шум спроса (0..N)"), noise);

    auto* minCl = new QSpinBox;
    auto* maxCl = new QSpinBox;
    minCl->setRange(0, 1000000);
    maxCl->setRange(0, 1000000);
    minCl->setValue(cfg ? cfg->minClaimsPerMonth : 1);
    maxCl->setValue(cfg ? cfg->maxClaimsPerMonth : 25);
    form->addRow(QStringLiteral("Случаи/мес: минимум"), minCl);
    form->addRow(QStringLiteral("Случаи/мес: максимум"), maxCl);

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
        cap->interpretText();
        tax->interpretText();
        bHome->interpretText();
        bCar->interpretText();
        bHealth->interpretText();
        noise->interpretText();
        minCl->interpretText();
        maxCl->interpretText();
        if (cfg) {
            cfg->M = m->value();
            cfg->initialCapital = cap->value();
            cfg->taxRate = tax->value();
            cfg->baseDemand[0] = bHome->value();
            cfg->baseDemand[1] = bCar->value();
            cfg->baseDemand[2] = bHealth->value();
            cfg->demandNoiseMax = noise->value();
            cfg->minClaimsPerMonth = minCl->value();
            cfg->maxClaimsPerMonth = maxCl->value();
        }
        dlg.accept();
    });
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.adjustSize();
    if (parent) {
        const QPoint center = parent->frameGeometry().center();
        dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
    }

    if (dlg.exec() != QDialog::Accepted) return false;
    m->interpretText();
    cap->interpretText();
    tax->interpretText();
    bHome->interpretText();
    bCar->interpretText();
    bHealth->interpretText();
    noise->interpretText();
    minCl->interpretText();
    maxCl->interpretText();
    if (cfg) {
        cfg->M = m->value();
        cfg->initialCapital = cap->value();
        cfg->taxRate = tax->value();
        cfg->baseDemand[0] = bHome->value();
        cfg->baseDemand[1] = bCar->value();
        cfg->baseDemand[2] = bHealth->value();
        cfg->demandNoiseMax = noise->value();
        cfg->minClaimsPerMonth = minCl->value();
        cfg->maxClaimsPerMonth = maxCl->value();
    }
    return true;
}

static bool editOffer(QWidget* parent, Offer* o) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Условия страховки"));

    auto* form = new QFormLayout;

    auto* prem = new QDoubleSpinBox;
    prem->setRange(0, 1e9);
    prem->setDecimals(2);
    prem->setValue(o->getPremiumPerMonth());

    auto* dur = new QSpinBox;
    dur->setRange(0, 120);
    dur->setValue(o->getContractMonths());

    auto* maxC = new QDoubleSpinBox;
    maxC->setRange(0, 1e12);
    maxC->setDecimals(2);
    maxC->setValue(o->getMaxCoverage());

    auto* ded = new QDoubleSpinBox;
    ded->setRange(0, 1e12);
    ded->setDecimals(2);
    ded->setValue(o->getDeductible());

    auto* valid = new QSpinBox;
    valid->setRange(0, 120);
    valid->setValue(o->getOfferValidMonths());

    form->addRow(QStringLiteral("Взнос (у.е./мес)"), prem);
    form->addRow(QStringLiteral("Срок договора (мес)"), dur);
    form->addRow(QStringLiteral("Макс. возмещение"), maxC);
    form->addRow(QStringLiteral("Франшиза"), ded);
    form->addRow(QStringLiteral("Срок действия условий (мес)"), valid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* lay = new QVBoxLayout;
    lay->addLayout(form);
    lay->addWidget(buttons);
    dlg.setLayout(lay);

    if (dlg.exec() != QDialog::Accepted) return false;

    o->setPremiumPerMonth(prem->value());
    o->setContractMonths(dur->value());
    o->setMaxCoverage(maxC->value());
    o->setDeductible(ded->value());
    o->setOfferValidMonths(valid->value());
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
    head->setAlignment(Qt::AlignCenter);
    head->setStyleSheet(QStringLiteral("font-size:16px; font-weight:700;"));
    v->addWidget(head);

    addKeyValueRow(v, QStringLiteral("premiumPerMonth"), &ui.premium);
    addKeyValueRow(v, QStringLiteral("contractMonths"), &ui.months);
    addKeyValueRow(v, QStringLiteral("curDemand"), &ui.demand);
    addKeyValueRow(v, QStringLiteral("maxCoverage"), &ui.maxCov);
    addKeyValueRow(v, QStringLiteral("baseDemand"), &ui.baseDemand);
    addKeyValueRow(v, QStringLiteral("offerValidMonths"), &ui.offerValid);

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
    double startCapital = cfg.initialCapital;

    QWidget window;
    window.setWindowTitle(QStringLiteral("Страховая компания — модель"));
    window.setStyleSheet(QStringLiteral(
        "QGroupBox{font-weight:600;} "
        "QPushButton{padding:4px 8px; border:1px solid #000000; background:#ffffff;}"));

    auto* main = new QVBoxLayout;

    auto* top = new QHBoxLayout;
    auto* month = new QLabel;
    month->setAlignment(Qt::AlignCenter);
    month->setStyleSheet(QStringLiteral("font-size:28px; font-weight:700;"));

    auto* endEarlyBtn = new QPushButton(QStringLiteral("Досрочно завершить игру"));
    endEarlyBtn->setEnabled(false);
    auto* nextBtn = new QPushButton(QStringLiteral("Следующий месяц"));
    nextBtn->setEnabled(false);
    top->addWidget(endEarlyBtn, 0, Qt::AlignLeft);
    top->addStretch();
    top->addWidget(month);
    top->addStretch();
    top->addWidget(nextBtn, 0, Qt::AlignRight);

    auto* center = new QHBoxLayout;

    auto* left = new QWidget;
    left->setFixedWidth(720);
    auto* leftV = new QVBoxLayout;
    auto* title = new QLabel(QStringLiteral("Актуальные предложения страхования"));
    title->setAlignment(Qt::AlignCenter);
    leftV->addWidget(title);

    auto* expectedClaimsFrame = new QFrame;
    expectedClaimsFrame->setStyleSheet(QStringLiteral(
        "QFrame{border:1px solid #000000; background:#fafafa;}"));
    auto* expectedClaimsLabel = new QLabel;
    expectedClaimsLabel->setAlignment(Qt::AlignCenter);
    auto* expectedClaimsLay = new QVBoxLayout;
    expectedClaimsLay->setContentsMargins(8, 6, 8, 6);
    expectedClaimsLay->addWidget(expectedClaimsLabel);
    expectedClaimsFrame->setLayout(expectedClaimsLay);
    leftV->addWidget(expectedClaimsFrame);

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

    auto* archiveTitle = new QLabel(QStringLiteral("Архив"));
    archiveTitle->setAlignment(Qt::AlignCenter);
    archiveTitle->setStyleSheet(QStringLiteral("font-size:14px; font-weight:700;"));
    leftV->addWidget(archiveTitle);

    auto* archiveRow = new QHBoxLayout;
    std::array<QVBoxLayout*, 3> archiveCols{};
    std::array<QWidget*, 3> archiveColsHost{};
    std::array<std::vector<ArchiveRowUi>, 3> archiveUi{};

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

    auto* table = new QTableWidget(0, 4);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("Капитал"),
         QStringLiteral("Выплаты"),
         QStringLiteral("Страховые\nслучаи"),
         QStringLiteral("Договоров\nв месяце")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->horizontalHeader()->setTextElideMode(Qt::ElideNone);
    table->horizontalHeader()->setMinimumHeight(44);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(28);

    center->addWidget(left, 2);
    center->addWidget(table, 5);

    refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);

    QObject::connect(nextBtn, &QPushButton::clicked, [&]() {
        if (sim.needsOfferUpdate()) {
            showCenteredMessageBox(&window, QMessageBox::Warning,
                                   QStringLiteral("Требуется обновление"),
                                   expiredOffersMessage(sim.expiredOffers()));
            refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
            return;
        }
        MonthResult r;
        const bool ok = sim.step(&r);
        if (!ok) {
            if (sim.bankrupt()) {
                appendMonthRow(table, r);
                const double loss = startCapital - r.capitalAfter;
                showCenteredMessageBox(
                    &window, QMessageBox::Warning, QStringLiteral("Поражение"),
                    QStringLiteral("Разорение в месяце %1.\nИтоговый капитал: %2\nУбыток: %3")
                        .arg(r.month)
                        .arg(money(r.capitalAfter))
                        .arg(money(loss)));
                highlightTableLastRow(table, QColor(255, 235, 235)); // светло-красный
                refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
                return;
            }

            if (sim.needsOfferUpdate()) {
                showCenteredMessageBox(&window, QMessageBox::Warning,
                                       QStringLiteral("Требуется обновление"),
                                       expiredOffersMessage(sim.expiredOffers()));
            }
            refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
            return;
        }
        appendMonthRow(table, r);

        if (sim.finished()) {
            const double gain = r.capitalAfter - startCapital;
            showCenteredMessageBox(&window, QMessageBox::Information, QStringLiteral("Победа"),
                                   QStringLiteral("Игра завершена.\nИтоговый капитал: %1\nПрирост капитала: %2")
                                       .arg(money(r.capitalAfter))
                                       .arg(money(gain)));
            highlightTableLastRow(table, gain >= 0 ? QColor(235, 255, 235) : QColor(255, 235, 235));
        }
        refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
    });

    QObject::connect(endEarlyBtn, &QPushButton::clicked, [&]() {
        const auto ans = QMessageBox::question(
            &window, QStringLiteral("Досрочное завершение"),
            QStringLiteral("Завершить игру досрочно?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes) return;
        sim.finishEarly();
        const double gain = sim.capital() - startCapital;
        showCenteredMessageBox(
            &window, QMessageBox::Information, QStringLiteral("Игра завершена"),
            QStringLiteral("Игра досрочно остановлена.\nТекущий капитал: %1\nИзменение капитала: %2")
                .arg(money(sim.capital()))
                .arg(money(gain)));
        highlightTableLastRow(table, gain >= 0 ? QColor(235, 255, 235) : QColor(255, 235, 235));
        refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
    });

    for (int ti = 0; ti < 3; ++ti) {
        QObject::connect(ui[ti].edit, &QPushButton::clicked, [&, ti]() {
            InsuranceType t = static_cast<InsuranceType>(ti);
            Offer o = sim.offers()[static_cast<size_t>(ti)];
            if (!editOffer(&window, &o)) return;
            sim.setOffer(t, o);
            refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
        });
    }

    main->addLayout(top);
    main->addLayout(center, 1);
    window.setLayout(main);

    refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);
    window.resize(1200, 720);
    window.show();

    Config gameCfg = sim.config();
    if (!startGameDialog(&window, &gameCfg)) return 0;
    sim.setConfig(gameCfg);
    cfg = gameCfg;
    gameStarted = true;
    startCapital = gameCfg.initialCapital;
    table->setRowCount(0);
    refreshUi(sim, month, expectedClaimsLabel, ui, archiveCols, archiveUi, nextBtn, endEarlyBtn, gameStarted, &window);

    return app.exec();
}