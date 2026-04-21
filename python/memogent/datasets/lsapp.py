"""LSApp loader.

Expected columns (TSV):
    user_id    session_id    timestamp    app_name    event_type

Source: https://github.com/aliannejadi/LSApp (599,635 events, 87 apps, 292 users).
"""
from __future__ import annotations

import csv
from datetime import datetime
from pathlib import Path
from typing import Iterator

from ..types import AppEvent, EventType


_EVENT_MAP = {
    "Opened": EventType.APP_OPEN,
    "Closed": EventType.APP_CLOSE,
    "User Interaction": EventType.USER_INTERACTION,
    "Broken": EventType.USER_INTERACTION,
}


def _parse_ts(raw: str) -> float:
    try:
        # LSApp uses "YYYY-MM-DD HH:MM:SS"
        return datetime.strptime(raw.strip(), "%Y-%m-%d %H:%M:%S").timestamp()
    except ValueError:
        return 0.0


def load_lsapp_tsv(path: str | Path, user_id: str | None = None) -> Iterator[AppEvent]:
    """Stream `AppEvent`s from an `lsapp.tsv` file.

    If `user_id` is provided, only events for that user are yielded. The file
    is sorted by timestamp, so callers can consume it sequentially.
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"LSApp TSV not found at {path}")
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.reader(f, delimiter="\t")
        header = next(reader, None)
        if not header:
            return
        # find column indexes robustly (order differs between mirrors)
        idx = {col: header.index(col) for col in header}
        col_user = idx.get("user_id", 0)
        _col_sess = idx.get("session_id", 1)  # noqa: F841
        col_ts = idx.get("timestamp", 2)
        col_app = idx.get("app_name", 3)
        col_ev = idx.get("event_type", 4)
        for row in reader:
            if len(row) < 5:
                continue
            if user_id is not None and row[col_user] != user_id:
                continue
            ts = _parse_ts(row[col_ts])
            ev_type = _EVENT_MAP.get(row[col_ev].strip(), EventType.USER_INTERACTION)
            hour = None
            try:
                hour = int(row[col_ts].split()[1].split(":")[0])
            except Exception:
                pass
            yield AppEvent(
                type=ev_type,
                app_id=row[col_app].strip(),
                timestamp=ts,
                hour_of_day=hour,
            )
