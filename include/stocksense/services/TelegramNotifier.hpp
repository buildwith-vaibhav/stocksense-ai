#pragma once

#include <string>

namespace stocksense::services {

struct TelegramResult {
  bool ok = false;
  std::string message;
  int http_status = 0;
};

class TelegramNotifier {
public:
  // Reads STOCKSENSE_TELEGRAM_BOT_TOKEN and STOCKSENSE_TELEGRAM_CHAT_ID from env
  TelegramNotifier();

  bool isConfigured() const { return configured_; }
  TelegramResult sendMessage(const std::string& text) const;

private:
  bool configured_ = false;
  std::string bot_token_;
  std::string chat_id_;
};

}  // namespace stocksense::services
