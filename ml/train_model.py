#!/usr/bin/env python3
"""
StockSense AI — Stage 7 model trainer

Trains a logistic regression on engineered features and exports:
  models/stocksense_lr.json   (weights for C++ inference)

Optional: if skl2onnx is installed, also writes models/stocksense_lr.onnx

Usage:
  pip install scikit-learn numpy
  python ml/train_model.py
  python ml/train_model.py --from-csv data/sample_bars.csv
"""

from __future__ import annotations

import argparse
import json
import math
import random
from pathlib import Path

import numpy as np

try:
    from sklearn.linear_model import LogisticRegression
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import accuracy_score, roc_auc_score
except ImportError as e:
    raise SystemExit("Install: pip install scikit-learn numpy") from e

FEATURE_NAMES = [
    "rsi_14_norm",       # RSI/100
    "price_vs_sma20",    # (price - sma20) / price
    "price_vs_sma50",
    "sma20_vs_sma50",
    "macd_hist_norm",    # tanh scaled
    "atr_pct",           # atr/price
]

ROOT = Path(__file__).resolve().parents[1]
MODEL_OUT = ROOT / "models" / "stocksense_lr.json"


def make_synthetic(n: int = 4000, seed: int = 42) -> tuple[np.ndarray, np.ndarray]:
    """Generate labeled feature rows with a weak but learnable signal."""
    rng = np.random.default_rng(seed)
    X = np.zeros((n, len(FEATURE_NAMES)), dtype=np.float64)
    y = np.zeros(n, dtype=np.int32)

    for i in range(n):
        rsi = rng.uniform(15, 85)
        price = rng.uniform(200, 4000)
        sma20 = price * rng.uniform(0.92, 1.08)
        sma50 = price * rng.uniform(0.90, 1.10)
        macd_h = rng.normal(0, 15)
        atr = price * rng.uniform(0.005, 0.04)

        feats = np.array(
            [
                rsi / 100.0,
                (price - sma20) / max(price, 1e-6),
                (price - sma50) / max(price, 1e-6),
                (sma20 - sma50) / max(price, 1e-6),
                math.tanh(macd_h / 20.0),
                atr / max(price, 1e-6),
            ],
            dtype=np.float64,
        )
        X[i] = feats

        # Latent edge: oversold + price above sma50 + positive macd → up
        score = (
            -1.5 * (feats[0] - 0.5)  # prefer lower RSI slightly for bounce
            + 2.0 * feats[2]
            + 1.2 * feats[4]
            + 0.8 * feats[3]
            + rng.normal(0, 0.8)
        )
        y[i] = 1 if score > 0 else 0

    return X, y


def features_from_ohlcv(closes: list[float]) -> list[list[float]] | None:
    """Build feature rows from a close series (need >= 60 points)."""
    if len(closes) < 60:
        return None
    rows = []
    for i in range(59, len(closes)):
        window = closes[: i + 1]
        price = window[-1]

        def sma(p: int) -> float:
            return sum(window[-p:]) / p

        # RSI 14
        gains, losses = [], []
        for j in range(-14, 0):
            d = window[j] - window[j - 1]
            gains.append(max(d, 0.0))
            losses.append(max(-d, 0.0))
        ag, al = sum(gains) / 14, sum(losses) / 14
        rsi = 100.0 if al < 1e-12 else 100.0 - (100.0 / (1.0 + ag / al))

        sma20 = sma(20)
        sma50 = sma(50)
        # crude macd hist proxy
        ema12 = price
        ema26 = price
        k12, k26 = 2 / 13, 2 / 27
        e12 = e26 = window[-60]
        for c in window[-60:]:
            e12 = c * k12 + e12 * (1 - k12)
            e26 = c * k26 + e26 * (1 - k26)
        macd_h = e12 - e26
        atr = abs(window[-1] - window[-2]) if len(window) > 1 else 0.01 * price

        rows.append(
            [
                rsi / 100.0,
                (price - sma20) / max(price, 1e-6),
                (price - sma50) / max(price, 1e-6),
                (sma20 - sma50) / max(price, 1e-6),
                math.tanh(macd_h / 20.0),
                atr / max(price, 1e-6),
            ]
        )
    return rows


def load_csv_closes(path: Path) -> dict[str, list[float]]:
    import csv

    by_sym: dict[str, list[tuple[str, float]]] = {}
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].lower() in ("symbol", "ticker"):
                continue
            if len(row) < 6:
                continue
            sym, date, close = row[0], row[1], float(row[5])
            by_sym.setdefault(sym, []).append((date, close))
    out: dict[str, list[float]] = {}
    for sym, pairs in by_sym.items():
        pairs.sort(key=lambda x: x[0])
        out[sym] = [c for _, c in pairs]
    return out


def train(X: np.ndarray, y: np.ndarray) -> dict:
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.25, random_state=42, stratify=y
    )
    clf = LogisticRegression(max_iter=1000, random_state=42)
    clf.fit(X_train, y_train)
    proba = clf.predict_proba(X_test)[:, 1]
    pred = (proba >= 0.5).astype(int)
    acc = float(accuracy_score(y_test, pred))
    try:
        auc = float(roc_auc_score(y_test, proba))
    except ValueError:
        auc = float("nan")

    payload = {
        "model_type": "logistic_regression",
        "version": "stage7-v1",
        "feature_names": FEATURE_NAMES,
        "coefficients": clf.coef_[0].tolist(),
        "intercept": float(clf.intercept_[0]),
        "metrics": {"accuracy": acc, "auc": auc, "n_train": int(len(y_train))},
        "notes": "probability_up = sigmoid(dot(coef, features) + intercept)",
    }
    print(f"accuracy={acc:.3f}  auc={auc:.3f}  n_train={len(y_train)}")
    return payload


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--from-csv", type=Path, default=None)
    ap.add_argument("--synthetic", type=int, default=4000)
    args = ap.parse_args()

    if args.from_csv and args.from_csv.exists():
        print(f"Loading CSV {args.from_csv}")
        closes_map = load_csv_closes(args.from_csv)
        rows, labels = [], []
        rng = random.Random(42)
        for sym, closes in closes_map.items():
            feats = features_from_ohlcv(closes)
            if not feats:
                continue
            # weak label: next-day up
            for i, f in enumerate(feats):
                idx = 59 + i
                if idx + 1 >= len(closes):
                    break
                rows.append(f)
                labels.append(1 if closes[idx + 1] > closes[idx] else 0)
        if len(rows) < 50:
            print("Not enough CSV rows; falling back to synthetic")
            X, y = make_synthetic(args.synthetic)
        else:
            X, y = np.array(rows), np.array(labels)
            print(f"CSV features: {len(X)} rows")
    else:
        print(f"Synthetic training n={args.synthetic}")
        X, y = make_synthetic(args.synthetic)

    payload = train(X, y)
    MODEL_OUT.parent.mkdir(parents=True, exist_ok=True)
    MODEL_OUT.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"Wrote {MODEL_OUT}")

    # Optional ONNX
    try:
        from skl2onnx import convert_sklearn
        from skl2onnx.common.data_types import FloatTensorType
        from sklearn.linear_model import LogisticRegression

        clf = LogisticRegression(max_iter=1000, random_state=42)
        clf.coef_ = np.array([payload["coefficients"]])
        clf.intercept_ = np.array([payload["intercept"]])
        clf.classes_ = np.array([0, 1])
        # refit properly
        clf.fit(X, y)
        onx = convert_sklearn(
            clf,
            initial_types=[("features", FloatTensorType([None, len(FEATURE_NAMES)]))],
        )
        onnx_path = ROOT / "models" / "stocksense_lr.onnx"
        onnx_path.write_bytes(onx.SerializeToString())
        print(f"Wrote {onnx_path}")
    except Exception as e:
        print(f"ONNX export skipped ({e})")


if __name__ == "__main__":
    main()
