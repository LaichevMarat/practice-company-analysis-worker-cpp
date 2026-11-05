#include "worker.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using practice::Worker;
using practice::WorkerList;

// ---------------------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------------------

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

/// Декодирование %XX и '+' в строке запроса.
std::string urlDecode(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '+') {
            result.push_back(' ');
        } else if (c == '%' && i + 2 < text.size()) {
            const int value = std::stoi(text.substr(i + 1, 2), nullptr, 16);
            result.push_back(static_cast<char>(value));
            i += 2;
        } else {
            result.push_back(c);
        }
    }
    return result;
}

std::string jsonEscape(const std::string& text) {
    std::string result;
    result.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream code;
                    code << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                         << static_cast<int>(static_cast<unsigned char>(c));
                    result += code.str();
                } else {
                    result.push_back(c);
                }
        }
    }
    return result;
}

std::string formatMoney(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value;
    return text.str();
}

std::map<std::string, std::string> parsePairs(const std::string& source,
                                              char separator,
                                              bool decode) {
    std::map<std::string, std::string> pairs;
    std::size_t start = 0;
    while (start <= source.size()) {
        const std::size_t next = source.find(separator, start);
        const std::string item =
            source.substr(start, next == std::string::npos ? std::string::npos
                                                           : next - start);
        if (!item.empty()) {
            const std::size_t equal = item.find('=');
            const std::string key = decode ? urlDecode(item.substr(0, equal))
                                           : item.substr(0, equal);
            const std::string value =
                equal == std::string::npos
                    ? std::string()
                    : (decode ? urlDecode(item.substr(equal + 1))
                              : item.substr(equal + 1));
            pairs[toLower(key)] = value;
        }
        if (next == std::string::npos) {
            break;
        }
        start = next + 1;
    }
    return pairs;
}

std::string readWholeFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// ---------------------------------------------------------------------------
// HTTP-сервер
// ---------------------------------------------------------------------------

struct Request {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> query;
};

struct ServerOptions {
    int port = 8080;
    bool openBrowser = true;
};

/// Минимальный HTTP-сервер: статика + JSON API поверх класса WORKER.
class HttpServer {
public:
    HttpServer(int port, std::string page) : port_(port), page_(std::move(page)) {}

    bool start() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            std::cerr << "Ошибка инициализации Winsock\n";
            return false;
        }

        listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket_ == INVALID_SOCKET) {
            std::cerr << "Не удалось создать сокет\n";
            return false;
        }

        const BOOL reuse = TRUE;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<u_short>(port_));
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

        if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&address),
                 sizeof(address)) == SOCKET_ERROR) {
            std::cerr << "Порт " << port_ << " занят, попробуйте другой (--port)\n";
            return false;
        }
        if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Не удалось начать прослушивание порта\n";
            return false;
        }
        return true;
    }

    void run() {
        for (;;) {
            SOCKET client = accept(listen_socket_, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                continue;
            }
            std::thread([this, client]() { handleClient(client); }).detach();
        }
    }

private:
    void handleClient(SOCKET client) {
        try {
            Request request;
            if (!readRequest(client, request)) {
                closesocket(client);
                return;
            }
            route(client, request);
        } catch (const std::exception& error) {
            std::cerr << "Ошибка запроса: " << error.what() << '\n';
        }
        closesocket(client);
    }

    // --- чтение запроса -----------------------------------------------------

    static bool receiveMore(SOCKET client, std::string& buffer) {
        char chunk[4096];
        const int received = recv(client, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            return false;
        }
        buffer.append(chunk, static_cast<std::size_t>(received));
        return true;
    }

    static bool readRequest(SOCKET client, std::string& header,
                            std::string& body) {
        std::size_t headerEnd = std::string::npos;
        while ((headerEnd = header.find("\r\n\r\n")) == std::string::npos) {
            if (!receiveMore(client, header) || header.size() > 65536) {
                return false;
            }
        }

        const std::string headerBlock = header.substr(0, headerEnd);
        std::istringstream lines(headerBlock);
        std::string line;
        std::size_t contentLength = 0;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::string prefix = "content-length:";
            if (toLower(line).rfind(prefix, 0) == 0) {
                try {
                    contentLength = static_cast<std::size_t>(
                        std::stoul(trim(line.substr(prefix.size()))));
                } catch (const std::exception&) {
                    contentLength = 0;
                }
            }
        }

        body = header.substr(headerEnd + 4);
        while (body.size() < contentLength) {
            if (!receiveMore(client, body)) {
                return false;
            }
        }

        if (headerEnd == std::string::npos) {
            return false;
        }
        header = headerBlock;
        return true;
    }

    static bool readRequest(SOCKET client, Request& request) {
        std::string header;
        std::string body;
        if (!readRequest(client, header, body)) {
            return false;
        }

        std::istringstream first(header);
        std::string target;
        std::string version;
        first >> request.method >> target >> version;
        if (request.method.empty()) {
            return false;
        }

        const std::size_t question = target.find('?');
        if (question == std::string::npos) {
            request.path = target;
        } else {
            request.path = target.substr(0, question);
            request.query = parsePairs(target.substr(question + 1), '&', true);
        }
        request.body = std::move(body);
        return true;
    }

    // --- ответ --------------------------------------------------------------

    static void sendAll(SOCKET client, const std::string& data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
            const int portion =
                send(client, data.data() + sent,
                     static_cast<int>(data.size() - sent), 0);
            if (portion <= 0) {
                return;
            }
            sent += static_cast<std::size_t>(portion);
        }
    }

    static void respond(SOCKET client, int code, const std::string& contentType,
                        const std::string& body) {
        const char* reason = "OK";
        if (code == 400) {
            reason = "Bad Request";
        } else if (code == 404) {
            reason = "Not Found";
        } else if (code == 405) {
            reason = "Method Not Allowed";
        }

        std::ostringstream response;
        response << "HTTP/1.1 " << code << ' ' << reason << "\r\n"
                 << "Content-Type: " << contentType << "; charset=utf-8\r\n"
                 << "Content-Length: " << body.size() << "\r\n"
                 << "Cache-Control: no-store\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;
        sendAll(client, response.str());
    }

    static void respondJson(SOCKET client, int code, const std::string& body) {
        respond(client, code, "application/json", body);
    }

    static void respondError(SOCKET client, int code, const std::string& message) {
        respondJson(client, code,
                    "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}");
    }

    // --- маршруты -----------------------------------------------------------

    void route(SOCKET client, const Request& request) {
        if (request.method == "GET" &&
            (request.path == "/" || request.path == "/index.html")) {
            if (page_.empty()) {
                respond(client, 404, "text/plain",
                        "Не найден файл web/index.html");
            } else {
                respond(client, 200, "text/html", page_);
            }
            return;
        }

        if (request.method == "GET" && request.path == "/api/state") {
            respondJson(client, 200, stateJson(request.query));
            return;
        }

        if (request.method == "POST" && request.path == "/api/workers") {
            addWorker(client, request.body);
            return;
        }

        if (request.method == "POST" && request.path == "/api/workers/delete") {
            deleteWorker(client, request.body);
            return;
        }

        if (request.method == "POST" && request.path == "/api/demo") {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                workers_ = practice::makeDemoWorkers();
            }
            respondJson(client, 200, "{\"ok\":true}");
            return;
        }

        if (request.method == "POST" && request.path == "/api/clear") {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                workers_.clear();
            }
            respondJson(client, 200, "{\"ok\":true}");
            return;
        }

        if (request.path.rfind("/api/", 0) == 0) {
            respondError(client, 404, "Неизвестный метод API");
        } else {
            respond(client, 404, "text/plain", "Страница не найдена");
        }
    }

    std::string stateJson(const std::map<std::string, std::string>& query) {
        int minYears = 3;
        int year = Worker::currentYear();

        const auto minIt = query.find("min");
        if (minIt != query.end()) {
            try {
                minYears = std::stoi(minIt->second);
            } catch (const std::exception&) {
                minYears = 0;
            }
        }
        if (minYears < 0) {
            minYears = 0;
        }

        const auto yearIt = query.find("year");
        if (yearIt != query.end()) {
            try {
                const int requested = std::stoi(yearIt->second);
                if (requested >= 1900 && requested <= 2100) {
                    year = requested;
                }
            } catch (const std::exception&) {
                // оставляем системный год
            }
        }

        std::lock_guard<std::mutex> guard(mutex_);

        std::ostringstream json;
        json << "{\"currentYear\":" << year << ",\"minExperience\":" << minYears
             << ",\"workers\":[";

        int selectedCount = 0;
        bool first = true;
        for (const Worker& worker : workers_) {
            const bool selected = worker.experienceExceeds(minYears, year);
            if (selected) {
                ++selectedCount;
            }
            if (!first) {
                json << ',';
            }
            first = false;
            json << "{\"name\":\"" << jsonEscape(worker.fullName())
                 << "\",\"position\":\"" << jsonEscape(worker.position())
                 << "\",\"salary\":\"" << formatMoney(worker.salary())
                 << "\",\"hireYear\":" << worker.hireYear()
                 << ",\"experience\":" << worker.experience(year)
                 << ",\"selected\":" << (selected ? "true" : "false") << '}';
        }

        json << "],\"selectedCount\":" << selectedCount << '}';
        return json.str();
    }

    void addWorker(SOCKET client, const std::string& body) {
        const auto fields = parsePairs(body, '&', true);

        const auto name = fields.find("name");
        const auto position = fields.find("position");
        const auto salary = fields.find("salary");
        const auto hireYear = fields.find("hireyear");

        if (name == fields.end() || position == fields.end() ||
            salary == fields.end() || hireYear == fields.end()) {
            respondError(client, 400, "Заполните все поля");
            return;
        }

        double salaryValue = 0.0;
        int yearValue = 0;
        try {
            salaryValue = std::stod(salary->second);
        } catch (const std::exception&) {
            respondError(client, 400, "Оклад должен быть числом");
            return;
        }
        try {
            yearValue = std::stoi(hireYear->second);
        } catch (const std::exception&) {
            respondError(client, 400, "Год поступления должен быть числом");
            return;
        }

        try {
            Worker worker(name->second, position->second, salaryValue, yearValue);
            std::lock_guard<std::mutex> guard(mutex_);
            workers_.push_back(worker);
        } catch (const std::exception& error) {
            respondError(client, 400, error.what());
            return;
        }

        respondJson(client, 200, "{\"ok\":true}");
    }

    void deleteWorker(SOCKET client, const std::string& body) {
        const auto fields = parsePairs(body, '&', true);
        const auto indexIt = fields.find("index");
        if (indexIt == fields.end()) {
            respondError(client, 400, "Не указан индекс сотрудника");
            return;
        }

        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(indexIt->second));
        } catch (const std::exception&) {
            respondError(client, 400, "Некорректный индекс");
            return;
        }

        std::lock_guard<std::mutex> guard(mutex_);
        if (index >= workers_.size()) {
            respondError(client, 400, "Сотрудник не найден");
            return;
        }
        auto it = workers_.begin();
        std::advance(it, static_cast<long>(index));
        workers_.erase(it);
        respondJson(client, 200, "{\"ok\":true}");
    }

    int port_;
    std::string page_;
    SOCKET listen_socket_ = INVALID_SOCKET;
    std::mutex mutex_;
    WorkerList workers_;
};

// ---------------------------------------------------------------------------
// Точка входа
// ---------------------------------------------------------------------------

void setupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

std::string loadPage() {
    const std::vector<std::string> candidates = {"web/index.html",
                                                 "../web/index.html"};
    for (const std::string& path : candidates) {
        const std::string page = readWholeFile(path);
        if (!page.empty()) {
            std::cout << "Страница: " << path << '\n';
            return page;
        }
    }
    return {};
}

void openBrowser(const std::string& url) {
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace

int main(int argc, char* argv[]) {
    setupConsole();

    ServerOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--port" && i + 1 < argc) {
            try {
                options.port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Некорректный номер порта\n";
                return 1;
            }
        } else if (argument == "--no-browser") {
            options.openBrowser = false;
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "Веб-версия учёта сотрудников (класс WORKER)\n"
                      << "Выполнил: Лайшев Марат Хасанович\n\n"
                      << "Использование: worker_server [--port 8080] [--no-browser]\n";
            return 0;
        }
    }

    const std::string page = loadPage();
    if (page.empty()) {
        std::cerr << "Не найден файл web/index.html.\n"
                  << "Запустите программу из корня проекта.\n";
        return 1;
    }

    HttpServer server(options.port, page);
    if (!server.start()) {
        return 1;
    }

    std::ostringstream url;
    url << "http://localhost:" << options.port << '/';
    std::cout << "Сервер запущен: " << url.str() << '\n'
              << "Остановка: Ctrl+C\n";

    if (options.openBrowser) {
        std::thread([address = url.str()]() {
            Sleep(300);
            openBrowser(address);
        }).detach();
    }

    server.run();
    return 0;
}
