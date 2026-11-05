#ifndef PRACTICE_WORKER_H
#define PRACTICE_WORKER_H

#include <iosfwd>
#include <list>
#include <string>
#include <vector>

namespace practice {

/// Класс WORKER описывает сотрудника организации.
///
/// Поля класса:
///  * фамилия и инициалы работника;
///  * название занимаемой должности;
///  * зарплата (оклад, руб.);
///  * год поступления на работу.
///
/// Все поля закрыты (инкапсуляция), доступ к ним выполняется только
/// через методы доступа и изменения, которые проверяют корректность данных.
class Worker {
public:
    /// Конструктор по умолчанию: создаёт «пустого» сотрудника,
    /// принятого в текущем году (стаж = 0).
    Worker();

    /// Основной конструктор с параметрами.
    /// @throw std::invalid_argument если ФИО/должность пустые,
    ///        оклад отрицательный или год поступления некорректен.
    Worker(const std::string& fullName,
           const std::string& position,
           double salary,
           int hireYear);

    /// Дополнительный конструктор: сотрудник без указания оклада и года,
    /// год поступления принимается равным текущему.
    Worker(const std::string& fullName, const std::string& position);

    /// Конструктор копирования и оператор присваивания (глубокая копия
    /// строковых полей выполняется стандартными средствами std::string).
    Worker(const Worker& other);
    Worker& operator=(const Worker& other);

    /// Деструктор.
    ~Worker();

    // --- Методы доступа (геттеры) -------------------------------------
    const std::string& fullName() const noexcept;
    const std::string& position() const noexcept;
    double salary() const noexcept;
    int hireYear() const noexcept;

    // --- Методы изменения (сеттеры) -----------------------------------
    void setFullName(const std::string& fullName);
    void setPosition(const std::string& position);
    void setSalary(double salary);
    void setHireYear(int hireYear);

    /// Изменение всех полей сотрудника одной операцией.
    void edit(const std::string& fullName,
              const std::string& position,
              double salary,
              int hireYear);

    /// Повышение оклада на percent процентов (percent >= 0).
    void raiseSalary(double percent);

    // --- Методы согласно заданию --------------------------------------
    /// Стаж работы в организации на указанный год (полных лет).
    int experience(int currentYear) const noexcept;

    /// True, если стаж на указанный год строго больше years.
    bool experienceExceeds(int years, int currentYear) const noexcept;

    // --- Методы отображения -------------------------------------------
    /// Текстовое представление сотрудника на указанный год.
    std::string toString(int currentYear) const;

    /// Текстовое представление сотрудника на текущий год.
    std::string toString() const;

    /// Вывод сотрудника в поток на указанный год.
    void print(std::ostream& os, int currentYear) const;

    /// Вывод сотрудника в поток на текущий год.
    void print(std::ostream& os) const;

    // --- Вспомогательные статические методы ---------------------------
    /// Проверка корректности года поступления: 1900 <= year <= текущий год.
    static bool isValidYear(int year) noexcept;

    /// Текущий год по системным часам.
    static int currentYear() noexcept;

private:
    std::string fullName_;  ///< фамилия и инициалы
    std::string position_;  ///< должность
    double salary_;         ///< оклад, руб.
    int hireYear_;          ///< год поступления на работу
};

/// Список сотрудников (стандартный контейнер std::list, как требует задание).
using WorkerList = std::list<Worker>;

/// Отбор сотрудников, чей стаж на указанный год строго больше minYears.
/// Возвращает указатели на элементы исходного списка; список должен
/// существовать на момент использования результата.
std::vector<const Worker*> selectByMinExperience(const WorkerList& workers,
                                                 int minYears,
                                                 int currentYear);

/// Вывод сотрудника в поток (эквивалент print).
std::ostream& operator<<(std::ostream& os, const Worker& worker);

/// Сравнение сотрудников по всем полям.
bool operator==(const Worker& lhs, const Worker& rhs) noexcept;
bool operator!=(const Worker& lhs, const Worker& rhs) noexcept;

}  // namespace practice

#endif  // PRACTICE_WORKER_H
