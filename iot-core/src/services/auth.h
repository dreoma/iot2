#pragma once

#include <drogon/drogon.h>

namespace auth
{
using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

bool init();
bool validNewPassword(const Json::Value &value);
bool validPasswordInput(const Json::Value &value);
std::string hashPassword(const std::string &password);
bool verifyPassword(const std::string &password, const std::string &hash);
bool needsPasswordUpgrade(const std::string &hash);
std::string newToken();
std::string tokenDigest(const drogon::HttpRequestPtr &request);
std::string digest(const std::string &token);
long long userId(const drogon::HttpRequestPtr &request);
void passwordTask(const Callback &done, std::function<void()> task);
void error(const Callback &done, drogon::HttpStatusCode status, const std::string &message);
}

class UserAuthFilter : public drogon::HttpFilter<UserAuthFilter>
{
  public:
    void doFilter(const drogon::HttpRequestPtr &request, drogon::FilterCallback &&reject,
                  drogon::FilterChainCallback &&next) override;
};
