#ifndef PRACTICE_TEST_FRAMEWORK_H
#define PRACTICE_TEST_FRAMEWORK_H

/// Минимальный самодостаточный каркас модульных тестов.
/// Не требует внешних библиотек (GoogleTest/Catch2), поэтому тесты
/// собираются одной командой g++ без сети и пакетного менеджера.

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tf {

struct Failure {
    std::string message;
};

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        registry().push_back({std::move(name), std::move(body)});
    }
};

inline int runAll(std::ostream& os) {
    int passed = 0;
    int failed = 0;

    for (const TestCase& test : registry()) {
        try {
            test.body();
            ++passed;
            os << "[  OK  ] " << test.name << '\n';
        } catch (const Failure& failure) {
            ++failed;
            os << "[ FAIL ] " << test.name << ": " << failure.message << '\n';
        } catch (const std::exception& error) {
            ++failed;
            os << "[ FAIL ] " << test.name
               << ": необработанное исключение: " << error.what() << '\n';
        } catch (...) {
            ++failed;
            os << "[ FAIL ] " << test.name << ": неизвестное исключение\n";
        }
    }

    os << "\nВсего тестов: " << registry().size() << ", успешно: " << passed
       << ", провалено: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}

template <typename T>
std::string describe(const T& value) {
    std::ostringstream text;
    text << value;
    return text.str();
}

inline std::string describe(bool value) { return value ? "true" : "false"; }

}  // namespace tf

#define TF_CONCAT_INNER(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT_INNER(a, b)

#define TF_TEST(name)                                                        \
    static void TF_CONCAT(testBody_, __LINE__)();                            \
    static ::tf::Registrar TF_CONCAT(testRegistrar_, __LINE__)(              \
        name, &TF_CONCAT(testBody_, __LINE__));                              \
    static void TF_CONCAT(testBody_, __LINE__)()

#define TF_CHECK(condition)                                                  \
    do {                                                                     \
        if (!(condition)) {                                                  \
            throw ::tf::Failure{std::string("условие не выполнено: ") +      \
                                #condition};                                 \
        }                                                                    \
    } while (false)

#define TF_CHECK_EQ(actual, expected)                                        \
    do {                                                                     \
        const auto& tfActual = (actual);                                     \
        const auto& tfExpected = (expected);                                 \
        if (!(tfActual == tfExpected)) {                                     \
            throw ::tf::Failure{std::string(#actual) + " != " + #expected +  \
                                ": получено " + ::tf::describe(tfActual) +   \
                                ", ожидалось " + ::tf::describe(tfExpected)};\
        }                                                                    \
    } while (false)

#define TF_CHECK_THROW(expression, exceptionType)                            \
    do {                                                                     \
        bool tfThrown = false;                                               \
        try {                                                                \
            (void)(expression);                                              \
        } catch (const exceptionType&) {                                     \
            tfThrown = true;                                                 \
        }                                                                    \
        if (!tfThrown) {                                                     \
            throw ::tf::Failure{std::string("не выброшено исключение ") +    \
                                #exceptionType + " в " + #expression};       \
        }                                                                    \
    } while (false)

#endif  // PRACTICE_TEST_FRAMEWORK_H
