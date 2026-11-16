"""Write a gbarecomp-keyinput-v1 input trace for TempleFrog perf runs.

Frames [0, walk_start) repeat gbarecomp's headless "menu" demo track (runtime.cpp
demo_keyinput_for_frame): Start/A/directions/B, each held 6 frames then released
for 6. From the Konami logo it reaches the first level by frame ~2400 on a fresh
save. From walk_start on, it holds a direction for 90 frames at a time, cycling
R/D/L/U, with an A tap every 60 frames: level traversal for the perf suite.

Usage: python -I make_trace.py OUT.txt [--frames 5400] [--walk-start 2400]
"""
import argparse

KEY_A, KEY_B, KEY_START = 1 << 0, 1 << 1, 1 << 3
KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN = 1 << 4, 1 << 5, 1 << 6, 1 << 7
ALL_UP = 0x03FF
MENU = [KEY_START, KEY_A, KEY_A, KEY_DOWN, KEY_A, KEY_RIGHT,
        KEY_B, KEY_A, KEY_LEFT, KEY_A, KEY_UP, KEY_B]
DIRS = [KEY_RIGHT, KEY_DOWN, KEY_LEFT, KEY_UP]


def menu_keys(f: int) -> int:
    if (f // 6) & 1:
        return ALL_UP
    return ALL_UP & ~MENU[(f // 12) % len(MENU)]


def walk_keys(f: int) -> int:
    btn = DIRS[(f // 90) % 4]
    if f % 60 < 2:
        btn |= KEY_A
    return ALL_UP & ~btn


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=5400)
    ap.add_argument("--walk-start", type=int, default=2400)
    a = ap.parse_args()
    rows, last = [], None
    for f in range(a.frames):
        k = menu_keys(f) if f < a.walk_start else walk_keys(f - a.walk_start)
        if k != last:
            rows.append(f"{f},0x{k:04X}")
            last = k
    with open(a.out, "w", newline="\n") as fh:
        fh.write("# gbarecomp-keyinput-v1\n# frame,keyinput_active_low\n")
        fh.write("\n".join(rows) + "\n")
    print(f"wrote {len(rows)} events over {a.frames} frames -> {a.out}")


if __name__ == "__main__":
    main()
