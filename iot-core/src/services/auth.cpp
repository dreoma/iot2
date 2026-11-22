#include "auth.h"

#include <sodium.h>
#include <trantor/utils/ConcurrentTaskQueue.h>

#include <algorithm>
#include <cstdlib>
#include <semaphore>
#include <string_view>

using namespace drogon;
using namespace drogon::orm;

namespace
{
constexpr auto kUserId = "authenticated_user_id";
constexpr std::string_view kPepperedHash = "pepper-v1:";
unsigned char passwordPepper[crypto_auth_hmacsha256_KEYBYTES];
std::string dummyPasswordHash;

void pepperPassword(const std::string &password, unsigned char *mac)
{
    crypto_auth_hmacsha256(mac, reinterpret_cast<const unsigned char *>(password.data()),
                           password.size(), passwordPepper);
}
}

namespace auth
{
bool init()
{
    if (sodium_init() < 0)
        return false;
    const auto *key = std::getenv("IOT_PASSWORD_PEPPER_HEX");
    if (!key || std::string_view(key).size() != 2 * sizeof(passwordPepper) ||
        sodium_hex2bin(passwordPepper, sizeof(passwordPepper), key, 2 * sizeof(passwordPepper),
                       nullptr, nullptr, nullptr) != 0)
        return false;
    dummyPasswordHash = hashPassword(newToken());
    return !dummyPasswordHash.empty();
}

bool validPasswordInput(const Json::Value &value)
{
    if (!value.isString())
        return false;
    const auto password = value.asString();
    return !password.empty() && password.size() <= 256;
}

bool validNewPassword(const Json::Value &value)
{
    return validPasswordInput(value) && value.asString().size() >= 15;
}

std::string hashPassword(const std::string &password)
{
    unsigned char mac[crypto_auth_hmacsha256_BYTES];
    pepperPassword(password, mac);
    char hash[crypto_pwhash_STRBYTES];
    const auto status = crypto_pwhash_str_alg(hash, reinterpret_cast<const char *>(mac), sizeof(mac),
                                              crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                                              crypto_pwhash_ALG_ARGON2ID13);
    sodium_memzero(mac, sizeof(mac));
    return status == 0 ? std::string(kPepperedHash) + hash : std::string{};
}

bool needsPasswordUpgrade(const std::string &hash)
{
    return hash.starts_with("$argon2id$");
}

bool verifyPassword(const std::string &password, const std::string &hash)
{
    const auto &stored = hash.empty() ? dummyPasswordHash : hash;
    if (needsPasswordUpgrade(stored))
        return crypto_pwhash_str_verify(stored.c_str(), password.data(), password.size()) == 0;
    if (!stored.starts_with(kPepperedHash))
        return false;
    unsigned char mac[crypto_auth_hmacsha256_BYTES];
    pepperPassword(password, mac);
    const bool verified = crypto_pwhash_str_verify(stored.c_str() + kPepperedHash.size(),
                                                    reinterpret_cast<const char *>(mac), sizeof(mac)) == 0;
    sodium_memzero(mac, sizeof(mac));
    return verified && !hash.empty();
}

std::string newToken()
{
    unsigned char random[32];
    randombytes_buf(random, sizeof(random));
    char hex[2 * sizeof(random) + 1];
    return sodium_bin2hex(hex, sizeof(hex), random, sizeof(random));
}

std::string digest(const std::string &token)
{
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, reinterpret_cast<const unsigned char *>(token.data()), token.size());
    char hex[2 * sizeof(hash) + 1];
    return sodium_bin2hex(hex, sizeof(hex), hash, sizeof(hash));
}

std::string tokenDigest(const HttpRequestPtr &request)
{
    const auto &header = request->getHeader("Authorization");
    constexpr std::string_view scheme = "bearer ";
    if (header.size() != scheme.size() + 64 ||
        !std::equal(scheme.begin(), scheme.end(), header.begin(), [](char expected, char actual) {
            if (actual >= 'A' && actual <= 'Z')
                actual += 'a' - 'A';
            return actual == expected;
        }))
        return {};
    const auto token = header.substr(scheme.size());
    if (token.find_first_not_of("0123456789abcdef") != std::string::npos)
        return {};
    return digest(token);
}

long long userId(const HttpRequestPtr &request)
{
    return request->attributes()->get<long long>(kUserId);
}

void passwordTask(const Callback &done, std::function<void()> task)
{
    // Argon2 выделяет память и выполняется вне потоков HTTP и PostgreSQL.
    static std::counting_semaphore<8> slots(8);
    static trantor::ConcurrentTaskQueue workers(2, "passwords");
    if (!slots.try_acquire())
    {
        error(done, k429TooManyRequests, "too many password requests");
        return;
    }
    workers.runTaskInQueue([task = std::move(task)] {
        task();
        slots.release();
    });
}

void error(const Callback &done, HttpStatusCode status, const std::string &message)
{
    Json::Value body;
    body["error"] = message;
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    if (status == k401Unauthorized)
        response->addHeader("WWW-Authenticate", "Bearer");
    done(response);
}
}

void UserAuthFilter::doFilter(const HttpRequestPtr &request, FilterCallback &&reject,
                              FilterChainCallback &&next)
{
    const auto digest = auth::tokenDigest(request);
    if (digest.empty())
    {
        auth::error(reject, k401Unauthorized, "session required");
        return;
    }
    app().getDbClient("postgres")->execSqlAsync(
        "SELECT id FROM users WHERE session_digest=$1 AND session_expires_at > now()",
        [request, reject, next = std::move(next)](const Result &rows) {
            if (rows.empty())
            {
                auth::error(reject, k401Unauthorized, "invalid or expired session");
                return;
            }
            request->attributes()->insert(kUserId, rows[0]["id"].as<long long>());
            next();
        },
        [reject](const DrogonDbException &) { auth::error(reject, k503ServiceUnavailable, "database unavailable"); },
        digest);
}
