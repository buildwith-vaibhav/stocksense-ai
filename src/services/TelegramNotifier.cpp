#include "stocksense/services/TelegramNotifier.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstdio>
#include <array>
#include <sstream>

namespace stocksense::services {

namespace {

std::string shellEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '`' || c == '$') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

}  // namespace

TelegramNotifier::TelegramNotifier() {
  if (const char* t = std::getenv("STOCKSENSE_TELEGRAM_BOT_TOKEN")) {
    bot_token_ = t;
  }
  if (const char* c = std::getenv("STOCKSENSE_TELEGRAM_CHAT_ID")) {
    chat_id_ = c;
  }
  configured_ = !bot_token_.empty() && !chat_id_.empty();
  if (configured_) {
    spdlog::info("Telegram notifier configured (chat_id set)");
  } else {
    spdlog::warn(
        "Telegram not configured. Set STOCKSENSE_TELEGRAM_BOT_TOKEN and "
        "STOCKSENSE_TELEGRAM_CHAT_ID to enable notifications.");
  }
}

TelegramResult TelegramNotifier::sendMessage(const std::string& text) const {
  TelegramResult result;
  if (!configured_) {
    result.message = "telegram_not_configured";
    spdlog::info("[Telegram dry-run] {}", text);
    return result;
  }

  // Use curl (available on modern Windows and Linux)
  std::ostringstream cmd;
  cmd << "curl -s -o NUL -w \"%{http_code}\" -X POST "
      << "\"https://api.telegram.org/bot" << shellEscape(bot_token_)
      << "/sendMessage\" "
      << "-d \"chat_id=" << shellEscape(chat_id_) << "\" "
      << "-d \"disable_web_page_preview=true\" "
      << "--data-urlencode \"text=" << text << "\"";

#ifdef _WIN32
  // Windows curl writes to NUL; capture status via temp is awkward — run and assume
  // Use _popen to capture body (http code with -w)
  FILE* pipe = _popen(cmd.str().c_str(), "r");
#else
  // Redirect http code only: adjust for unix
  std::string unixCmd =
      "curl -s -o /dev/null -w \"%{http_code}\" -X POST "
      "\"https://api.telegram.org/bot" +
      bot_token_ + "/sendMessage\" -d \"chat_id=" + chat_id_ +
      "\" -d \"disable_web_page_preview=true\" --data-urlencode \"text=" +
      text + "\"";
  FILE* pipe = popen(unixCmd.c_str(), "r");
#endif

  if (!pipe) {
    result.message = "curl_failed_to_start";
    return result;
  }

  std::array<char, 16> buf{};
  std::string status;
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
    status += buf.data();
  }
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif

  try {
    result.http_status = std::stoi(status);
  } catch (...) {
    result.http_status = 0;
  }
  result.ok = (result.http_status >= 200 && result.http_status < 300);
  result.message = result.ok ? "sent" : ("http_" + status);
  if (result.ok) {
    spdlog::info("Telegram message sent");
  } else {
    spdlog::error("Telegram send failed: {}", result.message);
  }
  return result;
}

}  // namespace stocksense::services
