-- Seed data for Stage 2 development

INSERT INTO stocks (symbol, exchange, name, sector) VALUES
    ('TCS',      'NSE', 'Tata Consultancy Services', 'IT'),
    ('RELIANCE', 'NSE', 'Reliance Industries',       'Energy'),
    ('INFY',     'NSE', 'Infosys',                   'IT'),
    ('HDFCBANK', 'NSE', 'HDFC Bank',                 'Banking'),
    ('ICICIBANK','NSE', 'ICICI Bank',                'Banking'),
    ('SBIN',     'NSE', 'State Bank of India',       'Banking'),
    ('BHARTIARTL','NSE','Bharti Airtel',             'Telecom'),
    ('ITC',      'NSE', 'ITC Limited',               'FMCG'),
    ('LT',       'NSE', 'Larsen & Toubro',           'Infrastructure'),
    ('WIPRO',    'NSE', 'Wipro',                     'IT')
ON CONFLICT (symbol, exchange) DO NOTHING;

-- Demo user (password hash is a placeholder — not for production)
INSERT INTO users (email, password_hash, display_name, risk_profile)
VALUES ('demo@stocksense.local', '$2b$12$placeholderhashforstage2only', 'Demo User', 'moderate')
ON CONFLICT (email) DO NOTHING;

-- Sample EOD bars for TCS (illustrative prices)
INSERT INTO price_bars (stock_id, ts, open, high, low, close, volume)
SELECT s.id, d.ts, d.o, d.h, d.l, d.c, d.v
FROM stocks s
CROSS JOIN (VALUES
    ('2026-09-12 15:30:00+05:30'::timestamptz, 3520.00, 3555.00, 3510.00, 3540.25, 2100000),
    ('2026-09-15 15:30:00+05:30'::timestamptz, 3542.00, 3578.50, 3535.00, 3560.25, 1850000),
    ('2026-09-16 15:30:00+05:30'::timestamptz, 3562.00, 3585.00, 3548.00, 3572.80, 1920000),
    ('2026-09-17 15:30:00+05:30'::timestamptz, 3575.00, 3590.00, 3555.00, 3568.40, 1750000),
    ('2026-09-18 15:30:00+05:30'::timestamptz, 3570.00, 3598.00, 3562.00, 3585.10, 2010000),
    ('2026-09-19 15:30:00+05:30'::timestamptz, 3588.00, 3610.50, 3575.00, 3595.75, 2230000)
) AS d(ts, o, h, l, c, v)
WHERE s.symbol = 'TCS' AND s.exchange = 'NSE'
ON CONFLICT (stock_id, ts) DO NOTHING;

-- Sample indicators for latest TCS bar
INSERT INTO technical_indicators (
    stock_id, ts, sma_20, sma_50, rsi_14, macd, volume_sma_20
)
SELECT s.id, '2026-09-19 15:30:00+05:30'::timestamptz,
       3480.10, 3410.50, 64.20, 12.45, 1950000
FROM stocks s
WHERE s.symbol = 'TCS' AND s.exchange = 'NSE'
ON CONFLICT (stock_id, ts) DO NOTHING;
