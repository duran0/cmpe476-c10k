#!/usr/bin/env python3
"""Generate benchmark charts from benchmark_results.csv.

Outputs:
  - throughput_vs_clients.png
  - peak_rss_vs_clients.png
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except Exception as exc:  # pragma: no cover
    raise SystemExit(
        "matplotlib is required for chart generation. "
        "Install with: pip install matplotlib\n"
        f"details: {exc}"
    )


def load_rows(csv_path: Path):
    rows = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(
                {
                    "server": row["server"],
                    "clients": int(row["clients"]),
                    "rps": float(row["rps"]),
                    "rss_peak": int(row["rss_kb_peak"]),
                }
            )
    if not rows:
        raise SystemExit(f"no rows found in {csv_path}")
    return rows


def by_server(rows):
    grouped = {"threadserv": [], "epollserv": []}
    for row in rows:
        if row["server"] in grouped:
            grouped[row["server"]].append(row)
    for key in grouped:
        grouped[key].sort(key=lambda r: r["clients"])
    return grouped


def plot_throughput(grouped, out_path: Path):
    plt.figure(figsize=(8, 5))
    for server, color in [("threadserv", "#1f77b4"), ("epollserv", "#d62728")]:
        xs = [r["clients"] for r in grouped[server]]
        ys = [r["rps"] for r in grouped[server]]
        plt.plot(xs, ys, marker="o", linewidth=2, label=server, color=color)
    plt.title("Throughput vs Concurrent Clients")
    plt.xlabel("Clients")
    plt.ylabel("Requests per second (RPS)")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()


def plot_peak_rss(grouped, out_path: Path):
    clients = sorted({r["clients"] for rows in grouped.values() for r in rows})
    thread = {r["clients"]: r["rss_peak"] for r in grouped["threadserv"]}
    epoll = {r["clients"]: r["rss_peak"] for r in grouped["epollserv"]}

    x = list(range(len(clients)))
    width = 0.36

    plt.figure(figsize=(8, 5))
    plt.bar([v - width / 2 for v in x], [thread.get(c, 0) for c in clients], width, label="threadserv", color="#1f77b4")
    plt.bar([v + width / 2 for v in x], [epoll.get(c, 0) for c in clients], width, label="epollserv", color="#d62728")
    plt.title("Peak RSS vs Concurrent Clients")
    plt.xlabel("Clients")
    plt.ylabel("Peak RSS (kB)")
    plt.xticks(x, [str(c) for c in clients])
    plt.grid(axis="y", linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()


def main():
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("benchmark_results.csv")
    if not csv_path.exists():
        raise SystemExit(f"missing input file: {csv_path}")

    rows = load_rows(csv_path)
    grouped = by_server(rows)

    plot_throughput(grouped, Path("throughput_vs_clients.png"))
    plot_peak_rss(grouped, Path("peak_rss_vs_clients.png"))

    print("generated throughput_vs_clients.png")
    print("generated peak_rss_vs_clients.png")


if __name__ == "__main__":
    main()
