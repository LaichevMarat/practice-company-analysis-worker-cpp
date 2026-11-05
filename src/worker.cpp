#include "worker.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace practice {
namespace {

/// Удаление пробельных символов в начале и в конце строки.
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

void requireNonEmpty(const std::string& value, const char* field) {
    if (trim(value).empty()) {
        throw std::invalid_argument(std::string("Поле «") + field +
                                    "» не может быть пустым");
    }
}

void requireNonNegativeSalary(double salary) {
    if (!std::isfinite(salary) || salary < 0.0) {
        throw std::invalid_argument("Оклад не может быть отрицательным");
    }
}

/// Слово «год» в нужной форме: 1 год, 2 года, 5 лет.
const char* yearsWord(int years) {
    const int lastTwo = years % 100;
    const int last = years % 10;
    if (lastTwo >= 11 && lastTwo <= 14) {
        return "лет";
    }
    if (last == 1) {
        return "год";
    }
    if (last >= 2 && last <= 4) {
        return "года";
    }
    return "лет";
}

std::string formatMoney(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value;
    return text.str();
}

}  // namespace

// ---------------------------------------------------------------------------
// Конструкторы и деструктор
// ---------------------------------------------------------------------------

Worker::Worker()
    : fullName_("Не указано"),
      position_("Не указано"),
      salary_(0.0),
      hireYear_(Worker::currentYear()) {}

Worker::Worker(const std::string& fullName,
               const std::string& position,
               double salary,
               int hireYear)
    : fullName_(trim(fullName)),
      position_(trim(position)),
      salary_(salary),
      hireYear_(hireYear) {
    requireNonEmpty(fullName_, "Фамилия и инициалы");
    requireNonEmpty(position_, "Должность");
    requireNonNegativeSalary(salary_);
    if (!isValidYear(hireYear_)) {
        throw std::invalid_argument(
            "Некорректный год поступления на работу: " +
            std::to_string(hireYear_));
    }
}

Worker::Worker(const std::string& fullName, const std::string& position)
    : Worker(fullName, position, 0.0, Worker::currentYear()) {}

Worker::Worker(const Worker& other) = default;

Worker& Worker::operator=(const Worker& other) = default;

Worker::~Worker() = default;

// ---------------------------------------------------------------------------
// Методы доступа
// ---------------------------------------------------------------------------

const std::string& Worker::fullName() const noexcept { return fullName_; }

const std::string& Worker::position() const noexcept { return position_; }

double Worker::salary() const noexcept { return salary_; }

int Worker::hireYear() const noexcept { return hireYear_; }

// ---------------------------------------------------------------------------
// Методы изменения
// ---------------------------------------------------------------------------

void Worker::setFullName(const std::string& fullName) {
    requireNonEmpty(fullName, "Фамилия и инициалы");
    fullName_ = trim(fullName);
}

void Worker::setPosition(const std::string& position) {
    requireNonEmpty(position, "Должность");
    position_ = trim(position);
}

void Worker::setSalary(double salary) {
    requireNonNegativeSalary(salary);
    salary_ = salary;
}

void Worker::setHireYear(int hireYear) {
    if (!isValidYear(hireYear)) {
        throw std::invalid_argument(
            "Некорректный год поступления на работу: " +
            std::to_string(hireYear));
    }
    hireYear_ = hireYear;
}

void Worker::edit(const std::string& fullName,
                  const std::string& position,
                  double salary,
                  int hireYear) {
    // Сначала проверяем все значения, чтобы не оставить объект
    // в частично изменённом состоянии.
    requireNonEmpty(fullName, "Фамилия и инициалы");
    requireNonEmpty(position, "Должность");
    requireNonNegativeSalary(salary);
    if (!isValidYear(hireYear)) {
        throw std::invalid_argument(
            "Некорректный год поступления на работу: " +
            std::to_string(hireYear));
    }

    fullName_ = trim(fullName);
    position_ = trim(position);
    salary_ = salary;
    hireYear_ = hireYear;
}

void Worker::raiseSalary(double percent) {
    if (!std::isfinite(percent) || percent < 0.0) {
        throw std::invalid_argument("Процент повышения не может быть отрицательным");
    }
    salary_ = salary_ * (1.0 + percent / 100.0);
}

// ---------------------------------------------------------------------------
// Методы согласно заданию
// ---------------------------------------------------------------------------

int Worker::experience(int currentYear) const noexcept {
    return currentYear > hireYear_ ? currentYear - hireYear_ : 0;
}

bool Worker::experienceExceeds(int years, int currentYear) const noexcept {
    return experience(currentYear) > years;
}

// ---------------------------------------------------------------------------
// Методы отображения
// ---------------------------------------------------------------------------

std::string Worker::toString(int currentYear) const {
    const int years = experience(currentYear);
    std::ostringstream text;
    text << fullName_ << " — " << position_ << ", оклад " << formatMoney(salary_)
         << " руб., год поступления " << hireYear_ << ", стаж " << years << ' '
         << yearsWord(years);
    return text.str();
}

std::string Worker::toString() const { return toString(Worker::currentYear()); }

void Worker::print(std::ostream& os, int currentYear) const {
    os << toString(currentYear);
}

void Worker::print(std::ostream& os) const { print(os, Worker::currentYear()); }

// ---------------------------------------------------------------------------
// Вспомогательные методы
// ---------------------------------------------------------------------------

bool Worker::isValidYear(int year) noexcept {
    return year >= 1900 && year <= Worker::currentYear();
}

int Worker::currentYear() noexcept {
    const std::time_t moment = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &moment);
#else
    localtime_r(&moment, &local);
#endif
    return local.tm_year + 1900;
}

// ---------------------------------------------------------------------------
// Свободные функции
// ---------------------------------------------------------------------------

std::vector<const Worker*> selectByMinExperience(const WorkerList& workers,
                                                 int minYears,
                                                 int currentYear) {
    std::vector<const Worker*> selected;
    for (const Worker& worker : workers) {
        if (worker.experienceExceeds(minYears, currentYear)) {
            selected.push_back(&worker);
        }
    }
    return selected;
}

std::ostream& operator<<(std::ostream& os, const Worker& worker) {
    worker.print(os);
    return os;
}

WorkerList makeDemoWorkers() {
    WorkerList workers;
    workers.push_back(Worker("Смирнов Д.А.", "Заведующий кафедрой", 180000.0, 2015));
    workers.push_back(Worker("Петрова А.С.", "Доцент", 120000.0, 2017));
    workers.push_back(Worker("Иванов И.И.", "Старший преподаватель", 95000.0, 2021));
    workers.push_back(
        Worker("Кузнецова Е.В.", "Инженер-программист", 110000.0, 2023));
    workers.push_back(Worker("Сидоров П.П.", "Специалист технической поддержки",
                             70000.0, 2025));
    return workers;
}

bool operator==(const Worker& lhs, const Worker& rhs) noexcept {
    return lhs.fullName() == rhs.fullName() && lhs.position() == rhs.position() &&
           lhs.salary() == rhs.salary() && lhs.hireYear() == rhs.hireYear();
}

bool operator!=(const Worker& lhs, const Worker& rhs) noexcept {
    return !(lhs == rhs);
}

}  // namespace practice
