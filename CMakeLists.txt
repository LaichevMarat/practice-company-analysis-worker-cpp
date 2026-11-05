cmake_minimum_required(VERSION 3.16)
project(practice_worker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Библиотека с классом WORKER и его методами (отдельный модуль).
add_library(practice_worker_lib STATIC src/worker.cpp)
target_include_directories(practice_worker_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(practice_worker_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

# Консольное приложение.
add_executable(worker_app src/main.cpp)
target_link_libraries(worker_app PRIVATE practice_worker_lib)

# Модульные тесты.
enable_testing()
add_executable(worker_tests tests/test_worker.cpp)
target_include_directories(worker_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(worker_tests PRIVATE practice_worker_lib)
add_test(NAME worker_tests COMMAND worker_tests)

# Веб-версия: локальный HTTP-сервер и страница в браузере (только Windows).
if(WIN32)
    add_executable(worker_server src/server.cpp)
    target_link_libraries(worker_server PRIVATE practice_worker_lib ws2_32 shell32)
    target_compile_options(worker_server PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-pthread>)
    target_link_options(worker_server PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-pthread>)
endif()
