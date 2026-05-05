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

    p = sub.add_parser(
        "watch",
        help="(macOS) live-watch your frontmost app and predict the next one",
    )
    p.add_argument("--interval", type=float, default=2.0,
                   help="seconds between samples")
    p.add_argument("--state", default=str(Path.home() / ".memogent" / "watch.json"),
                   help="path to persisted predictor state (defaults to ~/.memogent/watch.json)")
    p.add_argument("--predictor", default="markov2")
    p.add_argument("--cap", type=int, default=12)

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
    if args.cmd == "watch":
        return _cmd_watch(args)
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


def _cmd_watch(args) -> int:
    """Live-watch the macOS frontmost app, learn from sequences,
    and predict the next app you're likely to switch to.

    Persists the predictor's accumulated counts to disk so each session
    builds on the last — practical personal use, not a benchmark.
    """
    import json as _json
    import platform
    import subprocess
    import time as _time
    from datetime import datetime

    from rich.live import Live

    from ..predictor import make_predictor
    from ..config import Config
    from ..types import AppEvent, EventType

    if platform.system() != "Darwin":
        console.print("[red]`mem watch` currently uses macOS AppleScript to read the frontmost app.[/red]")
        return 2

    state_path = Path(args.state).expanduser()
    state_path.parent.mkdir(parents=True, exist_ok=True)

    predictor = make_predictor(Config(predictor=args.predictor))
    history: list[tuple[float, str]] = []
    if state_path.exists():
        try:
            saved = _json.loads(state_path.read_text())
            for ts, app in saved.get("history", [])[-2000:]:
                ev = AppEvent(
                    type=EventType.APP_OPEN, app_id=app, timestamp=ts,
                    hour_of_day=datetime.fromtimestamp(ts).hour,
                )
                predictor.observe(ev)
                history.append((ts, app))
            console.print(f"[dim]restored {len(history)} events from {state_path}[/dim]")
        except (OSError, ValueError):
            pass

    last_app: str = ""
    n_predictions = 0
    n_top1_hits = 0
    n_top3_hits = 0
    pending_preds: list[str] = []

    def _frontmost() -> str:
        try:
            out = subprocess.run(
                ["osascript", "-e",
                 'tell application "System Events" to get name of first process whose frontmost is true'],
                capture_output=True, text=True, timeout=2.0,
            )
            return out.stdout.strip() or ""
        except (subprocess.SubprocessError, OSError):
            return ""

    def _render() -> Table:
        tbl = Table(title="mem watch — live next-app prediction", show_lines=False)
        tbl.add_column("field")
        tbl.add_column("value")
        preds = predictor.predict(3)
        tbl.add_row("current",       f"[bold]{last_app or '(detecting…)'}[/bold]")
        tbl.add_row("next likely",   ", ".join(p.app_id for p in preds) if preds else "(learning…)")
        if n_predictions:
            tbl.add_row("top-1 acc",  f"{n_top1_hits / n_predictions:.0%}  ({n_top1_hits}/{n_predictions})")
            tbl.add_row("top-3 acc",  f"{n_top3_hits / n_predictions:.0%}  ({n_top3_hits}/{n_predictions})")
        tbl.add_row("history",       f"{len(history)} events")
        tbl.add_row("state file",    str(state_path))
        return tbl

    console.print(
        Panel.fit(
            f"watching frontmost app every {args.interval:.1f}s · predictor={args.predictor}\n"
            f"state persisted to {state_path}\n"
            "[dim]Ctrl-C to exit (state is saved)[/dim]",
            title="mem watch",
        )
    )

    try:
        with Live(_render(), console=console, refresh_per_second=2) as live:
            while True:
                cur = _frontmost()
                if cur and cur != last_app:
                    # Score current pending predictions against the new switch.
                    if pending_preds:
                        n_predictions += 1
                        if pending_preds[0] == cur:
                            n_top1_hits += 1
                        if cur in pending_preds:
                            n_top3_hits += 1
                    ts = _time.time()
                    ev = AppEvent(
                        type=EventType.APP_OPEN, app_id=cur, timestamp=ts,
                        hour_of_day=datetime.fromtimestamp(ts).hour,
                    )
                    predictor.observe(ev)
                    history.append((ts, cur))
                    pending_preds = [p.app_id for p in predictor.predict(3)]
                    last_app = cur
                    live.update(_render())
                else:
                    live.update(_render())
                _time.sleep(args.interval)
    except KeyboardInterrupt:
        try:
            state_path.write_text(_json.dumps({
                "history": history[-2000:],
                "predictor": args.predictor,
            }))
            console.print(f"\n[green]saved {len(history)} events to {state_path}[/green]")
        except OSError as exc:
            console.print(f"\n[red]failed to save state:[/red] {exc}")
        return 0




if __name__ == "__main__":
    sys.exit(cli())
