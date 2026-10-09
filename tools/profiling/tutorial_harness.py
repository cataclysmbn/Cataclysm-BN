#!/usr/bin/env python3
"""Play the real curses tutorial through a PTY and summarize Tracy CPU zones."""

import argparse
import codecs
import csv
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import pty
import re
import select
import signal
import socket
import statistics
import struct
import subprocess
import sys
import termios
import time

import pyte


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "client"))
from profile_isolation import profile_arguments, verify_profile_paths  # noqa: E402
TRACY_REVISION = "16bac5f80790fb29ca2bd1eab903f02e8ea0732a"
OPTIONS = {"USE_LANG": "en_US", "ANIMATIONS": "false", "AUTOSAVE": "false"}
INPUT_WAIT = "get_player_input_noanim_blocking_handle_mouseview"
LESSONS = json.loads((ROOT / "data/json/snippets/tutorial.json").read_text())[0]["text"]


def percentile(values, fraction):
    """Linearly interpolate a percentile, retaining all outliers."""
    ordered = sorted(values)
    index = (len(ordered) - 1) * fraction
    lower = int(index)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (index - lower)


def zone_statistics(events):
    grouped = {}
    for event in events:
        key = (event["name"], event["file"], event["line"])
        grouped.setdefault(key, []).append(event["duration"])
    return sorted(
        (
            {
                "name": name,
                "file": file,
                "line": line,
                "count": len(values),
                "total_ms": sum(values) / 1e6,
                "median_ms": statistics.median(values) / 1e6,
                "p95_ms": percentile(values, 0.95) / 1e6,
                "max_ms": max(values) / 1e6,
            }
            for (name, file, line), values in grouped.items()
        ),
        key=lambda zone: zone["total_ms"],
        reverse=True,
    )


def summarize(events, *, actions):
    """Select the final gameplay actions and exclude setup and input waiting."""
    switches = sorted(
        (e for e in events if e["name"] == "handle_action_alive_switch"),
        key=lambda e: e["start"],
    )
    if len(switches) < actions:
        raise ValueError(f"Expected {actions} actions, found only {len(switches)}")
    selected = switches[-actions:]
    thread = selected[0]["thread"]
    if any(e["thread"] != thread for e in selected):
        raise ValueError("Gameplay actions span multiple threads")
    start = selected[0]["start"]
    last_end = selected[-1]["start"] + selected[-1]["duration"]
    next_inputs = [
        e["start"]
        for e in events
        if e["thread"] == thread
        and e["name"] == "get_player_input"
        and e["start"] >= last_end
    ]
    if not next_inputs:
        raise ValueError("Trace does not reach input readiness after the final action")
    end = min(next_inputs)
    window = [
        e
        for e in events
        if e["thread"] == thread
        and e["start"] >= start
        and e["start"] + e["duration"] <= end
    ]
    waiting = sum(e["duration"] for e in window if e["name"] == INPUT_WAIT)
    return {
        "actions": actions,
        "thread": thread,
        "start_ns": start,
        "end_ns": end,
        "window_ms": (end - start) / 1e6,
        "input_wait_ms": waiting / 1e6,
        "wall_minus_input_wait_ms": (end - start - waiting) / 1e6,
        "zones": zone_statistics(window),
    }


def load_events(path):
    with path.open(newline="") as source:
        return [
            {
                "name": row["name"],
                "file": row["src_file"],
                "line": int(row["src_line"]),
                "thread": row["thread"],
                "start": int(row["ns_since_start"]),
                "duration": int(row["exec_time_ns"]),
            }
            for row in csv.DictReader(source, delimiter=";")
        ]


def nonwaiting_self_events(inclusive_events, self_events, *, window):
    candidates = [
        event
        for event in inclusive_events
        if event["thread"] == window["thread"]
        and event["start"] >= window["start_ns"]
        and event["start"] + event["duration"] <= window["end_ns"]
    ]
    waits = [event for event in candidates if event["name"] == INPUT_WAIT]
    keys = {
        (event["name"], event["file"], event["line"], event["thread"], event["start"])
        for event in candidates
        if not any(
            wait["start"] <= event["start"]
            and event["start"] + event["duration"] <= wait["start"] + wait["duration"]
            for wait in waits
        )
    }
    return [
        event
        for event in self_events
        if (
            event["name"],
            event["file"],
            event["line"],
            event["thread"],
            event["start"],
        )
        in keys
    ]


def read_saved_state(userdir):
    saves = list((userdir / "save" / "TUTORIAL").glob("*.sav"))
    if len(saves) != 1:
        raise ValueError(f"Expected one tutorial character save, found {len(saves)}")
    text = saves[0].read_text()
    data = json.loads(text[text.index("{") :])
    player = data["player"]
    if player["name"] != "John Smith":
        raise ValueError("Saved character is not the tutorial character")
    return {
        "turn": data["turn"],
        "position": player["abs_pos"],
        "moves": player["moves"],
        "name": player["name"],
    }


class Terminal:
    """Keep the terminal screen and raw bytes as evidence of actual UI input."""

    def __init__(self, command, *, output, port=None):
        self.output = output
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 0, 0))
        self.process = subprocess.Popen(
            command,
            stdin=slave,
            stdout=slave,
            stderr=slave,
            cwd=ROOT,
            env={
                **os.environ,
                "TERM": "xterm-256color",
                "LC_ALL": "C.UTF-8",
                "SDL_VIDEODRIVER": "offscreen",
                **({"TRACY_PORT": str(port)} if port else {}),
            },
            start_new_session=True,
        )
        os.close(slave)
        self.screen = pyte.Screen(120, 40)
        self.stream = pyte.Stream(self.screen)
        self.decoder = codecs.getincrementaldecoder("utf-8")("replace")
        self.raw = (output / "terminal.ansi").open("wb")
        self.inputs = []

    @property
    def text(self):
        return "\n".join(self.screen.display)

    def read(self, timeout=0.05):
        if not select.select([self.master], [], [], timeout)[0]:
            return False
        try:
            data = os.read(self.master, 65536)
        except OSError:
            data = b""
        if not data:
            raise RuntimeError(f"Game exited: {self.process.poll()}")
        self.raw.write(data)
        self.raw.flush()
        self.stream.feed(self.decoder.decode(data))
        return True

    def expect(self, predicate, *, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate(self.text):
                return
            self.read()
        self.snapshot("failure")
        raise TimeoutError(
            f"UI expectation timed out; see {self.output / 'failure.txt'}"
        )

    def settle(self, *, timeout=30):
        """Drain output until the main thread blocks reading the terminal again."""
        read_syscall = {"x86_64": 0, "aarch64": 63}.get(platform.machine())
        if read_syscall is None:
            raise RuntimeError("Input readiness requires Linux x86_64 or aarch64")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if not self.read(0.01):
                syscall = Path(f"/proc/{self.process.pid}/syscall").read_text().split()
                if (
                    len(syscall) > 2
                    and syscall[0] == str(read_syscall)
                    and syscall[1] == "0x0"
                ):
                    return
        self.snapshot("not-input-ready")
        raise TimeoutError("Game did not return to terminal input readiness")

    def send(self, keys):
        self.inputs.append({"keys": keys, "monotonic_ns": time.monotonic_ns()})
        os.write(self.master, keys.encode())

    def snapshot(self, name):
        (self.output / f"{name}.txt").write_text(self.text + "\n")

    def dismiss_lessons(self):
        for _ in range(len(LESSONS)):
            lesson = next(
                (lesson for lesson in LESSONS if lesson["text"][:40] in self.text), None
            )
            if lesson is None:
                return
            self.snapshot(lesson["id"])
            self.send(" ")
            self.settle()
        raise RuntimeError("Tutorial lessons did not finish")

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.raw.close()
        os.close(self.master)
        (self.output / "inputs.json").write_text(
            json.dumps(self.inputs, indent=2) + "\n"
        )


def play(args, *, output):
    userdir = output / "user"
    config = userdir / "config"
    config.mkdir(parents=True)
    (config / "options.json").write_text(
        json.dumps(
            [
                {"name": name, "value": value}
                for name, value in {
                    **OPTIONS,
                    "COMPUTE_ACCELERATION": "gpu_software"
                    if args.acceleration == "software"
                    else args.acceleration,
                }.items()
            ]
        )
    )
    (config / "preload.json").write_text(
        json.dumps({"compute_acceleration": args.acceleration})
    )
    base_command = [
        str(args.binary),
        "--client=curses",
        "--basepath",
        str(ROOT) + "/",
    ]
    verify_profile_paths(base_command, userdir)
    command = [
        *base_command,
        *profile_arguments(userdir),
        "--seed",
        args.seed,
    ]
    terminal = Terminal(command, output=output, port=args.port)
    capture = None
    with (output / "capture.log").open("w") as capture_log:
        try:
            capture = subprocess.Popen(
                [
                    "stdbuf",
                    "-oL",
                    str(args.capture),
                    "-a",
                    "127.0.0.1",
                    "-o",
                    str(output / "capture.tracy"),
                    "-p",
                    str(args.port),
                ],
                stdout=capture_log,
                stderr=subprocess.STDOUT,
            )
            terminal.expect(lambda text: "New Game" in text or "New game" in text)
            terminal.settle()
            terminal.snapshot("main-menu")
            deadline = time.monotonic() + 30
            while "Timer resolution:" not in (output / "capture.log").read_text():
                if capture.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Tracy did not connect; see capture.log")
                terminal.read()
            terminal.send("n")
            terminal.expect(lambda text: "Tutorial" in text)
            # 't' also selects the top-level Settings tab. Navigate within
            # New Game instead of relying on that conflicting shortcut.
            for _ in range(7):
                if re.search(r"»\s*Tutorial", terminal.text):
                    break
                terminal.send("\x1bOB")
                terminal.settle()
            else:
                raise RuntimeError("Could not select Tutorial in New Game menu")
            terminal.snapshot("tutorial-selected")
            terminal.send("\n")
            terminal.expect(lambda text: re.search(r"Place:\s*tutorial room", text))
            terminal.settle()
            terminal.send(".")
            terminal.expect(lambda text: "Welcome to the Cataclysm tutorial!" in text)
            for lesson in ("The '@' character", "To see what the symbols"):
                terminal.send(" ")
                terminal.expect(lambda text: lesson in text)
            terminal.send(" ")
            terminal.settle()
            terminal.dismiss_lessons()
            terminal.expect(lambda text: re.search(r"Place:\s*tutorial room", text))
            terminal.snapshot("tutorial-start")
            for _ in range(args.warmup):
                terminal.send(".")
                terminal.settle()
                terminal.dismiss_lessons()
            terminal.snapshot("warmup-end")
            for key in "." * args.waits + "lh" * args.cycles:
                terminal.send(key)
                terminal.settle()
                terminal.dismiss_lessons()
                if not re.search(r"Place:\s*tutorial room", terminal.text):
                    terminal.snapshot("unexpected-ui")
                    raise RuntimeError("Gameplay interrupted; see unexpected-ui.txt")
            terminal.snapshot("scenario-end")
            # Ctrl-G is unbound in DEFAULTMODE. Complete the final blocking-input
            # zone without adding a gameplay action, then disconnect.
            terminal.send("\x07")
            terminal.settle()
            # Tracy transfers and processes events asynchronously. Keep the
            # client alive after the final input scope closes; this interval
            # is outside the selected gameplay window.
            drain_until = time.monotonic() + 0.5
            while time.monotonic() < drain_until:
                terminal.read(0.05)
            capture.send_signal(signal.SIGINT)
            capture.wait(timeout=60)
            if capture.returncode != 0 or not (output / "capture.tracy").is_file():
                raise RuntimeError("Tracy capture failed; see capture.log")
            terminal.send("S")
            terminal.expect(lambda text: "You're saving a tutorial" in text)
            terminal.send(" ")
            terminal.expect(lambda text: "Save and quit?" in text)
            terminal.send("Y")
            terminal.expect(lambda text: "New Game" in text or "New game" in text)
            terminal.snapshot("saved-main-menu")
            terminal.send("q")
            terminal.expect(lambda text: "Really quit?" in text)
            terminal.send("Y")
            terminal.process.wait(timeout=10)
            # main.cpp finishes through exit_handler(-999), which is status 25
            # on POSIX. Accept it only after observing save and menu exit.
            if terminal.process.returncode not in (0, (-999) & 0xFF):
                raise RuntimeError(f"Game exited with {terminal.process.returncode}")
        finally:
            if capture is not None and capture.poll() is None:
                capture.terminate()
                try:
                    capture.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    capture.kill()
                    capture.wait()
            terminal.close()
    return {"command": command, "game_exit_code": terminal.process.returncode}


def export(args, *, output):
    for name, options in (
        ("inclusive", []),
        ("self", ["--self"]),
        ("events", ["--unwrap"]),
        ("self-events", ["--unwrap", "--self"]),
    ):
        with (output / f"{name}.csv").open("w") as destination:
            subprocess.run(
                [
                    str(args.exporter),
                    "-s",
                    ";",
                    *options,
                    str(output / "capture.tracy"),
                ],
                stdout=destination,
                check=True,
                timeout=120,
            )
    actions = args.waits + 2 * args.cycles
    inclusive_events = load_events(output / "events.csv")
    observed_actions = sum(
        e["name"] == "handle_action_alive_switch" for e in inclusive_events
    )
    if observed_actions != 1 + args.warmup + actions:
        raise ValueError(
            f"Expected {1 + args.warmup + actions} total gameplay actions, "
            f"found {observed_actions}"
        )
    inclusive = summarize(inclusive_events, actions=actions)
    # The exporter's self duration does not identify the original interval end.
    # Use inclusive event boundaries to select the matching self-time samples.
    bounds = inclusive["start_ns"], inclusive["end_ns"]
    self_events = nonwaiting_self_events(
        inclusive_events, load_events(output / "self-events.csv"), window=inclusive
    )
    self_zones = zone_statistics(self_events)
    actual_movements = sum(
        e["name"] == "handle_action_movement"
        for e in inclusive_events
        if bounds[0] <= e["start"] < bounds[1] and e["thread"] == inclusive["thread"]
    )
    if actual_movements != 2 * args.cycles:
        raise ValueError(
            f"Expected {2 * args.cycles} movements, found {actual_movements}"
        )
    placements = sum(
        e["name"] == "walk_move_place_player"
        for e in inclusive_events
        if bounds[0] <= e["start"] < bounds[1] and e["thread"] == inclusive["thread"]
    )
    if placements != actual_movements:
        raise ValueError(
            f"Only {placements}/{actual_movements} movement actions changed position"
        )
    return {
        "inclusive": inclusive,
        "self_zones_excluding_input": self_zones,
        "movement_actions": actual_movements,
        "successful_movements": placements,
        "saved_state": read_saved_state(output / "user"),
        "compute_backend": re.findall(
            r"Compute backend selected: ([^;\n]+)",
            (output / "user/config/debug.log").read_text(),
        )[-1].strip(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary", type=Path, default=ROOT / "out/build/linux-curses/src/cataclysm-bn"
    )
    parser.add_argument(
        "--capture", type=Path, default=ROOT / "out/tools/tracy-capture/tracy-capture"
    )
    parser.add_argument(
        "--exporter",
        type=Path,
        default=ROOT / "out/tools/tracy-csvexport/tracy-csvexport",
    )
    parser.add_argument(
        "--output", type=Path, required=True, help="New directory for this run"
    )
    parser.add_argument("--seed", default="tutorial-tracy-1")
    parser.add_argument("--warmup", type=int, default=20)
    parser.add_argument("--waits", type=int, default=100)
    parser.add_argument("--cycles", type=int, default=20)
    parser.add_argument(
        "--acceleration",
        choices=("auto", "cpu", "gpu", "software"),
        default="auto",
        help="Compute acceleration policy (default: game auto selection)",
    )
    parser.add_argument(
        "--port", type=int, help="Tracy port (default: an available local port)"
    )
    args = parser.parse_args()
    if args.port is None:
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            args.port = reservation.getsockname()[1]
    if (
        args.warmup < 0
        or args.waits < 0
        or args.cycles < 0
        or args.waits + args.cycles == 0
    ):
        parser.error(
            "Action counts must be nonnegative and the measured workload nonempty"
        )
    for name in ("binary", "capture", "exporter"):
        path = getattr(args, name).resolve(strict=True)
        setattr(args, name, path)
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    metadata = {
        "revision": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "platform": platform.platform(),
        "cpu": platform.processor(),
        "tracy_revision": TRACY_REVISION,
        "binary_sha256": hashlib.file_digest(
            args.binary.open("rb"), "sha256"
        ).hexdigest(),
        "seed": args.seed,
        "warmup": args.warmup,
        "waits": args.waits,
        "movement_cycles": args.cycles,
        "compute_acceleration": args.acceleration,
        "terminal": {"columns": 120, "rows": 40},
        "options": OPTIONS,
        "tracy_port": args.port,
    }
    (args.output / "CMakeCache.txt").write_text(
        (args.binary.parent.parent / "CMakeCache.txt").read_text()
    )
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    metadata.update(play(args, output=args.output))
    loaded_options = {
        entry["name"]: entry["value"]
        for entry in json.loads((args.output / "user/config/options.json").read_text())
    }
    expected_acceleration = (
        "gpu_software" if args.acceleration == "software" else args.acceleration
    )
    if loaded_options["COMPUTE_ACCELERATION"] != expected_acceleration:
        raise RuntimeError("Requested compute policy was not applied")
    summary = export(args, output=args.output)
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(args.output),
                "actions": summary["inclusive"]["actions"],
                "top_self_zones": summary["self_zones_excluding_input"][:8],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
