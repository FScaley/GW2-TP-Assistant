#pragma once
#include <string>
#include <optional>

struct HttpResponse {
    int statusCode = 0;
    std::string body;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    std::optional<HttpResponse> Get(const std::string& host, const std::string& path,
                                     int timeoutMs = 10000);

private:
    void* m_hSession = nullptr; // HINTERNET
};
