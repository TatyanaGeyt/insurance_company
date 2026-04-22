#pragma once

#include <array>
#include <random>
#include <string>
#include <vector>

enum class InsuranceType { Home = 0, Car = 1, Health = 2 };

inline const char* insuranceTypeName(InsuranceType t) {
    switch (t) {
        case InsuranceType::Home: return "Жильё";
        case InsuranceType::Car: return "Авто";
        case InsuranceType::Health: return "Здоровье";
        default: return "?";
    }
}

class Offer {
public:
    InsuranceType getType() const { return type_; }

    double getPremiumPerMonth() const { return premiumPerMonth_; }
    void setPremiumPerMonth(double v) { premiumPerMonth_ = v; }

    int getContractMonths() const { return contractMonths_; }
    void setContractMonths(int v) { contractMonths_ = v; }

    double getMaxCoverage() const { return maxCoverage_; }
    void setMaxCoverage(double v) { maxCoverage_ = v; }

    double getDeductible() const { return deductible_; }
    void setDeductible(double v) { deductible_ = v; }

    int getOfferValidMonths() const { return offerValidMonths_; }
    void setOfferValidMonths(int v) { offerValidMonths_ = v; }

    int getBaseDemand() const { return baseDemand_; }
    void setBaseDemand(int v) { baseDemand_ = v; }

    double getCurDemand() const { return curDemand_; }
    void setCurDemand(double v) { curDemand_ = v; }

    double totalPrice() const {
        return getPremiumPerMonth() * static_cast<double>(getContractMonths());
    }

private:
    InsuranceType type_ = InsuranceType::Home;
    friend class Simulation;

    double premiumPerMonth_ = 100.0;   // у.е. / месяц
    int contractMonths_ = 12;         // длительность договора
    double maxCoverage_ = 200.0;    // макс. возмещение
    double deductible_ = 100.0;       // франшиза
    int offerValidMonths_ = 12;        // сколько месяцев действуют условия
    int baseDemand_ = 0;              // базовый спрос
    double curDemand_ = 0.0;          // текущий спрос
};

struct Contract {
    Offer offerSnapshot{};
    int startMonth = 0;
    int endMonth = 0;

    bool isActive(int month) const { return month >= startMonth && month <= endMonth; }
    InsuranceType type() const { return offerSnapshot.getType(); }
};

struct Config {
    int M = 12;
    double initialCapital = 30000.0;
    double taxRate = 0.1;

    std::array<int, 3> baseDemand{50, 50, 50};
    int demandNoiseMax = 10;         // случайная добавка к спросу (0..max), см. условие

    int minClaimsPerMonth = 10;
    int maxClaimsPerMonth = 250;

    unsigned seed = 0;
};

struct MonthResult {
    int month = 0;
    double capitalBefore = 0.0;
    double taxPaid = 0.0;
    std::array<int, 3> sold{};
    std::array<double, 3> revenue{};
    std::array<int, 3> claimsCount{};
    std::array<double, 3> claimsPaid{};
    double totalClaims = 0.0;
    double capitalAfter = 0.0;
    std::array<double, 3> effectiveDemand{};
};

class Simulation {
public:
    explicit Simulation(Config cfg = {});

    const Config& config() const { return cfg_; }
    void setConfig(const Config& c);

    const std::array<Offer, 3>& offers() const { return offers_; }
    void setOffer(InsuranceType t, const Offer& o);
    const std::vector<Offer>& offerArchive(InsuranceType t) const;
    const std::vector<int>& offerArchiveContractsSold(InsuranceType t) const;

    int currentMonth() const { return month_; }
    double capital() const { return capital_; }
    bool bankrupt() const { return bankrupt_; }
    bool finished() const { return finished_; }

    void reset();
    void finishEarly();

    bool step(MonthResult* out); // один месяц: налог -> продажи -> выплаты
    double computeDemand(const Offer& o) const;
    std::array<int, 3> activeContractsByType(int month) const;

    bool needsOfferUpdate() const;
    std::array<bool, 3> expiredOffers() const;

private:
    int idx(InsuranceType t) const { return static_cast<int>(t); }

    double urand01();
    int irand(int lo, int hi);

    Config cfg_{};
    std::array<Offer, 3> offers_{};
    std::array<std::vector<Offer>, 3> offerArchive_{};
    std::array<std::vector<int>, 3> offerArchiveContractsSold_{};
    std::vector<Contract> contracts_{};

    int month_ = 0;
    double capital_ = 0.0;
    bool bankrupt_ = false;
    bool finished_ = false;

    std::mt19937 rng_;
};

