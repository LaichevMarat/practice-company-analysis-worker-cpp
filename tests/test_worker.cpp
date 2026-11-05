#include "test_framework.h"
#include "worker.h"

#include <sstream>
#include <string>

using practice::selectByMinExperience;
using practice::Worker;
using practice::WorkerList;

namespace {

Worker makeWorker() {
    return Worker("Иванов И.И.", "Доцент", 120000.0, 2015);
}

}  // namespace

// ---------------------------------------------------------------------------
// Конструкторы
// ---------------------------------------------------------------------------

TF_TEST("Конструктор по умолчанию создаёт сотрудника со стажем 0") {
    Worker worker;
    TF_CHECK_EQ(worker.experience(Worker::currentYear()), 0);
    TF_CHECK_EQ(worker.salary(), 0.0);
    TF_CHECK_EQ(worker.hireYear(), Worker::currentYear());
    TF_CHECK(!worker.fullName().empty());
}

TF_TEST("Конструктор с параметрами сохраняет переданные значения") {
    Worker worker("Петрова А.С.", "Старший преподаватель", 95000.0, 2019);
    TF_CHECK_EQ(worker.fullName(), std::string("Петрова А.С."));
    TF_CHECK_EQ(worker.position(), std::string("Старший преподаватель"));
    TF_CHECK_EQ(worker.salary(), 95000.0);
    TF_CHECK_EQ(worker.hireYear(), 2019);
}

TF_TEST("Конструктор обрезает пробелы по краям текстовых полей") {
    Worker worker("  Сидоров П.П.  ", "  Инженер-программист  ", 110000.0, 2021);
    TF_CHECK_EQ(worker.fullName(), std::string("Сидоров П.П."));
    TF_CHECK_EQ(worker.position(), std::string("Инженер-программист"));
}

TF_TEST("Конструктор без оклада ставит оклад 0 и текущий год") {
    Worker worker("Кузнецова Е.В.", "Специалист технической поддержки");
    TF_CHECK_EQ(worker.salary(), 0.0);
    TF_CHECK_EQ(worker.hireYear(), Worker::currentYear());
}

TF_TEST("Конструктор отклоняет пустые ФИО и должность") {
    TF_CHECK_THROW(Worker("", "Доцент", 100.0, 2020), std::invalid_argument);
    TF_CHECK_THROW(Worker("   ", "Доцент", 100.0, 2020), std::invalid_argument);
    TF_CHECK_THROW(Worker("Иванов И.И.", "", 100.0, 2020), std::invalid_argument);
}

TF_TEST("Конструктор отклоняет отрицательный оклад") {
    TF_CHECK_THROW(Worker("Иванов И.И.", "Доцент", -1.0, 2020),
                   std::invalid_argument);
}

TF_TEST("Конструктор отклоняет некорректный год поступления") {
    TF_CHECK_THROW(Worker("Иванов И.И.", "Доцент", 100.0, 1800),
                   std::invalid_argument);
    TF_CHECK_THROW(
        Worker("Иванов И.И.", "Доцент", 100.0, Worker::currentYear() + 1),
        std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Копирование
// ---------------------------------------------------------------------------

TF_TEST("Копирование создаёт независимую копию объекта") {
    Worker original = makeWorker();
    Worker copy = original;
    copy.setSalary(1.0);

    TF_CHECK_EQ(original.salary(), 120000.0);
    TF_CHECK_EQ(copy.salary(), 1.0);
    TF_CHECK(original != copy);
}

TF_TEST("Оператор присваивания копирует все поля") {
    Worker source = makeWorker();
    Worker target;
    target = source;

    TF_CHECK(target == source);
    TF_CHECK_EQ(target.fullName(), std::string("Иванов И.И."));
}

// ---------------------------------------------------------------------------
// Методы изменения
// ---------------------------------------------------------------------------

TF_TEST("Сеттеры изменяют значения полей") {
    Worker worker = makeWorker();
    worker.setFullName("Смирнов Д.А.");
    worker.setPosition("Заведующий кафедрой");
    worker.setSalary(180000.0);
    worker.setHireYear(2010);

    TF_CHECK_EQ(worker.fullName(), std::string("Смирнов Д.А."));
    TF_CHECK_EQ(worker.position(), std::string("Заведующий кафедрой"));
    TF_CHECK_EQ(worker.salary(), 180000.0);
    TF_CHECK_EQ(worker.hireYear(), 2010);
}

TF_TEST("Сеттеры не меняют поле при некорректном значении") {
    Worker worker = makeWorker();
    TF_CHECK_THROW(worker.setSalary(-5.0), std::invalid_argument);
    TF_CHECK_EQ(worker.salary(), 120000.0);
    TF_CHECK_THROW(worker.setHireYear(1800), std::invalid_argument);
    TF_CHECK_EQ(worker.hireYear(), 2015);
}

TF_TEST("Метод edit изменяет все поля одной операцией") {
    Worker worker = makeWorker();
    worker.edit("Кузнецова Е.В.", "Инженер-программист", 150000.0, 2020);

    TF_CHECK_EQ(worker.fullName(), std::string("Кузнецова Е.В."));
    TF_CHECK_EQ(worker.position(), std::string("Инженер-программист"));
    TF_CHECK_EQ(worker.salary(), 150000.0);
    TF_CHECK_EQ(worker.hireYear(), 2020);
}

TF_TEST("Метод edit не меняет объект при некорректных данных") {
    Worker worker = makeWorker();
    TF_CHECK_THROW(worker.edit("Кузнецова Е.В.", "", 150000.0, 2020),
                   std::invalid_argument);
    TF_CHECK(worker == makeWorker());
}

TF_TEST("Метод raiseSalary повышает оклад на заданный процент") {
    Worker worker("Иванов И.И.", "Доцент", 2000.0, 2020);
    worker.raiseSalary(25.0);
    TF_CHECK_EQ(worker.salary(), 2500.0);
}

TF_TEST("Метод raiseSalary отклоняет отрицательный процент") {
    Worker worker = makeWorker();
    TF_CHECK_THROW(worker.raiseSalary(-10.0), std::invalid_argument);
    TF_CHECK_EQ(worker.salary(), 120000.0);
}

// ---------------------------------------------------------------------------
// Стаж работы
// ---------------------------------------------------------------------------

TF_TEST("Стаж считается как разница текущего года и года поступления") {
    Worker worker("Иванов И.И.", "Доцент", 120000.0, 2015);
    TF_CHECK_EQ(worker.experience(2026), 11);
    TF_CHECK_EQ(worker.experience(2020), 5);
    TF_CHECK_EQ(worker.experience(2015), 0);
}

TF_TEST("Стаж не становится отрицательным") {
    Worker worker("Иванов И.И.", "Доцент", 120000.0, 2024);
    TF_CHECK_EQ(worker.experience(2020), 0);
}

TF_TEST("Проверка превышения стажа строго больше заданного значения") {
    Worker worker("Иванов И.И.", "Доцент", 120000.0, 2021);

    TF_CHECK(worker.experienceExceeds(4, 2026));
    TF_CHECK(!worker.experienceExceeds(5, 2026));
    TF_CHECK(!worker.experienceExceeds(6, 2026));
}

// ---------------------------------------------------------------------------
// Отображение
// ---------------------------------------------------------------------------

TF_TEST("Строковое представление содержит все поля и стаж") {
    Worker worker = makeWorker();
    const std::string text = worker.toString(2026);

    TF_CHECK(text.find("Иванов И.И.") != std::string::npos);
    TF_CHECK(text.find("Доцент") != std::string::npos);
    TF_CHECK(text.find("120000.00") != std::string::npos);
    TF_CHECK(text.find("2015") != std::string::npos);
    TF_CHECK(text.find("11 лет") != std::string::npos);
}

TF_TEST("Форма слова «год» зависит от числа лет") {
    Worker first("Иванов И.И.", "Доцент", 1.0, 2025);
    Worker second("Петров П.П.", "Доцент", 1.0, 2024);
    Worker third("Сидоров С.С.", "Доцент", 1.0, 2021);

    TF_CHECK(first.toString(2026).find("1 год") != std::string::npos);
    TF_CHECK(second.toString(2026).find("2 года") != std::string::npos);
    TF_CHECK(third.toString(2026).find("5 лет") != std::string::npos);
}

TF_TEST("Оператор вывода в поток совпадает с print") {
    Worker worker = makeWorker();
    std::ostringstream viaOperator;
    viaOperator << worker;

    std::ostringstream viaPrint;
    worker.print(viaPrint, Worker::currentYear());

    TF_CHECK_EQ(viaOperator.str(), viaPrint.str());
}

// ---------------------------------------------------------------------------
// Отбор по стажу (основная логика задания)
// ---------------------------------------------------------------------------

TF_TEST("selectByMinExperience отбирает только сотрудников с нужным стажем") {
    WorkerList workers;
    workers.push_back(Worker("Иванов И.И.", "Доцент", 120000.0, 2015));
    workers.push_back(Worker("Петрова А.С.", "Инженер-программист", 110000.0, 2023));
    workers.push_back(Worker("Сидоров П.П.", "Специалист поддержки", 70000.0, 2024));

    const auto selected = selectByMinExperience(workers, 3, 2026);

    TF_CHECK_EQ(selected.size(), static_cast<std::size_t>(1));
    TF_CHECK_EQ(selected.front()->fullName(), std::string("Иванов И.И."));
}

TF_TEST("selectByMinExperience сохраняет порядок сотрудников") {
    WorkerList workers;
    workers.push_back(Worker("Первый П.П.", "Доцент", 1.0, 2010));
    workers.push_back(Worker("Второй В.В.", "Доцент", 1.0, 2011));
    workers.push_back(Worker("Третий Т.Т.", "Доцент", 1.0, 2012));

    const auto selected = selectByMinExperience(workers, 10, 2026);

    TF_CHECK_EQ(selected.size(), static_cast<std::size_t>(3));
    TF_CHECK_EQ(selected[0]->fullName(), std::string("Первый П.П."));
    TF_CHECK_EQ(selected[1]->fullName(), std::string("Второй В.В."));
    TF_CHECK_EQ(selected[2]->fullName(), std::string("Третий Т.Т."));
}

TF_TEST("selectByMinExperience возвращает пустой результат, если стаж меньше") {
    WorkerList workers;
    workers.push_back(Worker("Иванов И.И.", "Доцент", 1.0, 2024));

    const auto selected = selectByMinExperience(workers, 10, 2026);
    TF_CHECK(selected.empty());
}

TF_TEST("selectByMinExperience на пустом списке возвращает пустой результат") {
    WorkerList workers;
    const auto selected = selectByMinExperience(workers, 0, 2026);
    TF_CHECK(selected.empty());
}

TF_TEST("Демонстрационный набор содержит пять сотрудников") {
    const auto demo = practice::makeDemoWorkers();
    TF_CHECK_EQ(demo.size(), static_cast<std::size_t>(5));

    const auto selected = selectByMinExperience(demo, 3, 2026);
    TF_CHECK_EQ(selected.size(), static_cast<std::size_t>(3));
}

int main() { return tf::runAll(std::cout); }
