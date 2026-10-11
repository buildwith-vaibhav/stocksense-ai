#include "stocksense/api/DashboardController.hpp"

#include <string>

namespace stocksense::api {

DashboardController::DashboardController(
    std::shared_ptr<repositories::StockRepository> repo)
    : repo_(std::move(repo)) {}

crow::response DashboardController::htmlPage() {
  static const char* html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>StockSense AI</title>
<style>
  :root {
    --bg: #0f1419; --card: #1a2332; --border: #2d3a4f;
    --text: #e7ecf3; --muted: #8b9bb4; --accent: #3b82f6;
    --pos: #22c55e; --neg: #ef4444; --neu: #eab308;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; font-family: ui-sans-serif, system-ui, sans-serif;
    background: var(--bg); color: var(--text); line-height: 1.45;
  }
  header {
    padding: 1rem 1.5rem; border-bottom: 1px solid var(--border);
    display: flex; flex-wrap: wrap; align-items: center; gap: 1rem;
    justify-content: space-between;
  }
  header h1 { margin: 0; font-size: 1.25rem; font-weight: 600; }
  header .ver { color: var(--muted); font-size: 0.85rem; }
  main { padding: 1.25rem 1.5rem; max-width: 1200px; margin: 0 auto; }
  .actions { display: flex; flex-wrap: wrap; gap: 0.5rem; margin-bottom: 1.25rem; }
  button, .btn {
    background: var(--accent); color: #fff; border: none; border-radius: 8px;
    padding: 0.5rem 0.9rem; font-size: 0.9rem; cursor: pointer;
  }
  button.secondary { background: var(--card); border: 1px solid var(--border); }
  button:disabled { opacity: 0.5; cursor: wait; }
  .grid { display: grid; gap: 1rem; grid-template-columns: 1fr; }
  @media (min-width: 900px) { .grid { grid-template-columns: 2fr 1fr; } }
  .card {
    background: var(--card); border: 1px solid var(--border);
    border-radius: 12px; padding: 1rem;
  }
  .card h2 { margin: 0 0 0.75rem; font-size: 1rem; color: var(--muted); font-weight: 500; }
  table { width: 100%; border-collapse: collapse; font-size: 0.9rem; }
  th, td { text-align: left; padding: 0.45rem 0.5rem; border-bottom: 1px solid var(--border); }
  th { color: var(--muted); font-weight: 500; }
  .badge {
    display: inline-block; padding: 0.15rem 0.5rem; border-radius: 999px;
    font-size: 0.75rem; font-weight: 600;
  }
  .POSITIVE { background: #14532d; color: var(--pos); }
  .NEGATIVE { background: #7f1d1d; color: var(--neg); }
  .NEUTRAL, .NONE { background: #422006; color: var(--neu); }
  #log {
    margin-top: 1rem; font-family: ui-monospace, monospace; font-size: 0.8rem;
    color: var(--muted); white-space: pre-wrap; max-height: 160px; overflow: auto;
  }
  .row-form { display: flex; flex-wrap: wrap; gap: 0.4rem; margin-bottom: 0.75rem; }
  input, select {
    background: var(--bg); border: 1px solid var(--border); color: var(--text);
    border-radius: 6px; padding: 0.4rem 0.55rem; font-size: 0.9rem;
  }
  .wl-sym { display: inline-flex; align-items: center; gap: 0.35rem;
    background: var(--bg); border-radius: 6px; padding: 0.25rem 0.5rem; margin: 0.2rem; }
  .wl-sym button { padding: 0.1rem 0.35rem; font-size: 0.75rem; background: #334155; }
</style>
</head>
<body>
<header>
  <div>
    <h1>StockSense AI</h1>
    <div class="ver">Stage 6 — dashboard · <span id="apiVer">…</span></div>
  </div>
  <div class="actions" style="margin:0">
    <button onclick="run('ingest')">Ingest 30d</button>
    <button onclick="run('compute')">Compute indicators</button>
    <button onclick="run('evaluate')">Evaluate alerts</button>
    <button class="secondary" onclick="refresh()">Refresh</button>
  </div>
</header>
<main>
  <div class="grid">
    <section class="card">
      <h2>Stocks &amp; signals</h2>
      <div style="overflow-x:auto">
        <table>
          <thead>
            <tr>
              <th>Symbol</th><th>Price</th><th>Signal</th><th>RSI</th>
              <th>SMA50</th><th>Risk</th><th>P(up)</th>
            </tr>
          </thead>
          <tbody id="stockRows"></tbody>
        </table>
      </div>
    </section>
    <div>
      <section class="card" style="margin-bottom:1rem">
        <h2>Watchlist</h2>
        <div class="row-form">
          <select id="wlSelect"></select>
          <input id="wlSymbol" placeholder="Symbol e.g. TCS" size="10"/>
          <button onclick="addWatch()">Add</button>
        </div>
        <div id="wlSymbols"></div>
      </section>
      <section class="card">
        <h2>Alerts</h2>
        <div style="overflow-x:auto">
          <table>
            <thead><tr><th>Symbol</th><th>Type</th><th>Threshold</th><th>Last</th></tr></thead>
            <tbody id="alertRows"></tbody>
          </table>
        </div>
      </section>
    </div>
  </div>
  <div id="log"></div>
</main>
<script>
const logEl = document.getElementById('log');
function log(msg) {
  const t = new Date().toLocaleTimeString();
  logEl.textContent = `[${t}] ${msg}\n` + logEl.textContent;
}
async function jget(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(url + ' → ' + r.status);
  return r.json();
}
async function refresh() {
  try {
    const root = await jget('/');
    document.getElementById('apiVer').textContent = root.version || '';

    const dash = await jget('/api/dashboard/stocks');
    const tb = document.getElementById('stockRows');
    tb.innerHTML = '';
    (dash.stocks || []).forEach(s => {
      const tr = document.createElement('tr');
      const sig = s.technical_signal || 'NONE';
      tr.innerHTML = `<td><strong>${s.symbol}</strong></td>
        <td>${s.price != null ? Number(s.price).toFixed(2) : '—'}</td>
        <td><span class="badge ${sig}">${sig}</span></td>
        <td>${s.rsi_14 != null ? Number(s.rsi_14).toFixed(1) : '—'}</td>
        <td>${s.sma_50 != null ? Number(s.sma_50).toFixed(1) : '—'}</td>
        <td>${s.risk_level || '—'}</td>`;
      tb.appendChild(tr);
    });

    const wls = await jget('/api/watchlists');
    const sel = document.getElementById('wlSelect');
    const prev = sel.value;
    sel.innerHTML = '';
    (wls.watchlists || []).forEach(w => {
      const o = document.createElement('option');
      o.value = w.id; o.textContent = w.name + ' (' + (w.symbols||[]).length + ')';
      sel.appendChild(o);
    });
    if (prev) sel.value = prev;
    renderWlSymbols();

    const al = await jget('/api/alerts');
    const ab = document.getElementById('alertRows');
    ab.innerHTML = '';
    (al.alerts || []).forEach(a => {
      const tr = document.createElement('tr');
      tr.innerHTML = `<td>${a.symbol}</td><td>${a.alert_type}</td>
        <td>${a.threshold}</td><td>${a.last_triggered_at || '—'}</td>`;
      ab.appendChild(tr);
    });
    log('Refreshed');
  } catch (e) { log('Error: ' + e.message); }
}
function renderWlSymbols() {
  const sel = document.getElementById('wlSelect');
  const box = document.getElementById('wlSymbols');
  box.innerHTML = '';
  if (!sel.value) return;
  fetch('/api/watchlists').then(r => r.json()).then(wls => {
    const w = (wls.watchlists || []).find(x => String(x.id) === sel.value);
    if (!w) return;
    (w.symbols || []).forEach(sym => {
      const span = document.createElement('span');
      span.className = 'wl-sym';
      span.innerHTML = `${sym} <button onclick="rmWatch('${sym}')">×</button>`;
      box.appendChild(span);
    });
  });
}
document.getElementById('wlSelect').addEventListener('change', renderWlSymbols);
async function addWatch() {
  const id = document.getElementById('wlSelect').value;
  const sym = document.getElementById('wlSymbol').value.trim().toUpperCase();
  if (!id || !sym) return;
  try {
    await jget(`/api/watchlists/${id}/add?symbol=${encodeURIComponent(sym)}`);
    document.getElementById('wlSymbol').value = '';
    refresh();
  } catch (e) { log(e.message); }
}
async function rmWatch(sym) {
  const id = document.getElementById('wlSelect').value;
  try {
    await jget(`/api/watchlists/${id}/remove?symbol=${encodeURIComponent(sym)}`);
    refresh();
  } catch (e) { log(e.message); }
}
async function run(kind) {
  const map = {
    ingest: '/api/admin/ingest/synthetic?days=30',
    compute: '/api/admin/indicators/compute',
    evaluate: '/api/admin/alerts/evaluate'
  };
  log('Running ' + kind + '…');
  try {
    const data = await jget(map[kind]);
    log(kind + ': ' + JSON.stringify(data));
    await refresh();
  } catch (e) { log(e.message); }
}
refresh();
setInterval(refresh, 60000);
</script>
</body>
</html>)HTML";

  crow::response res(200);
  res.set_header("Content-Type", "text/html; charset=utf-8");
  res.body = html;
  return res;
}

void DashboardController::registerRoutes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/dashboard")([]() { return htmlPage(); });
  CROW_ROUTE(app, "/ui")([]() { return htmlPage(); });

  CROW_ROUTE(app, "/api/dashboard/stocks")
  ([this]() {
    auto rows = repo_->dashboardRows();
    crow::json::wvalue res;
    res["count"] = static_cast<int>(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
      res["stocks"][i]["symbol"] = rows[i].symbol;
      res["stocks"][i]["exchange"] = rows[i].exchange;
      if (rows[i].has_price) {
        res["stocks"][i]["price"] = rows[i].price;
        res["stocks"][i]["data_timestamp"] = rows[i].data_timestamp;
      } else {
        res["stocks"][i]["price"] = nullptr;
      }
      res["stocks"][i]["technical_signal"] = rows[i].technical_signal;
      res["stocks"][i]["risk_level"] = rows[i].risk_level;
      if (rows[i].has_indicators) {
        res["stocks"][i]["rsi_14"] = rows[i].rsi_14;
        res["stocks"][i]["sma_50"] = rows[i].sma_50;
        res["stocks"][i]["macd_hist"] = rows[i].macd_hist;
      }
    }
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/watchlists")
  ([this]() {
    int64_t userId = repo_->ensureDemoUser();
    repo_->ensureDefaultWatchlist(userId);
    auto lists = repo_->listWatchlists(userId);
    crow::json::wvalue res;
    res["count"] = static_cast<int>(lists.size());
    for (size_t i = 0; i < lists.size(); ++i) {
      res["watchlists"][i]["id"] = lists[i].id;
      res["watchlists"][i]["name"] = lists[i].name;
      for (size_t j = 0; j < lists[i].symbols.size(); ++j) {
        res["watchlists"][i]["symbols"][j] = lists[i].symbols[j];
      }
    }
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/watchlists/<int>/add")
      .methods("GET"_method, "POST"_method)(
          [this](const crow::request& req, int wid) {
            std::string symbol;
            if (auto q = req.url_params.get("symbol")) symbol = q;
            crow::json::wvalue res;
            if (symbol.empty()) {
              res["ok"] = false;
              res["error"] = "missing_symbol";
              return crow::response(400, res);
            }
            bool ok = repo_->addToWatchlist(wid, symbol);
            res["ok"] = ok;
            res["symbol"] = symbol;
            res["watchlist_id"] = wid;
            return crow::response(ok ? 200 : 404, res);
          });

  CROW_ROUTE(app, "/api/watchlists/<int>/remove")
      .methods("GET"_method, "POST"_method)(
          [this](const crow::request& req, int wid) {
            std::string symbol;
            if (auto q = req.url_params.get("symbol")) symbol = q;
            crow::json::wvalue res;
            bool ok = repo_->removeFromWatchlist(wid, symbol);
            res["ok"] = ok;
            res["symbol"] = symbol;
            return crow::response(ok ? 200 : 404, res);
          });
}

}  // namespace stocksense::api
