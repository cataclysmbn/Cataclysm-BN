#!/usr/bin/env python3
"""Probe loaded Help topics and the native reader through MCP, then replay without a host."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from replay_smoke import ROOT, Session, profile_arguments, verify_profile_paths


def expect_rejection(session, snapshot, **arguments):
    result = session.request("tools/call", {
        "name": "bn.interact",
        "arguments": {"input_id": snapshot["input_id"], **arguments},
    })
    if not result.get("isError"):
        raise RuntimeError(f"Expected semantic rejection, received {result}")
    unchanged = session.interaction()
    if unchanged["input_id"] != snapshot["input_id"] or unchanged["schema_id"] != snapshot["schema_id"]:
        raise RuntimeError("Rejected command changed the native input boundary or schema")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "out/build/help-text/src/cataclysm-bn-tiles")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--playback-client", choices=("mcp", "tiles"), default="mcp")
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    command = [str(binary), "--basepath", str(ROOT) + "/", "--seed", "help-text-smoke-37"]
    profiles = {name: output / (name + "-profile") for name in ("record", "play")}
    options = {"USE_LANG": "en_US", "COMPUTE_ACCELERATION": "cpu"}
    for name, profile in profiles.items():
        (profile / "config").mkdir(parents=True)
        (profile / "config/options.json").write_text(json.dumps([
            {"name": key, "value": value} for key, value in options.items()
        ]))
        (profile / "config/preload.json").write_text(json.dumps({"compute_acceleration": "cpu"}))
        client = "mcp" if name == "record" else args.playback_client
        verify_profile_paths([*command, "--client=" + client], profile, output / (name + "-paths.txt"))

    record_output = output / "record"
    record_output.mkdir()
    replay = output / "help-input.jsonl"
    session = Session([
        *command, "--client=mcp", *profile_arguments(profiles["record"]),
        "--replay-record", str(replay),
    ], record_output, timeout=300)
    try:
        root = session.interaction()
        while len(root["choices"]) == 1:
            root = session.acknowledge(root)
        if root["context"] != "MAIN_MENU":
            raise RuntimeError(f"Expected native startup menu, received {root}")
        state_before = session.tool("bn.state")
        menu = session.choose_label(root, "Help")
        assert menu["kind"] == "choices" and menu["structured"] and menu["allow_cancel"], menu
        (output / "help-menu.json").write_text(json.dumps(menu, ensure_ascii=False, indent=2))
        rejections = [expect_rejection(session, menu, operation="choose", choice_id="unknown-help-topic")]
        rejections.append(expect_rejection(
            session, menu, input_id=menu["input_id"] - 1, operation="cancel"))
        matches = [choice for choice in menu["choices"] if "Movement" in choice["label"]]
        if len(matches) != 1:
            raise RuntimeError(f"Expected one live Movement topic, received {matches}")
        reader = session.interact(menu, "choose", choice_id=matches[0]["id"])
        assert reader["context"] == "SCROLLABLE_TEXT" and reader["kind"] == "custom", reader
        assert reader["structured"] and reader["allow_cancel"] and not reader["choices"], reader
        assert not reader["allow_set_count"] and "<press_" not in reader["message"], reader
        assert "Movement is performed" in reader["message"], reader
        (output / "reader.json").write_text(json.dumps(reader, ensure_ascii=False, indent=2))
        rejections.append(expect_rejection(session, reader, operation="choose", choice_id=matches[0]["id"]))
        actions = session.tool("bn.actions")
        assert "PAGE_DOWN" in {entry["id"] for entry in actions["actions"]}, actions
        before = session.tool("bn.observe")
        session.press(action="PAGE_DOWN")
        after = session.tool("bn.observe")
        assert before["rows"] != after["rows"], "Native paging did not change the visible viewport"
        (output / "native-pages.json").write_text(json.dumps({"before": before, "after": after}, ensure_ascii=False, indent=2))
        paged = session.interaction()
        assert paged["schema_id"] == reader["schema_id"] and paged["message"] == reader["message"], paged
        restored = session.interact(paged, "cancel")
        assert restored["schema_id"] == menu["schema_id"] and restored["choices"] == menu["choices"], restored
        root = session.interact(restored, "cancel")
        assert root["context"] == "MAIN_MENU", root
        state_after = session.tool("bn.state")
        assert state_before == state_after, "Read-only startup Help changed the observable game state"
        (output / "rejections.json").write_text(json.dumps(rejections, ensure_ascii=False, indent=2))
        confirmation = session.interact(root, "cancel")
        yes = [choice for choice in confirmation["choices"] if choice["description"] == "YES"]
        assert len(yes) == 1, confirmation
        session.interact_and_wait_for_exit(confirmation, "choose", choice_id=yes[0]["id"])
        assert session.process.returncode == 0, session.process.returncode
    finally:
        session.close()

    records = [json.loads(line) for line in replay.read_text().splitlines()]
    events = [record for record in records if record["kind"] == "input"]
    # Record type is keyboard/interaction; record kind identifies an input record.
    expected_contexts = ["MAIN_MENU", "default", "SCROLLABLE_TEXT", "SCROLLABLE_TEXT", "default", "MAIN_MENU", "YESNO"]
    assert [event["boundary"]["context"] for event in events] == expected_contexts, events
    assert records[-1]["kind"] == "end" and records[-1]["events"] == len(events), records[-1]
    with (output / "play.stderr").open("wb") as stderr:
        result = subprocess.run([
            *command, "--client=" + args.playback_client, *profile_arguments(profiles["play"]),
            "--replay-play", str(replay),
        ], cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=stderr, timeout=300)
    (output / "play.stdout").write_bytes(result.stdout)
    assert result.returncode == 0, f"Playback failed: {result.returncode}; see play.stderr"
    summary = {
        "binary": str(binary), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "record_exit": session.process.returncode, "play_exit": result.returncode,
        "playback_client": args.playback_client, "choice_label": matches[0]["label"],
        "reader_chars": len(reader["message"]), "topic_count": menu["choice_page"]["total"],
        "native_viewport_changed": before["rows"] != after["rows"],
        "input_events": len(events), "contexts": expected_contexts,
        "state_unchanged": state_before == state_after, "output": str(output),
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
