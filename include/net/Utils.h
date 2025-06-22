#pragma once
#include <openssl/ssl.h> // Для работы с SSL
#include <string> // Для работы со строками
#include <arpa/inet.h> // Для сетевых функций (htonl, ntohl)

namespace tls {

// Отправка данных через SSL с префиксом длины
bool sendWithLength(SSL* ssl, const char* data, size_t len) {
    uint32_t lenNet = htonl(len); // Переводит длину в сетевой порядок байтов (big-endian)
    if (SSL_write(ssl, &lenNet, 4) != 4) return false; // Отправляет 4 байта длины
    if (SSL_write(ssl, data, len) != static_cast<int>(len)) return false; // Отправляет данные
    return true; // Успех
}

// Получение данных через SSL с префиксом длины
bool receiveWithLength(SSL* ssl, std::string& data) {
    uint32_t lenNet; // Переменная для длины в сетевом порядке
    if (SSL_read(ssl, &lenNet, 4) != 4) return false; // Читает 4 байта длины
    size_t len = ntohl(lenNet); // Переводит длину в хост-порядок (little-endian)
    data.resize(len); // Устанавливает размер строки под данные
    size_t total = 0; // Счетчик прочитанных байтов
    while (total < len) { // Читает данные порциями
        int n = SSL_read(ssl, &data[total], len - total); // Читает оставшиеся байты
        if (n <= 0) return false; // Ошибка или конец соединения
        total += n; // Увеличивает счетчик
    }
    return true; // Успех
}

// Извлечение заголовка "Host" из HTTP-запроса
std::string getHostFromRequest(const std::string& request) {
    size_t hostPos = request.find("Host:"); // Ищет "Host:"
    if (hostPos == std::string::npos) return ""; // Не найдено
    size_t start = hostPos + 5; // Пропускает "Host:"
    while (start < request.size() && std::isspace(request[start])) start++; // Пропускает пробелы
    size_t end = request.find_first_of("\r\n", start); // Ищет конец строки
    if (end == std::string::npos) return ""; // Не найдено
    return request.substr(start, end - start); // Возвращает имя хоста
}

// Чтение HTTP-ответа из сокета
std::string readHttpResponse(int sock) {
    std::string response; // Ответ
    char buf[1024]; // Буфер для чтения
    while (true) {
        int n = recv(sock, buf, sizeof(buf), 0); // Читает данные из сокета
        if (n <= 0) break; // Ошибка или конец данных
        response.append(buf, n); // Добавляет данные в ответ
        if (response.find("\r\n\r\n") != std::string::npos) { // Найден конец заголовков
            size_t clPos = response.find("Content-Length:"); // Ищет длину тела
            if (clPos != std::string::npos) {
                size_t start = clPos + 15; // Пропускает "Content-Length:"
                while (start < response.size() && std::isspace(response[start])) start++; // Пропускает пробелы
                size_t end = response.find_first_of("\r\n", start); // Ищет конец строки
                if (end != std::string::npos) {
                    int contentLength = std::stoi(response.substr(start, end - start)); // Длина тела
                    size_t headerEnd = response.find("\r\n\r\n") + 4; // Конец заголовков
                    int bodyRead = response.size() - headerEnd; // Прочитано тела
                    while (bodyRead < contentLength) { // Читает остаток тела
                        n = recv(sock, buf, sizeof(buf), 0);
                        if (n <= 0) break;
                        response.append(buf, n);
                        bodyRead += n;
                    }
                }
            }
            break; // Завершает чтение после полного ответа
        }
    }
    return response; // Возвращает ответ
}

} // namespace tls