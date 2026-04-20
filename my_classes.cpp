#include "my_classes.h"

#include <algorithm>
#include <cmath>
#include <vector>

Simulation::Simulation(Config cfg) : cfg_(cfg) {
    offers_[0] = Offer{InsuranceType::Home, 90.0, 12, 20000.0, 800.0, 6};
    offers_[1] = Offer{InsuranceType::Car, 120.0, 12, 15000.0, 600.0, 6};
    offers_[2] = Offer{InsuranceType::Health, 150.0, 12, 25000.0, 1000.0, 6};
    reset();
}

void Simulation::setConfig(const Config& c) {
    cfg_ = c;
    reset();
}

void Simulation::setOffer(InsuranceType t, const Offer& o) {
    Offer x = o;
    x.type = t;
    const size_t i = static_cast<size_t>(idx(t));
    offers_[i] = x;
    offerArchive_[i].push_back(x);
    offerArchiveContractsSold_[i].push_back(0);
}

const std::vector<Offer>& Simulation::offerArchive(InsuranceType t) const {
    return offerArchive_[static_cast<size_t>(idx(t))];
}

const std::vector<int>& Simulation::offerArchiveContractsSold(InsuranceType t) const {
    return offerArchiveContractsSold_[static_cast<size_t>(idx(t))];
}

void Simulation::finishEarly() {
    if (bankrupt_ || finished_) return;
    finished_ = true;
}

void Simulation::reset() {
    if (cfg_.seed == 0) {
        std::random_device rd;
        rng_.seed(rd());
    } else {
        rng_.seed(cfg_.seed);
    }

    month_ = 0;
    capital_ = cfg_.initialCapital;
    bankrupt_ = false;
    finished_ = false;
    contracts_.clear();
    for (auto& v : offerArchive_) v.clear();
    for (auto& v : offerArchiveContractsSold_) v.clear();

    for (int i = 0; i < 3; ++i) {
        offers_[static_cast<size_t>(i)].baseDemand = cfg_.baseDemand[static_cast<size_t>(i)];
        offers_[static_cast<size_t>(i)].curDemand = 0.0;
        offerArchive_[static_cast<size_t>(i)].push_back(offers_[static_cast<size_t>(i)]);
        offerArchiveContractsSold_[static_cast<size_t>(i)].push_back(0);
    }
}

double Simulation::urand01() {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(rng_);
}

int Simulation::irand(int lo, int hi) {
    if (lo > hi) std::swap(lo, hi);
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng_);
}

double Simulation::computeDemand(const Offer& o) const {
    const int i = idx(o.type);
    (void)i;
    const int base = std::max(0, o.baseDemand);
    const double price = std::max(o.totalPrice(), 0.0);
    const double cov = std::max(o.maxCoverage, 1e-9);
    const double ratio = price / cov;
    const double mult = 1.0 / std::max(ratio, 1e-6);
    const double capped = std::min(mult, 5.0);
    return static_cast<double>(base) * capped;
}

bool Simulation::step(MonthResult* out) {
    if (bankrupt_ || finished_) return false;
    if (month_ >= cfg_.M) {
        finished_ = true;
        return false;
    }

    MonthResult r;
    r.month = month_ + 1;
    r.capitalBefore = capital_;

    r.taxPaid = capital_ * cfg_.taxRate;
    capital_ -= r.taxPaid;

    contracts_.erase(std::remove_if(contracts_.begin(), contracts_.end(),
                                    [&](const Contract& c) { return !c.isActive(r.month); }),
                     contracts_.end());

    for (auto& c : contracts_) c.ensuredEvents = false;

    for (int i = 0; i < 3; ++i) {
        Offer& o = offers_[static_cast<size_t>(i)];
        o.baseDemand = cfg_.baseDemand[static_cast<size_t>(i)];
        o.curDemand = computeDemand(o);
        r.effectiveDemand[i] = o.curDemand;

        const int noise = cfg_.demandNoiseMax > 0 ? irand(0, cfg_.demandNoiseMax) : 0;
        const int sold = std::max(0, static_cast<int>(std::lround(o.curDemand)) + noise);
        r.sold[i] = sold;

        r.revenue[i] = static_cast<double>(sold) * o.totalPrice();
        capital_ += r.revenue[i];

        if (sold > 0) {
            const size_t typeIdx = static_cast<size_t>(i);
            if (!offerArchiveContractsSold_[typeIdx].empty()) {
                offerArchiveContractsSold_[typeIdx].back() += sold;
            }

            Contract c;
            c.offerSnapshot = o;
            c.startMonth = r.month;
            c.endMonth = r.month + std::max(1, o.contractMonths) - 1;
            for (int k = 0; k < sold; ++k) contracts_.push_back(c);
        }
    }

    r.totalClaims = 0.0;
    for (int i = 0; i < 3; ++i) {
        const InsuranceType t = static_cast<InsuranceType>(i);
        std::vector<Contract*> active;
        active.reserve(contracts_.size());
        for (auto& c : contracts_) {
            if (c.type() == t && c.isActive(r.month)) active.push_back(&c);
        }

        const int maxPossible = static_cast<int>(active.size());
        int n = 0;
        if (maxPossible > 0) {
            n = irand(cfg_.minClaimsPerMonth, cfg_.maxClaimsPerMonth);
            n = std::min(n, maxPossible);
        }
        r.claimsCount[i] = n;

        double paid = 0.0;
        for (int k = 0; k < n; ++k) {
            const int pick = irand(0, static_cast<int>(active.size()) - 1);
            Contract* c = active[static_cast<size_t>(pick)];
            const Offer& o = c->offerSnapshot;
            c->ensuredEvents = true;
            active.erase(active.begin() + pick);

            double u = urand01();
            if (u <= 0.0) u = 1e-12;
            const double damage = o.maxCoverage * u;
            const double pay = std::max(0.0, damage - o.deductible);
            paid += std::min(pay, o.maxCoverage);
        }

        r.claimsPaid[i] = paid;
        r.totalClaims += paid;
    }

    if (capital_ + 1e-9 < r.totalClaims) {
        bankrupt_ = true;
        r.capitalAfter = capital_;
        month_ = r.month;
        if (out) *out = r;
        return false;
    }

    capital_ -= r.totalClaims;
    r.capitalAfter = capital_;

    month_ = r.month;
    if (month_ >= cfg_.M) finished_ = true;

    if (out) *out = r;
    return true;
}