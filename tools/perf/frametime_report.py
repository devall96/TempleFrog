"""Frame-time report for TempleFrog runs (GBARECOMP_FRAME_PHASE CSV).

Each CSV row is one presented frame. Its wall time is the sum of the phase
columns (guest + render + present + audio + pump + pacer); guest already
includes compile_us. That sum is the present-to-present interval.

Reports average fps, 1% and 0.1% lows (mean of the slowest 1% / 0.1% frames,
as fps), frame-time p50/p99/p99.9/max, hitches (frames over 25/33/50/100 ms),
stutter (consecutive frames differing by more than 4 ms), pacing against the
GBA's 59.7275 Hz interval, and the worst hitches with frame number, segment
and dominant phase. With --baseline it fails (exit 1) when lows or hitches
get worse.

Usage:
  python -I frametime_report.py RUN.csv [RUN2.csv ...] [--segments boot:0-700,menus:700-2200,level:2200-]
         [--skip 60] [--json out.json] [--baseline base.json] [--md out.md]
"""
import argparse
import csv
import json
import math
import sys

PHASES = ["guest_us", "render_us", "present_us", "audio_us", "pump_us", "pacer_us"]
TARGET_MS = 1000.0 / 59.7275  # GBA refresh, 16.743 ms
HITCH_MS = (25, 33, 50, 100)
STUTTER_MS = 4.0


def load(path):
    rows = []
    with open(path, newline="") as fh:
        for r in csv.DictReader(fh):
            row = {k: int(v) for k, v in r.items()}
            row["ms"] = sum(row[p] for p in PHASES) / 1000.0
            rows.append(row)
    if rows and rows[0]["guest_us"] == 0:  # first row has no previous present
        rows = rows[1:]
    return rows


def pct(sorted_vals, p):
    if not sorted_vals:
        return float("nan")
    k = (len(sorted_vals) - 1) * p / 100.0
    lo, hi = math.floor(k), math.ceil(k)
    return sorted_vals[lo] + (sorted_vals[hi] - sorted_vals[lo]) * (k - lo)


def low_fps(times, frac):
    n = max(1, int(round(len(times) * frac)))
    worst = sorted(times, reverse=True)[:n]
    return 1000.0 / (sum(worst) / n)


def parse_segments(spec):
    segs = []
    for part in filter(None, (spec or "").split(",")):
        name, rng = part.split(":")
        a, _, b = rng.partition("-")
        segs.append((name, int(a), int(b) if b else None))
    return segs


def segment_of(frame, segs):
    for name, a, b in segs:
        if frame >= a and (b is None or frame < b):
            return name
    return "-"


def stats(rows, segs, top=10):
    t = [r["ms"] for r in rows]
    if not t:
        return {}
    s = sorted(t)
    deltas = [abs(t[i] - t[i - 1]) for i in range(1, len(t))]
    hitches = []
    for r in rows:
        if r["ms"] > HITCH_MS[0]:
            phase = max(PHASES + ["compile_us"], key=lambda p: r[p])
            hitches.append({
                "frame": r["frame"], "ms": round(r["ms"], 2), "segment": segment_of(r["frame"], segs),
                "dominant_phase": phase.replace("_us", ""), "dominant_ms": round(r[phase] / 1000.0, 2),
                "compile_ms": round(r["compile_us"] / 1000.0, 2),
            })
    hitches.sort(key=lambda h: -h["ms"])
    dev = [abs(x - TARGET_MS) for x in t]
    return {
        "frames": len(t),
        "avg_fps": round(1000.0 * len(t) / sum(t), 2),
        "low_1pct_fps": round(low_fps(t, 0.01), 2),
        "low_0_1pct_fps": round(low_fps(t, 0.001), 2),
        "p50_ms": round(pct(s, 50), 3), "p99_ms": round(pct(s, 99), 3),
        "p99_9_ms": round(pct(s, 99.9), 3), "max_ms": round(s[-1], 3),
        **{f"hitches_over_{h}ms": sum(1 for x in t if x > h) for h in HITCH_MS},
        "stutter_events": sum(1 for d in deltas if d > STUTTER_MS),
        "pacing_mean_abs_dev_ms": round(sum(dev) / len(dev), 3),
        "pacing_within_2ms_pct": round(100.0 * sum(1 for d in dev if d <= 2.0) / len(dev), 2),
        "worst_hitches": hitches[:top],
    }


def by_segment(rows, segs):
    out = {}
    for name, a, b in segs:
        sub = [r for r in rows if r["frame"] >= a and (b is None or r["frame"] < b)]
        if sub:
            st = stats(sub, segs, top=3)
            out[name] = {k: v for k, v in st.items() if k != "worst_hitches"}
    return out


def compare(cur, base):
    """Return failure reasons when the current run is worse than the baseline."""
    fails = []
    for k in ("low_1pct_fps", "low_0_1pct_fps"):
        if cur[k] < base[k] * 0.98:  # 2% tolerance for run-to-run noise
            fails.append(f"{k} {cur[k]} < baseline {base[k]}")
    for h in HITCH_MS:
        k = f"hitches_over_{h}ms"
        if cur[k] > base[k]:
            fails.append(f"{k} {cur[k]} > baseline {base[k]}")
    if cur["stutter_events"] > base["stutter_events"] * 1.10 + 2:
        fails.append(f"stutter_events {cur['stutter_events']} > baseline {base['stutter_events']}")
    return fails


def md_table(name, st):
    keys = ["frames", "avg_fps", "low_1pct_fps", "low_0_1pct_fps", "p50_ms", "p99_ms", "p99_9_ms", "max_ms",
            *[f"hitches_over_{h}ms" for h in HITCH_MS], "stutter_events", "pacing_mean_abs_dev_ms",
            "pacing_within_2ms_pct"]
    lines = [f"### {name}", "", "| metric | value |", "|---|---:|"]
    lines += [f"| {k} | {st[k]} |" for k in keys]
    if st.get("worst_hitches"):
        lines += ["", "| frame | ms | segment | dominant phase | phase ms | compile ms |", "|---:|---:|---|---|---:|---:|"]
        lines += [f"| {h['frame']} | {h['ms']} | {h['segment']} | {h['dominant_phase']} | {h['dominant_ms']} | {h['compile_ms']} |"
                  for h in st["worst_hitches"]]
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--segments", default="")
    ap.add_argument("--skip", type=int, default=0, help="drop the first N frames (window open)")
    ap.add_argument("--json")
    ap.add_argument("--md")
    ap.add_argument("--baseline")
    a = ap.parse_args()
    segs = parse_segments(a.segments)
    report, md = {"target_ms": round(TARGET_MS, 3), "runs": {}}, []
    for path in a.csv:
        rows = [r for r in load(path) if r["frame"] >= a.skip]
        st = stats(rows, segs)
        st["segments"] = by_segment(rows, segs)
        report["runs"][path] = st
        md.append(md_table(path, st))
        for seg, ss in st["segments"].items():
            md.append(f"- {seg}: avg {ss['avg_fps']} fps, 1% low {ss['low_1pct_fps']}, 0.1% low "
                      f"{ss['low_0_1pct_fps']}, max {ss['max_ms']} ms, >33 ms x{ss['hitches_over_33ms']}, "
                      f"stutter x{ss['stutter_events']}\n")
    fails = []
    if a.baseline:
        base = json.load(open(a.baseline))["runs"]
        base_runs = list(base.values())
        for i, (path, st) in enumerate(report["runs"].items()):
            b = base.get(path) or base_runs[min(i, len(base_runs) - 1)]
            for f in compare(st, b):
                fails.append(f"{path}: {f}")
        report["baseline_failures"] = fails
        md.append("## Baseline comparison\n\n" + ("\n".join(f"- FAIL {f}" for f in fails) if fails else "- PASS: no worse lows or hitches\n"))
    text = "\n".join(md)
    print(text)
    if a.json:
        json.dump(report, open(a.json, "w"), indent=2)
    if a.md:
        open(a.md, "w").write(text)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
