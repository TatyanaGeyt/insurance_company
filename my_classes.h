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

struct Offer {
    InsuranceType type = InsuranceType::Home;
    double premiumPerMonth = 90.0;   // у.е. / месяц
    int contractMonths = 12;         // длительность договора
    double maxCoverage = 20000.0;    // макс. возмещение
    double deductible = 500.0;       // франшиза
    int offerValidMonths = 6;        // сколько месяцев действуют условия
    int baseDemand = 0;              // базовый спрос (параметр моделирования)
    double curDemand = 0.0;          // текущий спрос (пересчитывается каждый месяц)

    double totalPrice() const { return premiumPerMonth * contractMonths; }
    double getMonthlyPremium() const { return premiumPerMonth; }
};

struct Config {
    int M = 12;
    double initialCapital = 30000.0;
    double taxRate = 0.09;

    std::array<int, 3> baseDemand{50, 40, 45};
    int demandNoiseMax = 10;         // случайная добавка к спросу (0..max), см. условие

    int minClaimsPerMonth = 1;
    int maxClaimsPerMonth = 25;

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

private:
    struct Contract {
        Offer offerSnapshot{};
        int startMonth = 0;
        int endMonth = 0;
        bool ensuredEvents = false; // страховой случай в текущем месяце (True/False)

        bool isActive(int month) const { return month >= startMonth && month <= endMonth; }
        InsuranceType type() const { return offerSnapshot.type; }
    };

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

