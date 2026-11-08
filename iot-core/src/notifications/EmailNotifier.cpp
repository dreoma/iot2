
#include "EmailNotifier.h"
#include <curl/curl.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "EmailTemplate.h"

namespace {

std::string base64(const std::string& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) { out.push_back(t[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) out.push_back(t[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string wrap76(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i += 76) o += s.substr(i, 76) + "\r\n";
    return o;
}

std::string rfcDate() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream o;
    o << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S +0000");
    return o.str();
}

bool badHeader(const std::string& s) {      // защита от инъекции заголовков
    return s.find('\r') != std::string::npos || s.find('\n') != std::string::npos;
}

struct Payload { std::string data; size_t pos = 0; };

size_t readCb(char* ptr, size_t size, size_t nmemb, void* userp) {
    auto* p = static_cast<Payload*>(userp);
    size_t n = std::min(size * nmemb, p->data.size() - p->pos);
    std::memcpy(ptr, p->data.data() + p->pos, n);
    p->pos += n;
    return n;
}

}  // namespace

bool EmailNotifier::send(const Notification& n) {
    if (n.email.empty() || badHeader(n.email) || badHeader(cfg_.from)) return false;

    const std::string b = "=_iot_boundary_7f3a91";
    std::ostringstream m;
    m << "Date: " << rfcDate() << "\r\n"
      << "From: <" << cfg_.from << ">\r\n"
      << "To: <" << n.email << ">\r\n"
      << "Subject: =?UTF-8?B?" << base64(email_tpl::subject(n)) << "?=\r\n"
      << "MIME-Version: 1.0\r\n"
      << "Content-Type: multipart/alternative; boundary=\"" << b << "\"\r\n\r\n"
      << "--" << b << "\r\n"
      << "Content-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: base64\r\n\r\n"
      << wrap76(base64(email_tpl::text(n))) << "\r\n"
      << "--" << b << "\r\n"
      << "Content-Type: text/html; charset=UTF-8\r\nContent-Transfer-Encoding: base64\r\n\r\n"
      << wrap76(base64(email_tpl::html(n))) << "\r\n"
      << "--" << b << "--\r\n";

    Payload payload{m.str(), 0};
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string from = "<" + cfg_.from + ">";
    std::string to = "<" + n.email + ">";
    curl_slist* rcpt = curl_slist_append(nullptr, to.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, cfg_.url.c_str());
    if (!cfg_.user.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, cfg_.user.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg_.password.c_str());
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);   // STARTTLS/TLS обязателен
    }
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCb);
    curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK)
        std::cerr << "[email] send failed: " << curl_easy_strerror(rc) << "\n";   // пароль не логируем

    curl_slist_free_all(rcpt);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK;
}
