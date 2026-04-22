#include "my_classes.h"

#include <algorithm>
#include <cmath>
#include <vector>

Simulation::Simulation(Config cfg) : cfg_(cfg) {
    offers_[0].type_ = InsuranceType::Home;
    offers_[1].type_ = InsuranceType::Car;
    offers_[2].type_ = InsuranceType::Health;
    reset();
}

bool Simulation::needsOfferUpdate() const {
    const auto e = expiredOffers();
    return e[0] || e[1] || e[2];
}

std::array<bool, 3> Simulation::expiredOffers() const {
    std::array<bool, 3> e{};
    for (int i = 0; i < 3; ++i) {
        e[static_cast<size_t>(i)] = (offers_[static_cast<size_t>(i)].getOfferValidMonths() <= 0);
    }
    return e;
}

void Simulation::setConfig(const Config& c) {
    cfg_ = c;
    reset();
}

void Simulation::setOffer(InsuranceType t, const Offer& o) {
    const size_t i = static_cast<size_t>(idx(t));
    offers_[i] = o;
    offerArchive_[i].push_back(o);
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
        offers_[static_cast<size_t>(i)].setBaseDemand(cfg_.baseDemand[static_cast<size_t>(i)]);
        offers_[static_cast<size_t>(i)].setCurDemand(0.0);
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
    const int base = std::max(0, o.getBaseDemand());
    const double price = std::max(o.totalPrice(), 0.0);
    const double cov = std::max(o.getMaxCoverage(), 1e-9);
    const double ratio = price / cov;
    const double mult = 1.0 / std::max(ratio, 1e-6);
    // const double capped = std::min(mult, 5.0);
    // return static_cast<double>(base) * capped; // жёсткий потолок на спрос
    return static_cast<double>(base) * mult;
}

std::array<int, 3> Simulation::activeContractsByType(int month) const {
    std::array<int, 3> out{};
    for (const auto& c : contracts_) {
        if (!c.isActive(month)) continue;
        const int ti = idx(c.type());
        if (ti >= 0 && ti < 3) out[static_cast<size_t>(ti)] += 1;
    }
    return out;
}

bool Simulation::step(MonthResult* out) {
    if (bankrupt_ || finished_) return false;
    if (needsOfferUpdate()) return false; // нельзя идти дальше, пока не обновлены истёкшие предложения
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

    for (int i = 0; i < 3; ++i) {
        const size_t typeIdx = static_cast<size_t>(i);
        Offer& o = offers_[static_cast<size_t>(i)];
        o.setBaseDemand(cfg_.baseDemand[static_cast<size_t>(i)]);
        o.setCurDemand(computeDemand(o));
        r.effectiveDemand[i] = o.getCurDemand();

        if (o.getOfferValidMonths() <= 0) {
            r.sold[i] = 0;
            r.revenue[i] = 0.0;
            continue;
        }

        const int noise = cfg_.demandNoiseMax > 0 ? irand(0, cfg_.demandNoiseMax) : 0;
        const int sold = std::max(0, static_cast<int>(std::lround(o.getCurDemand())) + noise);
        r.sold[i] = sold;
        r.revenue[i] = 0.0;

        if (sold > 0) {
            if (!offerArchiveContractsSold_[typeIdx].empty()) {
                offerArchiveContractsSold_[typeIdx].back() += sold;
            }

            Contract c;
            c.offerSnapshot = o;
            c.startMonth = r.month;
            c.endMonth = r.month + std::max(1, o.getContractMonths()) - 1;
            for (int k = 0; k < sold; ++k) contracts_.push_back(c);
        }
    }

    for (const auto& c : contracts_) {
        if (!c.isActive(r.month)) continue;
        const int ti = idx(c.type());
        if (ti < 0 || ti >= 3) continue;
        const double prem = std::max(0.0, c.offerSnapshot.getPremiumPerMonth());
        r.revenue[static_cast<size_t>(ti)] += prem;
    }
    capital_ += (r.revenue[0] + r.revenue[1] + r.revenue[2]);

    r.totalClaims = 0.0;
    for (int i = 0; i < 3; ++i) {
        const InsuranceType t = static_cast<InsuranceType>(i);
        std::vector<Contract*> active;
        active.reserve(contracts_.size());
        for (auto& c : contracts_) {
            if (c.type() == t && c.isActive(r.month)) active.push_back(&c);
        }

        const int maxPossible = static_cast<int>(active.size());
        int n = 0, curMaxInsEvents=0, curMinInsEvents=0;
        if (maxPossible > 0) {
            curMinInsEvents = std::min(cfg_.minClaimsPerMonth, maxPossible);
            curMaxInsEvents = std::min(cfg_.maxClaimsPerMonth, maxPossible);
            n = irand(curMinInsEvents, curMaxInsEvents);
            n = std::min(n, maxPossible);
        }
        r.claimsCount[i] = n;

        double paid = 0.0;
        for (int k = 0; k < n; ++k) {
            const int pick = irand(0, static_cast<int>(active.size()) - 1);
            Contract* c = active[static_cast<size_t>(pick)];
            const Offer& o = c->offerSnapshot;
            active.erase(active.begin() + pick);

            double u = urand01();
            if (u <= 0.0) u = 1e-12;
            const double damage = o.getMaxCoverage() * u;
            
            // Проверка: страховая выплата начинается только если ущерб >= франшизы
            if (damage >= o.getDeductible()) {
                paid += std::min(damage, o.getMaxCoverage());
            }
        }

        r.claimsPaid[i] = paid;
        r.totalClaims += paid;
    }

    if (capital_ + 1e-9 < r.totalClaims) {
        bankrupt_ = true;
        r.capitalAfter = capital_ - r.totalClaims;
        capital_ = r.capitalAfter;
        month_ = r.month;
        if (out) *out = r;
        return false;
    }

    capital_ -= r.totalClaims;
    r.capitalAfter = capital_;

    month_ = r.month;
    if (month_ >= cfg_.M) finished_ = true;

    for (int i = 0; i < 3; ++i) {
        const size_t typeIdx = static_cast<size_t>(i);
        const int cur = offers_[typeIdx].getOfferValidMonths();
        if (cur <= 0) continue;
        const int left = cur - 1;
        offers_[typeIdx].setOfferValidMonths(std::max(0, left));
    }

    if (out) *out = r;
    return true;
}