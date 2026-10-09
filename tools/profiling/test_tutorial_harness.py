"""Behavior checks for trace selection and terminal replay evidence."""

import tempfile
from pathlib import Path
import sys
import unittest

from tutorial_harness import (
    INPUT_WAIT,
    Terminal,
    percentile,
    nonwaiting_self_events,
    summarize,
    zone_statistics,
)


def event(name, start, duration, *, thread="main", file="game.cpp", line=1):
    return {
        "name": name,
        "start": start,
        "duration": duration,
        "thread": thread,
        "file": file,
        "line": line,
    }


class TraceTests(unittest.TestCase):
    def test_excludes_input_wait_children_using_inclusive_boundaries(self):
        inclusive = [
            event(INPUT_WAIT, 100, 100),
            event("input_context_get_input_event", 110, 80),
            event("redraw", 210, 30),
        ]
        self_times = [
            event(INPUT_WAIT, 100, 20),
            event("input_context_get_input_event", 110, 80),
            event("redraw", 210, 30),
        ]
        result = nonwaiting_self_events(
            inclusive,
            self_times,
            window={"start_ns": 100, "end_ns": 250, "thread": "main"},
        )
        self.assertEqual([e["name"] for e in result], ["redraw"])

    def test_selects_measured_actions_and_excludes_setup_and_other_threads(self):
        events = [
            event("setup", 0, 1000),
            event("handle_action_alive_switch", 1000, 10),
            event("handle_action_alive_switch", 2000, 10),
            event(INPUT_WAIT, 2020, 800),
            event("process_items", 2030, 500, thread="worker"),
            event("handle_action_alive_switch", 3000, 10),
            event("process_items", 3010, 40),
            event("get_player_input", 3050, 50),
        ]
        result = summarize(events, actions=2)
        self.assertEqual((result["start_ns"], result["end_ns"]), (2000, 3050))
        self.assertAlmostEqual(result["input_wait_ms"], 800 / 1e6)
        self.assertAlmostEqual(result["wall_minus_input_wait_ms"], 250 / 1e6)
        zones = {zone["name"]: zone for zone in result["zones"]}
        self.assertNotIn("setup", zones)
        self.assertEqual(zones["process_items"]["total_ms"], 40 / 1e6)

    def test_rejects_insufficient_actions_and_incomplete_capture(self):
        actions = [event("handle_action_alive_switch", 100, 10)]
        with self.assertRaisesRegex(ValueError, "found only"):
            summarize(actions, actions=2)
        with self.assertRaisesRegex(ValueError, "input readiness"):
            summarize(actions, actions=1)

    def test_does_not_merge_distinct_source_locations(self):
        zones = zone_statistics(
            [event("same", 0, 10, line=1), event("same", 100, 20, line=2)]
        )
        self.assertEqual(len(zones), 2)
        self.assertEqual(zones[0]["line"], 2)

    def test_percentile_preserves_tail_and_handles_single_sample(self):
        self.assertEqual(percentile([7], 0.95), 7)
        self.assertAlmostEqual(percentile([0, 10, 100], 0.95), 91)


class TerminalTests(unittest.TestCase):
    def test_reads_ansi_screen_and_records_real_pty_input(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            terminal = Terminal(
                [
                    sys.executable,
                    "-c",
                    "import time; print('Ready', flush=True); "
                    "value = input(); time.sleep(0.1); print('Accepted:' + value, flush=True); "
                    "input()",
                ],
                output=output,
            )
            try:
                terminal.expect(lambda text: "Ready" in text, timeout=5)
                terminal.send("hello\n")
                terminal.settle(timeout=5)
                terminal.expect(lambda text: "Accepted:hello" in text, timeout=5)
                terminal.snapshot("screen")
            finally:
                terminal.close()
            self.assertIn("Accepted:hello", (output / "screen.txt").read_text())
            self.assertIn("hello", (output / "inputs.json").read_text())
            self.assertGreater((output / "terminal.ansi").stat().st_size, 0)

    def test_timeout_leaves_screen_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            terminal = Terminal([sys.executable, "-c", "input()"], output=output)
            try:
                with self.assertRaises(TimeoutError):
                    terminal.expect(lambda text: "never" in text, timeout=0.1)
                self.assertTrue((output / "failure.txt").exists())
            finally:
                terminal.close()


if __name__ == "__main__":
    unittest.main()
