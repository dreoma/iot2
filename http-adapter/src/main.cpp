#include <curl/curl.h>
#include <drogon/drogon.h>

#include "crypto/encoder.h"

#include <cstdlib>
#include <string>

using namespace drogon;

namespace
{
std::string coreUrl()
{
    const char *value = std::getenv("CORE_URL");
    return value ? value : "https://core-tls:8443";
}

std::string caFile()
{
    const char *value = std::getenv("CORE_CA_FILE");
    return value ? value : "/certs/ca.crt";
}

std::string trafficKey()
{
    const char *value = std::getenv("TRAFFIC_KEY_HEX");
    if (!value)
        throw std::runtime_error("TRAFFIC_KEY_HEX is not set");
    return value;
}

size_t appendResponse(char *data, size_t size, size_t count, void *userData)
{
    static_cast<std::string *>(userData)->append(data, size * count);
    return size * count;
}

HttpResponsePtr forwardTelemetry(long long deviceId, const Json::Value &data)
{
    Json::Value body;
    body["device_id"] = Json::Int64(deviceId);
    body["data"] = data;
    Json::StreamWriterBuilder writer;
    const Encoder encoder(trafficKey());
    Json::Value encrypted;
    encrypted["payload"] = encoder.encrypt(Json::writeString(writer, body));
    const auto json = Json::writeString(writer, encrypted);
    std::string responseBody;

    CURL *curl = curl_easy_init();
    if (!curl)
        return HttpResponse::newHttpResponse(k500InternalServerError, CT_APPLICATION_JSON);

    const auto core = coreUrl();
    const auto ca = caFile();
    const auto url = core + "/telemetry/encrypted";
    struct curl_slist *headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);

    // TLS: адаптер доверяет только нашему CA и проверяет имя сервера.
    curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    const auto transfer = curl_easy_perform(curl);
    long status = 502;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (transfer != CURLE_OK)
        return HttpResponse::newHttpResponse(k502BadGateway, CT_APPLICATION_JSON);

    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(static_cast<HttpStatusCode>(status));
    response->setContentTypeCode(CT_APPLICATION_JSON);
    response->setBody(responseBody);
    return response;
}
}  // namespace

int main()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    Encoder(trafficKey());  // завершить запуск сразу, если ключ отсутствует или неправильный

    // Устройства отправляют: {"device_id": 1, "data": {"temperature": 22.5}}.
    app().registerHandler(
        "/telemetry",
        [](const HttpRequestPtr &request,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (!body || !body->isMember("device_id") || !body->isMember("data") ||
                !(*body)["device_id"].isInt64() || !(*body)["data"].isObject())
            {
                Json::Value error;
                error["error"] = "device_id and data object are required";
                auto response = HttpResponse::newHttpJsonResponse(error);
                response->setStatusCode(k400BadRequest);
                callback(response);
                return;
            }
            callback(forwardTelemetry((*body)["device_id"].asInt64(), (*body)["data"]));
        },
        {Post});

    app().loadConfigFile("config.json");
    app().run();
    curl_global_cleanup();
}
