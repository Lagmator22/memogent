"""`mem` CLI entry."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Optional

from rich.console import Console
from rich.panel import Panel
from rich.table import Table

from ..bench import run_harness
from ..config import Config
from ..datasets import generate_synthetic, load_lsapp_tsv
from ..orchestrator import Orchestrator

console = Console()


def cli(argv: Optional[list[str]] = None) -> int:
    import argparse
    parser = argparse.ArgumentParser(prog="mem", description="Memogent reference CLI")
    sub = parser.add_subparsers(dest="cmd", required=False)

    p = sub.add_parser("orchestrate", help="replay a synthetic trace live")
    p.add_argument("--n", type=int, default=2000)
    p.add_argument("--cap", type=int, default=16)
    p.add_argument("--predictor", default="freq_recency")
    p.add_argument("--cache", default="context_arc")

    p = sub.add_parser("bench", help="run the full KPI harness")
    p.add_argument("--trace", default=None, help="Path to LSApp TSV (optional)")
    p.add_argument("--user", default=None, help="Filter to one LSApp user_id")
    p.add_argument("--n", type=int, default=5000)
    p.add_argument("--cap", type=int, default=16)
    p.add_argument("--output", default="bench_results/latest.json")

    p = sub.add_parser("train", help="train the LSTM predictor (requires torch)")
    p.add_argument("--trace", default=None)
    p.add_argument("--epochs", type=int, default=5)

    args = parser.parse_args(argv)
    if not args.cmd:
        parser.print_help()
        return 0

    if args.cmd == "orchestrate":
        return _cmd_orchestrate(args)
    if args.cmd == "bench":
        return _cmd_bench(args)
    if args.cmd == "train":
        return _cmd_train(args)
    return 0


def _cmd_orchestrate(args) -> int:
    cfg = Config(predictor=args.predictor, cache_policy=args.cache, cache_capacity_app=args.cap)
    orch = Orchestrator(cfg)
    events = generate_synthetic(n_events=args.n)
    console.print(
        Panel.fit(
            f"replaying {len(events)} synthetic events\n"
            f"predictor={args.predictor}   cache={args.cache}   cap={args.cap}",
            title="mem orchestrate",
        )
    )
    for i, e in enumerate(events):
        orch.record_event(e)
        if i % 100 == 0:
            orch.tick()
    result = orch.tick()
    snap = orch.kpis()
    tbl = Table(title="post-replay snapshot")
    tbl.add_column("field")
    tbl.add_column("value")
    tbl.add_row("power_mode", result.power_mode.name)
    tbl.add_row("rationale", result.plan.rationale)
    tbl.add_row("cache hit_rate", f"{snap.cache_hit_rate:.3f}")
    tbl.add_row("prediction top1", f"{snap.prediction_accuracy_top1:.3f}")
    tbl.add_row("prediction top3", f"{snap.prediction_accuracy_top3:.3f}")
    tbl.add_row("p95 decision ms", f"{snap.p95_decision_latency_ms:.2f}")
    console.print(tbl)
    return 0


def _cmd_bench(args) -> int:
    if args.trace:
        events = list(load_lsapp_tsv(args.trace, user_id=args.user))
        console.print(f"loaded {len(events)} events from {args.trace}")
        if args.n and len(events) > args.n:
            events = events[: args.n]
    else:
        events = generate_synthetic(n_events=args.n)
        console.print(f"generated {len(events)} synthetic events (pass --trace to use LSApp)")
    report = run_harness(events, cache_capacity=args.cap)
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(report.to_json())

    tbl = Table(title="KPI report", show_lines=True)
    tbl.add_column("metric")
    tbl.add_column("value", justify="right")
    tbl.add_column("target")
    tbl.add_column("pass?")
    for name, target_val, val in [
        ("app_load_time_improvement_pct", report.targets.app_load_time_improvement_pct, report.kpis["app_load_time_improvement_pct"]),
        ("launch_time_improvement_pct", report.targets.launch_time_improvement_pct, report.kpis["launch_time_improvement_pct"]),
        ("thrashing_reduction_pct", report.targets.thrashing_reduction_pct, report.kpis["thrashing_reduction_pct"]),
        ("prediction_accuracy_top3", report.targets.prediction_accuracy_top3, report.kpis["prediction_accuracy_top3"]),
        ("cache_hit_rate", report.targets.cache_hit_rate, report.kpis["cache_hit_rate"]),
        ("memory_utilization_efficiency_pct", report.targets.memory_utilization_efficiency_pct, report.kpis["memory_utilization_efficiency_pct"]),
    ]:
        ok = val >= target_val
        tbl.add_row(
            name,
            f"{val:.2f}",
            f"≥{target_val:.2f}",
            "[green]yes[/green]" if ok else "[red]no[/red]",
        )
    console.print(tbl)
    console.print(
        f"[dim]report written to[/dim] {out}\n"
        f"[dim]elapsed:[/dim] {report.elapsed_s:.2f}s  "
        f"[dim]events:[/dim] {report.events}  [dim]apps:[/dim] {report.apps_seen}"
    )
    fail = sum(1 for v in report.pass_fail.values() if not v)
    return 0 if fail == 0 else 1


def _cmd_train(args) -> int:
    try:
        import torch  # noqa: F401
    except ImportError:
        console.print("[red]install extras first:[/red] pip install -e .[train]")
        return 2
    console.print("[yellow]LSTM training is a stub in v0.1 — see docs/PREDICTOR_DESIGN.md[/yellow]")
    return 0


if __name__ == "__main__":
    sys.exit(cli())
