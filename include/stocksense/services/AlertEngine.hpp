#pragma once

#include "stocksense/repositories/StockRepository.hpp"
#include "stocksense/services/TelegramNotifier.hpp"

#include <memory>
#include <string>
#include <vector>

namespace stocksense::services {

struct AlertEvalResult {
  bool ok = false;
  int checked = 0;
  int triggered = 0;
  int notified = 0;
  int suppressed_cooldown = 0;
  std::string message;
  std::vector<std::string> details;
};

class AlertEngine {
public:
  AlertEngine(std::shared_ptr<repositories::StockRepository> repo,
              TelegramNotifier notifier);

  AlertEvalResult evaluateAll();
  int ensureDemoAlerts();

private:
  std::shared_ptr<repositories::StockRepository> repo_;
  TelegramNotifier telegram_;

  bool evaluateOne(const repositories::AlertRecord& a,
                   const repositories::AnalysisSnapshot& snap,
                   std::string& reason) const;
};

}  // namespace stocksense::services
