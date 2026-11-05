#include "worker.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

using practice::Worker;
using practice::WorkerList;

/// Переключение консоли Windows в режим UTF-8, чтобы русский текст
/// корректно и читался, и выводился.
void setupConsole() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

struct Options {
    bool help = false;
    bool demo = false;
    std::string file;
    int minExperience = -1;   ///< -1 означает «спросить у пользователя»
    int currentYear = -1;     ///< -1 означает «взять системный год»
};

void printUsage() {
    std::cout
        << "Программа учёта сотрудников (класс WORKER)\n"
        << "Производственная практика, 02.03.03, профиль «Разработка ПО "
           "(Full-stack разработка)»\n"
        << "Выполнил: Лайшев Марат Хасанович\n\n"
        << "Использование:\n"
        << "  worker_app                     интерактивный ввод с клавиатуры\n"
        << "  worker_app --demo              демонстрационный набор данных\n"
        << "  worker_app --file <путь>       загрузка сотрудников из файла\n\n"
        << "Параметры:\n"
        << "  --min-experience <лет>   порог стажа (по умолчанию 3 в режимах\n"
        << "                           --demo и --file, иначе запрос с клавиатуры)\n"
        << "  --current-year <год>     расчётный год (по умолчанию системный)\n"
        << "  --help                   эта справка\n\n"
        << "Формат файла: Фамилия И.О.;Должность;Оклад;Год поступления\n"
        << "Строки, начинающиеся с #, и пустые строки игнорируются.\n";
}

/// Разбор целого числа из строки целиком (без «хвостов»).
bool parseInt(const std::string& text, int& value) {
    try {
        std::size_t processed = 0;
        const int parsed = std::stoi(text, &processed);
        if (processed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/// Разбор дробного числа из строки целиком.
bool parseDouble(const std::string& text, double& value) {
    try {
        std::size_t processed = 0;
        const double parsed = std::stod(text, &processed);
        if (processed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string trim(const std::string& text) {
    // Убираем BOM (UTF-8 with BOM — частая проблема файлов, сохранённых
    // в Блокноте), иначе первое поле первой строки не разберётся.
    const std::string bom = "\xEF\xBB\xBF";
    std::string view = text;
    if (view.rfind(bom, 0) == 0) {
        view.erase(0, bom.size());
    }
    const auto first = view.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const auto last = view.find_last_not_of(" \t\r\n");
    return view.substr(first, last - first + 1);
}

/// Читает строку и повторяет запрос, пока пользователь не введёт
/// корректное значение (проверку выполняет convert).
template <typename T, typename Converter>
T askValue(const std::string& prompt, Converter convert) {
    for (;;) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            throw std::runtime_error("Ввод прерван пользователем");
        }
        line = trim(line);
        T value{};
        if (convert(line, value)) {
            return value;
        }
        std::cout << "  Некорректное значение, повторите ввод.\n";
    }
}

/// Ввод одного сотрудника с клавиатуры с проверкой каждого поля.
Worker readWorker(std::size_t index) {
    std::cout << "\nСотрудник №" << index << '\n';

    const auto name = askValue<std::string>(
        "  Фамилия и инициалы: ",
        [](const std::string& text, std::string& value) {
            if (text.empty()) {
                return false;
            }
            value = text;
            return true;
        });
    const auto position = askValue<std::string>(
        "  Должность: ",
        [](const std::string& text, std::string& value) {
            if (text.empty()) {
                return false;
            }
            value = text;
            return true;
        });
    const double salary = askValue<double>(
        "  Оклад, руб.: ",
        [](const std::string& text, double& value) {
            return parseDouble(text, value) && value >= 0.0;
        });
    const int hireYear = askValue<int>(
        "  Год поступления на работу: ",
        [](const std::string& text, int& value) {
            return parseInt(text, value) && Worker::isValidYear(value);
        });

    return Worker(name, position, salary, hireYear);
}

WorkerList readWorkersFromKeyboard() {
    const std::size_t count = askValue<std::size_t>(
        "Введите количество сотрудников: ",
        [](const std::string& text, std::size_t& value) {
            int parsed = 0;
            if (!parseInt(text, parsed) || parsed < 0) {
                return false;
            }
            value = static_cast<std::size_t>(parsed);
            return true;
        });

    WorkerList workers;
    for (std::size_t i = 1; i <= count; ++i) {
        workers.push_back(readWorker(i));
    }
    return workers;
}

/// Загрузка сотрудников из файла формата
/// «Фамилия И.О.;Должность;Оклад;Год поступления».
WorkerList loadFromFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Не удалось открыть файл: " + path);
    }

    WorkerList workers;
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        std::vector<std::string> fields;
        std::size_t start = 0;
        for (;;) {
            const std::size_t separator = line.find(';', start);
            if (separator == std::string::npos) {
                fields.push_back(trim(line.substr(start)));
                break;
            }
            fields.push_back(trim(line.substr(start, separator - start)));
            start = separator + 1;
        }

        const std::string where = " (" + path + ", строка " +
                                  std::to_string(lineNumber) + ")";

        if (fields.size() != 4) {
            throw std::runtime_error("Ожидается 4 поля, разделённые ';'" + where);
        }

        double salary = 0.0;
        int hireYear = 0;
        if (!parseDouble(fields[2], salary)) {
            throw std::runtime_error("Некорректный оклад: " + fields[2] + where);
        }
        if (!parseInt(fields[3], hireYear)) {
            throw std::runtime_error("Некорректный год поступления: " +
                                     fields[3] + where);
        }

        try {
            workers.push_back(Worker(fields[0], fields[1], salary, hireYear));
        } catch (const std::exception& error) {
            throw std::runtime_error(std::string(error.what()) + where);
        }
    }

    return workers;
}

/// Демонстрационный набор данных, адаптированный под профильное
/// подразделение организации: должности соответствуют штатному
/// расписанию вуза (преподаватели и ИТ-специалисты).
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

void printResult(const WorkerList& workers, int minYears, int currentYear) {
    std::cout << "Всего сотрудников в списке: " << workers.size() << '\n';

    const auto selected =
        practice::selectByMinExperience(workers, minYears, currentYear);

    if (selected.empty()) {
        std::cout << "Сотрудников со стажем более " << minYears
                  << " лет (на " << currentYear << " год) не найдено.\n";
        return;
    }

    std::cout << "Сотрудники со стажем более " << minYears << " лет (на "
              << currentYear << " год):\n";
    for (const Worker* worker : selected) {
        std::cout << "  " << worker->fullName() << " — " << worker->position()
                  << ", стаж " << worker->experience(currentYear) << " лет\n";
    }
}

Options parseArguments(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--demo") {
            options.demo = true;
        } else if (argument == "--file") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Не указан путь после --file");
            }
            options.file = argv[++i];
        } else if (argument == "--min-experience") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Не указано значение после --min-experience");
            }
            if (!parseInt(argv[++i], options.minExperience) ||
                options.minExperience < 0) {
                throw std::runtime_error("Некорректный порог стажа");
            }
        } else if (argument == "--current-year") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Не указано значение после --current-year");
            }
            if (!parseInt(argv[++i], options.currentYear) ||
                !Worker::isValidYear(options.currentYear)) {
                throw std::runtime_error("Некорректный расчётный год");
            }
        } else {
            throw std::runtime_error("Неизвестный параметр: " + argument);
        }
    }
    return options;
}

}  // namespace

int main(int argc, char* argv[]) {
    setupConsole();

    Options options;
    try {
        options = parseArguments(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << "\n\n";
        printUsage();
        return 1;
    }

    if (options.help) {
        printUsage();
        return 0;
    }

    const int currentYear =
        options.currentYear > 0 ? options.currentYear : Worker::currentYear();

    try {
        WorkerList workers;
        const bool batchMode = options.demo || !options.file.empty();

        if (options.demo) {
            workers = makeDemoWorkers();
        } else if (!options.file.empty()) {
            workers = loadFromFile(options.file);
        } else {
            std::cout << "Программа учёта сотрудников (класс WORKER)\n"
                      << "Введите данные сотрудников.\n";
            workers = readWorkersFromKeyboard();
        }

        int minYears = options.minExperience;
        if (minYears < 0) {
            if (batchMode) {
                minYears = 3;
            } else {
                minYears = askValue<int>(
                    "\nВведите минимальный стаж (полных лет): ",
                    [](const std::string& text, int& value) {
                        return parseInt(text, value) && value >= 0;
                    });
            }
        }

        printResult(workers, minYears, currentYear);
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
