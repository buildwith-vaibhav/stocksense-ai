-- StockSense AI — Stage 2 schema
-- PostgreSQL 14+ (TimescaleDB optional for later stages)

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Users
CREATE TABLE IF NOT EXISTS users (
    id              BIGSERIAL PRIMARY KEY,
    email           TEXT NOT NULL UNIQUE,
    password_hash   TEXT NOT NULL,
    display_name    TEXT,
    risk_profile    TEXT NOT NULL DEFAULT 'moderate'
                        CHECK (risk_profile IN ('conservative', 'moderate', 'aggressive')),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Stocks (master list)
CREATE TABLE IF NOT EXISTS stocks (
    id              BIGSERIAL PRIMARY KEY,
    symbol          TEXT NOT NULL,
    exchange        TEXT NOT NULL DEFAULT 'NSE',
    name            TEXT NOT NULL,
    sector          TEXT,
    isin            TEXT,
    is_active       BOOLEAN NOT NULL DEFAULT TRUE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (symbol, exchange)
);

CREATE INDEX IF NOT EXISTS idx_stocks_symbol ON stocks (symbol);

-- Watchlists
CREATE TABLE IF NOT EXISTS watchlists (
    id              BIGSERIAL PRIMARY KEY,
    user_id         BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name            TEXT NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (user_id, name)
);

CREATE TABLE IF NOT EXISTS watchlist_stocks (
    watchlist_id    BIGINT NOT NULL REFERENCES watchlists(id) ON DELETE CASCADE,
    stock_id        BIGINT NOT NULL REFERENCES stocks(id) ON DELETE CASCADE,
    added_at        TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (watchlist_id, stock_id)
);

-- Price bars (EOD / intraday later)
CREATE TABLE IF NOT EXISTS price_bars (
    id              BIGSERIAL PRIMARY KEY,
    stock_id        BIGINT NOT NULL REFERENCES stocks(id) ON DELETE CASCADE,
    ts              TIMESTAMPTZ NOT NULL,
    open            NUMERIC(18,4) NOT NULL,
    high            NUMERIC(18,4) NOT NULL,
    low             NUMERIC(18,4) NOT NULL,
    close           NUMERIC(18,4) NOT NULL,
    volume          BIGINT NOT NULL DEFAULT 0,
    UNIQUE (stock_id, ts)
);

CREATE INDEX IF NOT EXISTS idx_price_bars_stock_ts
    ON price_bars (stock_id, ts DESC);

-- Technical indicators (pre-computed)
CREATE TABLE IF NOT EXISTS technical_indicators (
    id              BIGSERIAL PRIMARY KEY,
    stock_id        BIGINT NOT NULL REFERENCES stocks(id) ON DELETE CASCADE,
    ts              TIMESTAMPTZ NOT NULL,
    sma_20          NUMERIC(18,4),
    sma_50          NUMERIC(18,4),
    sma_200         NUMERIC(18,4),
    ema_12          NUMERIC(18,4),
    ema_26          NUMERIC(18,4),
    rsi_14          NUMERIC(10,4),
    macd            NUMERIC(18,6),
    macd_signal     NUMERIC(18,6),
    macd_hist       NUMERIC(18,6),
    atr_14          NUMERIC(18,4),
    volume_sma_20   NUMERIC(18,2),
    UNIQUE (stock_id, ts)
);

CREATE INDEX IF NOT EXISTS idx_tech_ind_stock_ts
    ON technical_indicators (stock_id, ts DESC);

-- Alerts
CREATE TABLE IF NOT EXISTS alerts (
    id                  BIGSERIAL PRIMARY KEY,
    user_id             BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    stock_id            BIGINT NOT NULL REFERENCES stocks(id) ON DELETE CASCADE,
    alert_type          TEXT NOT NULL
                            CHECK (alert_type IN (
                                'price_above', 'price_below',
                                'pct_change', 'volume_spike',
                                'ma_crossover', 'rsi_overbought', 'rsi_oversold'
                            )),
    threshold           NUMERIC(18,4),
    is_active           BOOLEAN NOT NULL DEFAULT TRUE,
    cooldown_minutes    INT NOT NULL DEFAULT 60,
    last_triggered_at   TIMESTAMPTZ,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_alerts_active
    ON alerts (stock_id) WHERE is_active = TRUE;

-- Predictions (placeholder for Stage 7)
CREATE TABLE IF NOT EXISTS predictions (
    id              BIGSERIAL PRIMARY KEY,
    stock_id        BIGINT NOT NULL REFERENCES stocks(id) ON DELETE CASCADE,
    model_version   TEXT NOT NULL,
    horizon_days    INT NOT NULL DEFAULT 5,
    probability_up  NUMERIC(6,4),
    confidence      TEXT,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_predictions_stock_created
    ON predictions (stock_id, created_at DESC);

-- Notification log
CREATE TABLE IF NOT EXISTS notification_logs (
    id              BIGSERIAL PRIMARY KEY,
    alert_id        BIGINT REFERENCES alerts(id) ON DELETE SET NULL,
    prediction_id   BIGINT REFERENCES predictions(id) ON DELETE SET NULL,
    channel         TEXT NOT NULL CHECK (channel IN ('telegram', 'email', 'web_push')),
    status          TEXT NOT NULL CHECK (status IN (
                        'queued', 'sent', 'failed', 'suppressed_cooldown'
                    )),
    payload         JSONB,
    sent_at         TIMESTAMPTZ
);

-- Model versions (placeholder)
CREATE TABLE IF NOT EXISTS model_versions (
    id              BIGSERIAL PRIMARY KEY,
    version_tag     TEXT NOT NULL UNIQUE,
    description     TEXT,
    is_active       BOOLEAN NOT NULL DEFAULT FALSE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
