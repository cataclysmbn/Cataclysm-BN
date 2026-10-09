#!/usr/bin/env python3
"""Record a real MCP tutorial session and replay it in an identical fresh profile."""

import argparse
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import time

from profile_isolation import ROOT, profile_arguments, verify_profile_paths


class Session:
    """Synchronous stdio MCP client; readiness comes from responses, never sleeps."""

    def __init__(self, command, output, timeout=90):
        self.output = output
        self.timeout = timeout
        self.buffer = b""
        self.request_id = 0
        self.transcript = []
        self.log = (output / "game.stderr").open("wb")
        self.process = subprocess.Popen(
            command, cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log,
        )
        try:
            self.request("initialize", {
                "protocolVersion": "2025-11-25", "capabilities": {},
                "clientInfo": {"name": "bn-replay-smoke", "version": "1"},
            })
            self.send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        except BaseException:
            self.close()
            raise

    def send(self, message):
        self.process.stdin.write((json.dumps(message) + "\n").encode())
        self.process.stdin.flush()

    def request(self, method, params):
        self.request_id += 1
        message = {"jsonrpc": "2.0", "id": self.request_id, "method": method, "params": params}
        start = time.monotonic_ns()
        self.send(message)
        deadline = time.monotonic() + self.timeout
        while True:
            if b"\n" in self.buffer:
                line, self.buffer = self.buffer.split(b"\n", 1)
                response = json.loads(line)
                if response.get("id") != self.request_id:
                    raise RuntimeError(f"Unexpected response id: {response}")
                self.transcript.append({
                    "request": message, "response": response,
                    "elapsed_ns": time.monotonic_ns() - start,
                })
                if "error" in response:
                    raise RuntimeError(f"MCP error: {response['error']}")
                return response["result"]
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.process.stdout], [], [], remaining)[0]:
                raise TimeoutError(f"No response to {method}; see game.stderr")
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                raise RuntimeError(f"Game closed protocol stdout; exit={self.process.poll()}")
            self.buffer += data

    def tool(self, name, arguments=None):
        result = self.request("tools/call", {"name": name, "arguments": arguments or {}})
        if result.get("isError"):
            raise RuntimeError(f"{name} rejected the command: {result}")
        return result["structuredContent"]

    def press(self, *, action=None, key=None, text=None):
        boundary = self.tool("bn.actions")["input_id"]
        if action is not None:
            event = {"action": action}
        elif text is not None:
            event = {"text": text}
        else:
            event = {"key": key}
        event["input_id"] = boundary
        return self.tool("bn.press", {"keys": [event]})

    def interaction(self):
        return self.tool("bn.interaction")

    def interact(self, snapshot, operation, **arguments):
        return self.tool("bn.interact", {
            "input_id": snapshot["input_id"], "operation": operation, **arguments,
        })

    def choose_label(self, snapshot, label):
        matches = [choice for choice in snapshot["choices"] if choice["label"] == label]
        if len(matches) != 1:
            raise RuntimeError(f"Expected one live choice labeled {label!r}, found {matches!r}")
        return self.interact(snapshot, "choose", choice_id=matches[0]["id"])

    def choose_action(self, snapshot, action):
        matches = [choice for choice in snapshot["choices"]
                   if choice["description"] == action]
        if len(matches) != 1:
            raise RuntimeError(f"Expected one live choice for {action!r}, found {matches!r}")
        return self.interact(snapshot, "choose", choice_id=matches[0]["id"])

    def acknowledge(self, snapshot):
        if len(snapshot["choices"]) != 1:
            raise RuntimeError(f"Expected one acknowledgement choice, found {snapshot['choices']!r}")
        return self.interact(snapshot, "choose", choice_id=snapshot["choices"][0]["id"])

    def require_action(self, action):
        actions = self.tool("bn.actions")
        if action not in {entry["id"] for entry in actions["actions"]}:
            raise RuntimeError(f"The active {actions['category']} context has no {action!r} action")
        return action

    def interact_and_wait_for_exit(self, snapshot, operation, **arguments):
        self.request_id += 1
        message = {
            "jsonrpc": "2.0", "id": self.request_id, "method": "tools/call",
            "params": {
                "name": "bn.interact",
                "arguments": {
                    "input_id": snapshot["input_id"], "operation": operation, **arguments,
                },
            },
        }
        start = time.monotonic_ns()
        self.send(message)
        deadline = time.monotonic() + self.timeout
        response = None
        while self.process.poll() is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("Game did not exit after the final semantic confirmation")
            if not select.select([self.process.stdout], [], [], min(remaining, 0.1))[0]:
                continue
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                continue
            self.buffer += data
            if b"\n" in self.buffer:
                line, self.buffer = self.buffer.split(b"\n", 1)
                response = json.loads(line)
                if response.get("id") != self.request_id:
                    raise RuntimeError(f"Unexpected response id: {response}")
        self.transcript.append({
            "request": message, "response": response,
            "elapsed_ns": time.monotonic_ns() - start,
        })

    def close(self):
        if not self.process.stdin.closed:
            self.process.stdin.close()
        try:
            self.process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        finally:
            self.log.close()
            self.process.stdout.close()
            (self.output / "protocol.jsonl").write_text(
                "".join(json.dumps(entry) + "\n" for entry in self.transcript)
            )


def saved_data(profile):
    saves = list((profile / "save/TUTORIAL").glob("*.sav"))
    if len(saves) != 1:
        raise RuntimeError(f"Expected one tutorial save, found {len(saves)}")
    text = saves[0].read_text()
    return json.loads(text[text.index("{"):])


def saved_state(data):
    player = data["player"]
    return {
        "turn": data["turn"], "position": player["abs_pos"],
        "moves": player["moves"], "name": player["name"],
    }


def save_differences(recorded, played, path=""):
    if type(recorded) is not type(played):
        return [{"path": path, "recorded": recorded, "played": played}]
    if isinstance(recorded, dict):
        differences = []
        for key in sorted(recorded.keys() | played.keys()):
            child = f"{path}/{key}"
            if key not in recorded or key not in played:
                differences.append({
                    "path": child, "recorded": recorded.get(key), "played": played.get(key),
                })
            else:
                differences.extend(save_differences(recorded[key], played[key], child))
            if len(differences) >= 20:
                break
        return differences[:20]
    if isinstance(recorded, list):
        differences = []
        for index, (recorded_item, played_item) in enumerate(zip(recorded, played)):
            differences.extend(save_differences(recorded_item, played_item, f"{path}/{index}"))
            if len(differences) >= 20:
                break
        if len(recorded) != len(played) and len(differences) < 20:
            differences.append({
                "path": path + "/length", "recorded": len(recorded), "played": len(played),
            })
        return differences[:20]
    if recorded != played:
        return [{"path": path, "recorded": recorded, "played": played}]
    return []


def canonical_json_sha256(data):
    encoded = json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def record_tutorial(command, output, waits):
    session = Session(command, output)
    fallbacks = []

    def dismiss_lessons(snapshot):
        for _ in range(30):
            if snapshot.get("context") != "POPUP_WAIT":
                return snapshot
            message = snapshot.get("message", "")
            if "can't" in message.lower() or "cannot" in message.lower():
                raise RuntimeError(f"Unexpected denial while dismissing tutorial lessons: {message}")
            if len(snapshot.get("choices", [])) != 1:
                raise RuntimeError(f"Unexpected tutorial popup shape: {snapshot!r}")
            snapshot = session.acknowledge(snapshot)
        raise RuntimeError("Tutorial acknowledgement prompts did not finish")

    def wait_for_activity_completion(label, require_active=True):
        state = session.tool("bn.state")
        activity_ids = []
        idle_polls = 0
        saw_active = False
        for _ in range(2000):
            activity = state["avatar"]["activity"]
            if not activity["active"]:
                if require_active and not saw_active:
                    raise RuntimeError(f"{label} did not enter a native activity")
                actions = session.tool("bn.actions")
                if actions["category"] == "ADVANCED_INVENTORY":
                    session.interact(session.interaction(), "cancel")
                    state = session.tool("bn.state")
                return state, {
                    "activity_ids": activity_ids,
                    "idle_polls": idle_polls,
                }
            saw_active = True
            activity_id = activity["id"]
            if not activity_ids or activity_ids[-1] != activity_id:
                activity_ids.append(activity_id)
            actions = session.tool("bn.actions")
            if actions["timeout_ms"] != 0:
                prompt = session.interaction()
                if prompt.get("context") == "POPUP_WAIT":
                    dismiss_lessons(prompt)
                    state = session.tool("bn.state")
                    continue
                if prompt.get("context") == "ADVANCED_INVENTORY":
                    session.interact(prompt, "cancel")
                    state = session.tool("bn.state")
                    continue
                raise RuntimeError(
                    f"Unexpected blocking prompt during {label}: "
                    f"actions={actions!r}, interaction={prompt!r}")
            if actions["category"] != "DEFAULTMODE":
                raise RuntimeError(
                    f"Unexpected nonblocking context during {label}: {actions!r}")
            session.press(key="IDLE")
            idle_polls += 1
            state = session.tool("bn.state")
        raise RuntimeError(f"{label} exceeded 2000 native activity polls")

    def item_type_counts(state, include_map=True):
        counts = {}

        def add_items(items):
            for entry in items:
                type_id = entry["type_id"]
                counts[type_id] = counts.get(type_id, 0) + entry.get("count", 1)
                add_items(entry.get("contents", []))

        avatar = state["avatar"]
        add_items(avatar["inventory"])
        add_items(avatar["worn"])
        add_items(avatar["wielded"])
        if include_map:
            for tile in state["visible_map"]:
                add_items(tile.get("items", []))
        return counts

    def absolute_tile_position(state, position):
        avatar_position = state["avatar"]["position"]
        avatar_absolute = state["avatar"]["absolute_position"]
        return {
            "x": avatar_absolute["x"] + position["x"] - avatar_position["x"],
            "y": avatar_absolute["y"] + position["y"] - avatar_position["y"],
            "z": avatar_absolute["z"] + position["z"] - avatar_position["z"],
        }

    def item_position(state, type_id):
        matches = [absolute_tile_position(state, tile["position"])
                   for tile in state["visible_map"]
                   if any(item["type_id"] == type_id for item in tile.get("items", []))]
        if len(matches) != 1:
            raise RuntimeError(
                f"Expected one visible tile containing {type_id!r}, found {matches!r}")
        return matches[0]

    directions = (
        ((0, -1), "UP"), ((-1, 0), "LEFT"),
        ((1, 0), "RIGHT"), ((0, 1), "DOWN"),
    )
    pickup_direction_labels = {
        (0, -1): "North", (1, -1): "North East", (1, 0): "East",
        (1, 1): "South East", (0, 1): "South", (-1, 1): "South West",
        (-1, 0): "West", (-1, -1): "North West", (0, 0): "Here",
    }

    def walkable_positions(state):
        result = set()
        for tile in state["visible_map"]:
            furniture = tile.get("furniture", "")
            terrain = tile.get("terrain", "")
            if tile.get("traversable") or "door" in furniture or "door" in terrain:
                position = tile["position"]
                result.add((position["x"], position["y"], position["z"]))
        return result

    def live_paths(state):
        start = state["avatar"]["position"]
        start_key = (start["x"], start["y"], start["z"])
        walkable = walkable_positions(state)
        frontier = deque([start_key])
        previous = {start_key: None}
        while frontier:
            current = frontier.popleft()
            for (dx, dy), _action in directions:
                neighbor = (current[0] + dx, current[1] + dy, current[2])
                if neighbor in walkable and neighbor not in previous:
                    previous[neighbor] = current
                    frontier.append(neighbor)
        return start_key, previous

    known_walkable = set()

    def update_known_walkable(state):
        for position in walkable_positions(state):
            absolute = absolute_tile_position(state, {
                "x": position[0], "y": position[1], "z": position[2],
            })
            known_walkable.add((absolute["x"], absolute["y"], absolute["z"]))

    def navigate_to(target):
        goal = (target["x"], target["y"], target["z"])
        visited_states = set()
        while True:
            state = session.tool("bn.state")
            update_known_walkable(state)
            avatar_absolute = state["avatar"]["absolute_position"]
            start = (avatar_absolute["x"], avatar_absolute["y"], avatar_absolute["z"])
            if start == goal:
                return state
            state_signature = (start, tuple(sorted(known_walkable)))
            if state_signature in visited_states:
                raise RuntimeError(f"Navigation repeated live state before reaching {target}")
            visited_states.add(state_signature)
            frontier = deque([start])
            previous = {start: None}
            previous_action = {}
            while frontier and goal not in previous:
                current = frontier.popleft()
                for (dx, dy), action in directions:
                    neighbor = (current[0] + dx, current[1] + dy, current[2])
                    if neighbor in known_walkable and neighbor not in previous:
                        previous[neighbor] = current
                        previous_action[neighbor] = action
                        frontier.append(neighbor)
            if goal not in previous:
                raise RuntimeError(
                    f"No observed traversable/openable path from {start} to {goal}; "
                    f"remembered {len(known_walkable)} live tiles")
            step = goal
            while previous[step] != start:
                step = previous[step]
            action = previous_action[step]
            session.press(action=session.require_action(action))
            dismiss_lessons(session.interaction())
            moved_state = session.tool("bn.state")
            update_known_walkable(moved_state)
            moved_absolute = moved_state["avatar"]["absolute_position"]
            moved = (moved_absolute["x"], moved_absolute["y"], moved_absolute["z"])
            if moved == start:
                session.press(action=session.require_action(action))
                dismiss_lessons(session.interaction())
                moved_state = session.tool("bn.state")
                update_known_walkable(moved_state)
                moved_absolute = moved_state["avatar"]["absolute_position"]
                moved = (moved_absolute["x"], moved_absolute["y"], moved_absolute["z"])
            if moved != step:
                raise RuntimeError(
                    f"Live movement {action} reached {moved}, expected absolute step {step}")

    def approach_item(target):
        state = session.tool("bn.state")
        update_known_walkable(state)
        avatar = state["avatar"]["absolute_position"]
        start = (avatar["x"], avatar["y"], avatar["z"])
        target_key = (target["x"], target["y"], target["z"])
        frontier = deque([start])
        previous = {start: None}
        while frontier:
            current = frontier.popleft()
            for (dx, dy), _action in directions:
                neighbor = (current[0] + dx, current[1] + dy, current[2])
                if neighbor in known_walkable and neighbor not in previous:
                    previous[neighbor] = current
                    frontier.append(neighbor)
        candidates = [target_key] if target_key in previous else [
            (target_key[0] + dx, target_key[1] + dy, target_key[2])
            for dx, dy in pickup_direction_labels
            if (dx, dy) != (0, 0)
            and (target_key[0] + dx, target_key[1] + dy, target_key[2]) in previous
        ]
        if not candidates:
            raise RuntimeError(f"No observed reachable stance beside item tile {target}")

        def path_length(candidate):
            length = 0
            while candidate != start:
                candidate = previous[candidate]
                length += 1
            return length

        stance = min(candidates, key=lambda candidate: (path_length(candidate), candidate))
        navigate_to({"x": stance[0], "y": stance[1], "z": stance[2]})

    def pickup_at(target):
        state = session.tool("bn.state")
        avatar = state["avatar"]["absolute_position"]
        delta = (target["x"] - avatar["x"], target["y"] - avatar["y"])
        if delta not in pickup_direction_labels:
            raise RuntimeError(f"Item tile {target} is not adjacent to pickup stance {avatar}")
        session.press(action=session.require_action("pickup"))
        snapshot = session.interaction()
        if snapshot.get("message") == "Pickup where?":
            label = pickup_direction_labels[delta]
            choices = [choice for choice in snapshot.get("choices", [])
                       if choice["label"] == label]
            if len(choices) != 1:
                raise RuntimeError(
                    f"Pickup direction did not expose one {label!r} choice: {snapshot!r}")
            snapshot = session.interact(snapshot, "choose", choice_id=choices[0]["id"])
        return snapshot

    def discover_item(type_id):
        explored = set()
        observed = set()
        steps = 0
        while True:
            state = session.tool("bn.state")
            matches = [tile["position"] for tile in state["visible_map"]
                       if any(item["type_id"] == type_id for item in tile.get("items", []))]
            if len(matches) > 1:
                raise RuntimeError(f"Expected one tutorial {type_id!r} tile, found {matches!r}")
            start_key, previous = live_paths(state)
            if len(matches) == 1:
                match = matches[0]
                match_key = (match["x"], match["y"], match["z"])
                adjacent = (
                    (match_key[0] + dx, match_key[1] + dy, match_key[2])
                    for dx, dy in pickup_direction_labels
                )
                if any(position in previous for position in adjacent):
                    return absolute_tile_position(state, match)
            avatar_absolute = state["avatar"]["absolute_position"]
            explored.add((avatar_absolute["x"], avatar_absolute["y"], avatar_absolute["z"]))
            observed.update(
                (position["x"], position["y"], position["z"])
                for position in (
                    absolute_tile_position(state, tile["position"])
                    for tile in state["visible_map"]
                ))
            candidates = [
                position for position in previous
                if tuple(absolute_tile_position(state, {
                    "x": position[0], "y": position[1], "z": position[2],
                }).values()) not in explored
            ]
            if not candidates:
                raise RuntimeError(
                    f"Exhausted {len(explored)} reachable live positions without finding {type_id}")
            destination = min(
                candidates,
                key=lambda position: (
                    abs(position[0] - start_key[0]) + abs(position[1] - start_key[1]),
                    position[1], position[0], position[2],
                ),
            )
            navigate_to(absolute_tile_position(state, {
                "x": destination[0], "y": destination[1], "z": destination[2],
            }))
            steps += 1
            if steps > len(observed) * 2:
                raise RuntimeError(
                    f"Live exploration exceeded its observed-map bound while finding {type_id}")

    try:
        interaction = session.interaction()
        for _ in range(2):
            help_view = session.choose_label(interaction, "Help")
            if (not help_view["structured"] or help_view["actions_only"]
                    or help_view["kind"] != "choices"):
                raise RuntimeError("Semantic Help choice did not expose its live topics")
            movement_topics = [choice for choice in help_view["choices"]
                               if "Movement" in choice["label"]]
            if len(movement_topics) != 1:
                raise RuntimeError("Help did not expose one live Movement topic")
            reader = session.interact(
                help_view, "choose", choice_id=movement_topics[0]["id"])
            if (reader["context"] != "SCROLLABLE_TEXT" or not reader["structured"]
                    or not reader["message"]):
                raise RuntimeError("Help topic did not expose its full native reader text")
            original_text = reader["message"]
            session.press(action=session.require_action("PAGE_DOWN"))
            reader = session.interaction()
            if reader["message"] != original_text:
                raise RuntimeError("Native paging changed the full Help topic text")
            help_view = session.interact(reader, "cancel")
            if not help_view["structured"] or help_view["kind"] != "choices":
                raise RuntimeError("Closing the topic reader did not restore Help choices")
            interaction = session.interact(help_view, "cancel")
        quit_confirmation = session.choose_label(interaction, "Quit")
        if "Really quit?" not in quit_confirmation["message"]:
            raise RuntimeError("Semantic Quit choice did not activate its confirmation")
        interaction = session.choose_action(quit_confirmation, "NO")
        interaction = session.choose_label(interaction, "New Game")
        interaction = session.choose_label(interaction, "Tutorial")
        initial = session.tool("bn.state")
        if not initial["game_ready"]:
            raise RuntimeError("Tutorial did not initialize the game")
        interaction = dismiss_lessons(session.interaction())
        initial_inventory = initial["avatar"]["inventory"]
        if not initial_inventory:
            raise RuntimeError("Tutorial semantic fixture unexpectedly started with empty inventory")
        session.press(action=session.require_action("drop"))
        quantity = session.interaction()
        if quantity["kind"] != "inventory" or not quantity["choices"]:
            raise RuntimeError("Drop selector did not publish nonempty structured inventory entries")
        item_choice = quantity["choices"][0]
        quantity = session.interact(
            quantity, "set_count", choice_id=item_choice["id"], count=0)
        item_choice = next(
            choice for choice in quantity["choices"] if choice["id"] == item_choice["id"])
        quantity = session.interact(
            quantity, "set_count", choice_id=item_choice["id"], count=1)
        selected = next(
            choice for choice in quantity["choices"] if choice["id"] == item_choice["id"])
        if selected["selected_count"] != 1:
            raise RuntimeError("Drop selector did not preserve the requested quantity")
        session.interact(quantity, "cancel")

        session.press(action=session.require_action("throw"))
        picker = session.interaction()
        if picker["kind"] != "inventory" or not picker["choices"]:
            raise RuntimeError("Throw picker did not publish nonempty structured inventory entries")
        targeting = session.interact(
            picker, "choose", choice_id=picker["choices"][0]["id"])
        if targeting["kind"] != "target":
            raise RuntimeError("Throw picker did not enter the structured target UI")
        source = targeting["target"]["source"]
        position = {"x": source["x"] + 1, "y": source["y"], "z": source["z"]}
        targeting = session.interact(targeting, "set_target", position=position)
        if targeting["target"]["cursor"] != position:
            raise RuntimeError("Target UI did not move the cursor to the requested map square")
        interaction = session.interact(targeting, "cancel")

        backpack_position = discover_item("backpack")
        approach_item(backpack_position)
        interaction = pickup_at(backpack_position)
        if interaction.get("context") == "PICKUP":
            backpacks = [choice for choice in interaction["choices"]
                         if any(column["label"] == "Type" and column["value"] == "backpack"
                                for column in choice["columns"])]
            if len(backpacks) != 1:
                raise RuntimeError(f"Ground pickup did not expose one backpack: {backpacks!r}")
            interaction = session.interact(
                interaction, "set_count", choice_id=backpacks[0]["id"], count=1)
            session.press(action=session.require_action("CONFIRM"))
            interaction = session.interaction()
        if (interaction.get("context") == "UILIST"
                and "capacity" in interaction.get("message", "").lower()):
            wear_choices = [choice for choice in interaction.get("choices", [])
                            if "Wear" in choice["label"] and "backpack" in choice["label"]]
            if len(wear_choices) != 1:
                raise RuntimeError(f"Backpack capacity prompt lacked one Wear choice: {interaction!r}")
            interaction = session.interact(
                interaction, "choose", choice_id=wear_choices[0]["id"])
        interaction = dismiss_lessons(interaction)
        after_backpack_pickup, backpack_activity = wait_for_activity_completion(
            "backpack pickup", require_active=False)
        if item_type_counts(after_backpack_pickup, include_map=False).get("backpack", 0) != 1:
            raise RuntimeError("Normal one-item pickup did not acquire the tutorial backpack")
        if not any(item["type_id"] == "backpack"
                   for item in after_backpack_pickup["avatar"]["worn"]):
            session.press(action=session.require_action("wear"))
            wear = session.interaction()
            backpack_choices = [choice for choice in wear["choices"]
                                if re.sub(r"<[^>]+>", "", choice["label"]) == "backpack"]
            if len(backpack_choices) != 1:
                raise RuntimeError(
                    f"Wear selector did not expose one acquired backpack: {backpack_choices!r}")
            interaction = session.interact(
                wear, "choose", choice_id=backpack_choices[0]["id"])
            interaction = dismiss_lessons(interaction)
        worn_state = session.tool("bn.state")
        if not any(item["type_id"] == "backpack" for item in worn_state["avatar"]["worn"]):
            raise RuntimeError("Native wear action did not wear the tutorial backpack")
        pants_position = item_position(worn_state, "pants_cargo")

        machete_position = discover_item("machete")
        approach_item(machete_position)
        ground_pickup = pickup_at(machete_position)
        if ground_pickup["context"] != "PICKUP" or not ground_pickup["structured"]:
            raise RuntimeError("Tutorial rack did not enter semantic ground pickup")
        machetes = [choice for choice in ground_pickup["choices"]
                    if any(column["label"] == "Type" and column["value"] == "machete"
                           for column in choice["columns"])]
        if len(machetes) != 1:
            raise RuntimeError(f"Ground pickup did not expose one machete: {machetes!r}")
        ground_pickup = session.interact(
            ground_pickup, "set_count", choice_id=machetes[0]["id"], count=1)
        selected_machetes = [choice for choice in ground_pickup["choices"]
                             if any(column["label"] == "Type" and column["value"] == "machete"
                                    for column in choice["columns"])]
        if len(selected_machetes) != 1 or selected_machetes[0].get("selected_count") != 1:
            raise RuntimeError("Semantic ground pickup did not select the machete")
        session.press(action=session.require_action("CONFIRM"))
        interaction = dismiss_lessons(session.interaction())
        after_machete, machete_activity = wait_for_activity_completion(
            "machete pickup", require_active=False)
        if item_type_counts(after_machete, include_map=False).get("machete", 0) != 1:
            raise RuntimeError("Native pickup activity did not acquire the tutorial machete")

        approach_item(pants_position)
        before_salvage = session.tool("bn.state")
        session.press(action=session.require_action("butcher"))
        salvage_menu = session.interaction()
        salvage_choices = [
            choice for choice in salvage_menu.get("choices", [])
            if "Salvage" in re.sub(r"<[^>]+>", "", choice["label"])
            and "cargo pants" in re.sub(r"<[^>]+>", "", choice["label"])
        ]
        if len(salvage_choices) != 1:
            raise RuntimeError(
                f"Native butcher menu did not expose 'Salvage cargo pants': {salvage_menu!r}")
        interaction = session.interact(
            salvage_menu, "choose", choice_id=salvage_choices[0]["id"])
        if (interaction.get("structured") and
                any(choice.get("description") == "YES" for choice in interaction.get("choices", []))):
            if "salvage" not in interaction.get("message", "").lower():
                raise RuntimeError(f"Unexpected confirmation after salvage choice: {interaction!r}")
            interaction = session.choose_action(interaction, "YES")
        interaction = dismiss_lessons(interaction)
        after_salvage, salvage_activity = wait_for_activity_completion("cargo-pants salvage")
        rag_yield = (item_type_counts(after_salvage).get("rag", 0)
                     - item_type_counts(before_salvage).get("rag", 0))
        if rag_yield < 2:
            raise RuntimeError(
                f"Native cargo-pants salvage yielded {rag_yield} rags; batch two needs at least 2")

        rag_pickup = pickup_at(pants_position)
        if rag_pickup["context"] != "PICKUP" or not rag_pickup["structured"]:
            raise RuntimeError("Salvaged rags did not enter semantic ground pickup")
        rags = [choice for choice in rag_pickup["choices"]
                if any(column["label"] == "Type" and column["value"] == "rag"
                       for column in choice["columns"])]
        if len(rags) != 1 or rags[0].get("available_count", 0) < 2:
            raise RuntimeError(f"Ground pickup did not expose at least two salvaged rags: {rags!r}")
        rag_pickup = session.interact(
            rag_pickup, "set_count", choice_id=rags[0]["id"],
            count=rags[0]["available_count"])
        session.press(action=session.require_action("CONFIRM"))
        interaction = dismiss_lessons(session.interaction())
        before_bandages, rag_pickup_activity = wait_for_activity_completion(
            "salvaged-rag pickup", require_active=False)
        before_types = item_type_counts(before_bandages)
        before_avatar_types = item_type_counts(before_bandages, include_map=False)
        if before_avatar_types.get("rag", 0) < 2:
            raise RuntimeError("Native pickup activity did not transfer at least two salvaged rags")

        session.press(action=session.require_action("craft"))
        crafting = session.interaction()
        if crafting["context"] != "CRAFTING" or not crafting["structured"]:
            raise RuntimeError("Crafting did not expose its structured recipe model")
        session.press(action=session.require_action("FILTER"))
        field = session.interaction()
        if field["kind"] != "field":
            raise RuntimeError("Crafting filter did not expose the shared text field")
        crafting = session.interact(
            field, "fill", field_id=field["field"]["id"],
            value="makeshift bandage", submit=True)
        bandage_choices = [choice for choice in crafting["choices"]
                           if "makeshift bandage" in re.sub(r"<[^>]+>", "", choice["label"])]
        if len(bandage_choices) != 1:
            raise RuntimeError(
                f"Crafting filter did not expose one makeshift bandage recipe: {bandage_choices!r}")
        crafting = session.interact(
            crafting, "set_count", choice_id=bandage_choices[0]["id"], count=2)
        batch_two = [choice for choice in crafting["choices"]
                     if choice.get("selected_count") == 2 and choice["highlighted"]]
        if len(batch_two) != 1 or not batch_two[0].get("enabled"):
            raise RuntimeError(f"Makeshift bandage batch two was not craftable: {batch_two!r}")
        interaction = session.interact(crafting, "choose", choice_id=batch_two[0]["id"])
        interaction = dismiss_lessons(interaction)
        after_bandages, bandage_activity = wait_for_activity_completion(
            "makeshift-bandage batch")
        after_types = item_type_counts(after_bandages)
        if after_types.get("bandages_makeshift", 0) < before_types.get("bandages_makeshift", 0) + 2:
            raise RuntimeError("Native batch craft did not produce two makeshift bandages")
        if after_types.get("rag", 0) > before_types.get("rag", 0) - 2:
            raise RuntimeError("Native batch craft did not consume two salvaged rags")
        pickup_crafting_evidence = {
            "backpack_position": backpack_position,
            "machete_position": machete_position,
            "pants_position": pants_position,
            "rag_yield": rag_yield,
            "rag_count_before_craft": before_types.get("rag", 0),
            "rag_count_after_craft": after_types.get("rag", 0),
            "bandage_count_before": before_types.get("bandages_makeshift", 0),
            "bandage_count_after": after_types.get("bandages_makeshift", 0),
            "activities": {
                "backpack_pickup": backpack_activity,
                "machete_pickup": machete_activity,
                "salvage": salvage_activity,
                "rag_pickup": rag_pickup_activity,
                "bandage_batch": bandage_activity,
            },
        }
        (output / "pickup-crafting.json").write_text(
            json.dumps(pickup_crafting_evidence, indent=2) + "\n")

        def current_ground_type_count(state, type_id):
            avatar_position = state["avatar"]["position"]
            matches = [tile for tile in state["visible_map"]
                       if tile["position"] == avatar_position]
            if len(matches) != 1:
                raise RuntimeError(
                    f"Expected one visible avatar tile, found {len(matches)}")
            return sum(item.get("count", 1) for item in matches[0].get("items", [])
                       if item["type_id"] == type_id)

        def advanced_inventory_pane(snapshot, role):
            matches = [pane for pane in snapshot.get("panes", []) if pane["role"] == role]
            if len(matches) != 1:
                raise RuntimeError(
                    f"Expected one advanced inventory {role!r} pane: {snapshot!r}")
            return matches[0]

        def advanced_inventory_item(snapshot, role, type_id):
            pane = advanced_inventory_pane(snapshot, role)
            matches = [choice for choice in snapshot.get("choices", [])
                       if choice.get("pane_id") == pane["id"]
                       and any(column["label"] == "Type" and column["value"] == type_id
                               for column in choice["columns"])]
            if len(matches) != 1:
                raise RuntimeError(
                    f"Expected one {type_id!r} item in {role} pane: {matches!r}")
            return matches[0]

        def enter_advanced_inventory():
            session.press(action=session.require_action("advinv"))
            snapshot = session.interaction()
            if (snapshot.get("context") != "ADVANCED_INVENTORY"
                    or not snapshot.get("structured") or len(snapshot.get("panes", [])) != 2):
                raise RuntimeError(
                    f"Advanced inventory did not publish two structured panes: {snapshot!r}")
            return snapshot

        def set_advanced_inventory_source_area(snapshot, action, area_label):
            if advanced_inventory_pane(snapshot, "source")["area_label"] != area_label:
                session.press(action=session.require_action(action))
                snapshot = session.interaction()
            if advanced_inventory_pane(snapshot, "source")["area_label"] != area_label:
                raise RuntimeError(
                    f"Advanced inventory {action} did not select {area_label!r}: {snapshot!r}")
            return snapshot

        before_advanced_inventory = session.tool("bn.state")
        before_inventory_bandages = item_type_counts(
            before_advanced_inventory, include_map=False).get("bandages_makeshift", 0)
        before_ground_bandages = current_ground_type_count(
            before_advanced_inventory, "bandages_makeshift")

        advanced = enter_advanced_inventory()
        advanced = set_advanced_inventory_source_area(
            advanced, "ITEMS_INVENTORY", "Inventory")
        inventory_pane_id = advanced_inventory_pane(advanced, "source")["id"]
        advanced_inventory_item(advanced, "source", "bandages_makeshift")
        session.press(action=session.require_action("TOGGLE_TAB"))
        advanced = session.interaction()
        if advanced_inventory_pane(advanced, "source")["id"] == inventory_pane_id:
            raise RuntimeError("Advanced inventory tab toggle did not change the source pane")
        advanced = set_advanced_inventory_source_area(
            advanced, "ITEMS_CE", "Directly below you")
        center_pane_id = advanced_inventory_pane(advanced, "source")["id"]
        session.press(action=session.require_action("TOGGLE_TAB"))
        advanced = session.interaction()
        if advanced_inventory_pane(advanced, "source")["id"] != inventory_pane_id:
            raise RuntimeError("Advanced inventory did not restore the inventory source pane")
        bandage = advanced_inventory_item(advanced, "source", "bandages_makeshift")
        advanced = session.interact(advanced, "choose", choice_id=bandage["id"])
        focused = advanced_inventory_item(advanced, "source", "bandages_makeshift")
        if not focused["highlighted"]:
            raise RuntimeError("Semantic advanced inventory choice did not focus the bandage")
        session.press(action=session.require_action("MOVE_VARIABLE_ITEM"))
        quantity = session.interaction()
        if quantity.get("kind") != "field" or quantity["field"]["type"] != "integer":
            raise RuntimeError(
                f"Advanced inventory drop did not open its native quantity popup: {quantity!r}")
        session.interact(
            quantity, "fill", field_id=quantity["field"]["id"], value="1", submit=True)
        after_drop, drop_activity = wait_for_activity_completion(
            "advanced-inventory bandage drop")
        dropped_inventory_bandages = item_type_counts(
            after_drop, include_map=False).get("bandages_makeshift", 0)
        dropped_ground_bandages = current_ground_type_count(after_drop, "bandages_makeshift")
        if (dropped_inventory_bandages != before_inventory_bandages - 1
                or dropped_ground_bandages != before_ground_bandages + 1):
            raise RuntimeError(
                "Advanced inventory native drop did not move exactly one bandage: "
                f"inventory {before_inventory_bandages}->{dropped_inventory_bandages}, "
                f"ground {before_ground_bandages}->{dropped_ground_bandages}")

        advanced = enter_advanced_inventory()
        advanced = set_advanced_inventory_source_area(
            advanced, "ITEMS_CE", "Directly below you")
        center_pane_id = advanced_inventory_pane(advanced, "source")["id"]
        ground_bandage = advanced_inventory_item(
            advanced, "source", "bandages_makeshift")
        session.press(action=session.require_action("TOGGLE_TAB"))
        advanced = session.interaction()
        advanced = set_advanced_inventory_source_area(
            advanced, "ITEMS_INVENTORY", "Inventory")
        session.press(action=session.require_action("TOGGLE_TAB"))
        advanced = session.interaction()
        if advanced_inventory_pane(advanced, "source")["id"] != center_pane_id:
            raise RuntimeError("Advanced inventory did not restore the center source pane")
        published_panes = advanced["panes"]
        ground_bandage = advanced_inventory_item(
            advanced, "source", "bandages_makeshift")
        advanced = session.interact(
            advanced, "choose", choice_id=ground_bandage["id"])
        if not advanced_inventory_item(
                advanced, "source", "bandages_makeshift")["highlighted"]:
            raise RuntimeError("Semantic advanced inventory choice did not focus ground bandage")
        session.press(action=session.require_action("MOVE_VARIABLE_ITEM"))
        quantity = session.interaction()
        if quantity.get("kind") != "field" or quantity["field"]["type"] != "integer":
            raise RuntimeError(
                f"Advanced inventory pickup did not open its native quantity popup: {quantity!r}")
        session.interact(
            quantity, "fill", field_id=quantity["field"]["id"], value="1", submit=True)
        after_pickup, pickup_activity = wait_for_activity_completion(
            "advanced-inventory bandage pickup")
        final_inventory_bandages = item_type_counts(
            after_pickup, include_map=False).get("bandages_makeshift", 0)
        final_ground_bandages = current_ground_type_count(
            after_pickup, "bandages_makeshift")
        if (final_inventory_bandages != before_inventory_bandages
                or final_ground_bandages != before_ground_bandages):
            raise RuntimeError(
                "Advanced inventory round trip did not restore bandage counts: "
                f"inventory {before_inventory_bandages}->{final_inventory_bandages}, "
                f"ground {before_ground_bandages}->{final_ground_bandages}")
        advanced_inventory_evidence = {
            "inventory_count_before": before_inventory_bandages,
            "ground_count_before": before_ground_bandages,
            "inventory_count_after_drop": dropped_inventory_bandages,
            "ground_count_after_drop": dropped_ground_bandages,
            "inventory_count_after_pickup": final_inventory_bandages,
            "ground_count_after_pickup": final_ground_bandages,
            "published_panes": published_panes,
            "activities": {"drop": drop_activity, "pickup": pickup_activity},
        }
        (output / "advanced-inventory.json").write_text(
            json.dumps(advanced_inventory_evidence, indent=2) + "\n")

        session.press(action=session.require_action("craft"))
        crafting = session.interaction()
        if crafting["context"] != "CRAFTING" or not crafting["structured"]:
            raise RuntimeError("Crafting did not expose its structured recipe model")

        def crafting_key(snapshot):
            return snapshot["message"], tuple(choice["id"] for choice in snapshot["choices"])

        def next_crafting_tab(snapshot):
            available_actions = {
                entry["id"] for entry in session.tool("bn.actions")["actions"]
            }
            navigation = next(
                (action for action in ("NEXT_TAB", "RIGHT") if action in available_actions), None)
            if navigation is None:
                raise RuntimeError(
                    f"Crafting state {snapshot['message']!r} has no registered tab navigation")
            session.press(action=navigation)
            result = session.interaction()
            if result["context"] != "CRAFTING" or not result["structured"]:
                raise RuntimeError("Crafting tab navigation left the structured selector")
            return result

        visited = set()
        while not crafting["choices"]:
            key = crafting_key(crafting)
            if key in visited:
                raise RuntimeError(
                    "Crafting returned to an already visited live tab without exposing a recipe")
            visited.add(key)
            crafting = next_crafting_tab(crafting)

        leaves = [choice for choice in crafting["choices"]
                  if "selected_count" in choice]
        if not leaves:
            raise RuntimeError("The first nonempty crafting tab exposed only nested categories")
        filter_label = re.sub(r"<[^>]+>", "", leaves[0]["label"])
        session.press(action=session.require_action("FILTER"))
        field = session.interaction()
        if field["kind"] != "field":
            raise RuntimeError("Crafting filter did not expose the shared text field")
        crafting = session.interact(
            field, "fill", field_id=field["field"]["id"], value=filter_label, submit=True)
        if not any(filter_label in re.sub(r"<[^>]+>", "", choice["label"])
                   for choice in crafting["choices"]):
            raise RuntimeError("Crafting filter removed its selected recipe")
        session.press(action=session.require_action("RESET_FILTER"))
        crafting = session.interaction()

        empty_tabs_visited = len(visited)
        visited = set()
        rejected_batches = []
        craftable_batch = None
        while crafting_key(crafting) not in visited:
            visited.add(crafting_key(crafting))
            candidate_ids = [choice["id"] for choice in crafting["choices"]
                             if choice.get("enabled") and "selected_count" in choice]
            for candidate_id in candidate_ids:
                candidate = next(
                    (choice for choice in crafting["choices"] if choice["id"] == candidate_id),
                    None)
                if candidate is None:
                    continue
                candidate_label = re.sub(r"<[^>]+>", "", candidate["label"])
                batch = session.interact(
                    crafting, "set_count", choice_id=candidate["id"], count=2)
                batch_two = [choice for choice in batch["choices"]
                             if choice.get("selected_count") == 2 and choice["highlighted"]]
                if len(batch_two) != 1:
                    raise RuntimeError("Crafting did not enter native batch-two state")
                if batch_two[0]["enabled"]:
                    craftable_batch = (candidate_label, batch, batch_two[0])
                    break
                rejected_batches.append({
                    "tab": crafting["message"], "recipe": candidate_label,
                    "denial": batch_two[0].get("denial", ""),
                })
                session.press(action=session.require_action("CYCLE_BATCH"))
                crafting = session.interaction()
            if craftable_batch is not None:
                break
            crafting = next_crafting_tab(crafting)

        crafting_evidence = {
            "initial_empty_tabs_visited": empty_tabs_visited,
            "filter_label": filter_label,
            "rejected_batches": rejected_batches,
        }
        if craftable_batch is None:
            leaves = [choice for choice in crafting["choices"]
                      if "selected_count" in choice]
            if not leaves:
                raise RuntimeError(
                    "No recipe leaf remained after exhaustive live crafting tab traversal")
            batch = session.interact(
                crafting, "set_count", choice_id=leaves[0]["id"], count=2)
            batch_two = [choice for choice in batch["choices"]
                         if choice.get("selected_count") == 2 and choice["highlighted"]]
            if len(batch_two) != 1:
                raise RuntimeError("Crafting denial probe did not enter batch-two state")
            denial = session.interact(batch, "choose", choice_id=batch_two[0]["id"])
            if "You can't do that" not in denial.get("message", ""):
                raise RuntimeError("Unavailable batch did not follow the native denial popup")
            crafting = session.acknowledge(denial)
            interaction = session.interact(crafting, "cancel")
            reason = (
                "No tutorial recipe exposed a craftable batch of 2 after a cycle of live tabs; "
                f"tested {len(rejected_batches)} enabled recipe rows and verified native "
                "batch-two denial and cancel")
            fallbacks.append(reason)
            crafting_evidence.update({"completed": False, "blocker": reason})
        else:
            recipe_label, batch, batch_two = craftable_batch
            before_craft = session.tool("bn.state")
            interaction = session.interact(batch, "choose", choice_id=batch_two["id"])
            interaction = dismiss_lessons(interaction)
            after_craft, craft_activity = wait_for_activity_completion(
                f"{recipe_label} batch")

            def item_counts(state):
                counts = {}

                def add_items(items):
                    for entry in items:
                        name = re.sub(r"<[^>]+>", "", entry["name"])
                        counts[name] = counts.get(name, 0) + entry.get("count", 1)
                        add_items(entry.get("contents", []))

                avatar = state["avatar"]
                add_items(avatar["inventory"])
                add_items(avatar["worn"])
                add_items(avatar["wielded"])
                for tile in state["visible_map"]:
                    add_items(tile.get("items", []))
                return counts

            before_items = item_counts(before_craft)
            after_items = item_counts(after_craft)
            result_before = before_items.get(recipe_label, 0)
            result_after = after_items.get(recipe_label, 0)
            consumed = sorted(
                name for name, count in before_items.items()
                if name != recipe_label and after_items.get(name, 0) < count)
            if result_after < result_before + 2 or not consumed:
                raise RuntimeError(
                    "Craftable tutorial batch did not expose completed results and component "
                    f"consumption: recipe={recipe_label!r}, before={before_items!r}, "
                    f"after={after_items!r}")
            crafting_evidence.update({
                "completed": True,
                "recipe": recipe_label,
                "result_count_before": result_before,
                "result_count_after": result_after,
                "consumed_items": consumed,
                "activity": craft_activity,
            })
        (output / "crafting.json").write_text(json.dumps(crafting_evidence, indent=2) + "\n")

        session.press(action=session.require_action("smash"))
        direction = session.interaction()
        if direction["context"] != "DEFAULTMODE" or not direction["structured"]:
            raise RuntimeError("Normal smash action did not expose common direction choices")
        here = [choice for choice in direction["choices"] if choice["label"] == "Here"]
        if len(here) != 1:
            raise RuntimeError("Common direction choices did not contain exactly one Here vector")
        interaction = session.interact(direction, "choose", choice_id=here[0]["id"])
        interaction = dismiss_lessons(interaction)

        session.press(action=session.require_action("construct"))
        construction = session.interaction()
        if construction["context"] != "CONSTRUCTION" or not construction["structured"]:
            raise RuntimeError("Construction did not expose its structured project model")

        def construction_key(snapshot):
            return snapshot["message"], tuple(choice["id"] for choice in snapshot["choices"])

        construction_tabs = set()
        while not construction["choices"]:
            key = construction_key(construction)
            if key in construction_tabs:
                raise RuntimeError(
                    "Construction returned to an already visited tab without exposing a project")
            construction_tabs.add(key)
            session.press(action=session.require_action("RIGHT"))
            construction = session.interaction()
            if construction["context"] != "CONSTRUCTION" or not construction["structured"]:
                raise RuntimeError("Construction tab navigation left the structured selector")

        selected_project = construction["choices"][0]
        project_label = re.sub(r"<[^>]+>", "", selected_project["label"])
        session.press(action=session.require_action("FILTER"))
        field = session.interaction()
        if field["kind"] != "field":
            raise RuntimeError("Construction filter did not expose the shared text field")
        construction = session.interact(
            field, "fill", field_id=field["field"]["id"], value=project_label, submit=True)
        matches = [choice for choice in construction["choices"]
                   if re.sub(r"<[^>]+>", "", choice["label"]) == project_label]
        if not matches:
            raise RuntimeError("Construction filter removed its selected live project")
        selected_project = matches[0]
        construction_result = session.interact(
            construction, "choose", choice_id=selected_project["id"])
        construction_evidence = {
            "initial_empty_tabs_visited": len(construction_tabs),
            "filter_label": project_label,
            "enabled": selected_project["enabled"],
            "denial": selected_project.get("denial", ""),
        }
        if selected_project["enabled"]:
            if (construction_result["context"] != "DEFAULTMODE"
                    or not construction_result["structured"]):
                raise RuntimeError(
                    "Buildable construction did not enter common adjacent direction selection")
            east = [choice for choice in construction_result["choices"]
                    if choice["label"] == "East"]
            if len(east) != 1:
                raise RuntimeError("Construction placement did not expose one East vector")
            interaction = session.interact(
                construction_result, "choose", choice_id=east[0]["id"])
            construction_evidence["result"] = "native placement attempted"
            construction_evidence["next_context"] = interaction["context"]
            interaction = dismiss_lessons(interaction)
        else:
            if "can't build" not in construction_result.get("message", ""):
                raise RuntimeError("Unavailable construction did not follow native denial popup")
            construction = session.acknowledge(construction_result)
            interaction = session.interact(construction, "cancel")
            construction_evidence["result"] = "native denial"
        (output / "construction.json").write_text(
            json.dumps(construction_evidence, indent=2) + "\n")

        fallbacks.append("DEFAULTMODE gameplay actions use bn.actions/bn.press")
        for _ in range(waits):
            interaction = dismiss_lessons(session.interaction())
            session.press(action=session.require_action("pause"))
            interaction = dismiss_lessons(session.interaction())
        start = session.tool("bn.state")
        if waits > 0:
            interaction = dismiss_lessons(session.interaction())
            session.press(action=session.require_action("RIGHT"))
            interaction = dismiss_lessons(session.interaction())
            moved = session.tool("bn.state")
            if moved["avatar"]["absolute_position"] == start["avatar"]["absolute_position"]:
                raise RuntimeError("East movement did not move the avatar")
            session.press(action=session.require_action("LEFT"))
            interaction = dismiss_lessons(session.interaction())
            final = session.tool("bn.state")
            if final["avatar"]["absolute_position"] != start["avatar"]["absolute_position"]:
                raise RuntimeError("West movement did not return to the start")
        else:
            final = start
        interaction = dismiss_lessons(session.interaction())
        session.press(action=session.require_action("inventory"))
        inventory = session.interaction()
        if inventory["kind"] != "inventory" or not inventory["structured"]:
            raise RuntimeError("Inventory did not expose its structured entry model")
        session.press(action=session.require_action("INVENTORY_FILTER"))
        field = session.interaction()
        if field["kind"] != "field":
            raise RuntimeError("Inventory filter did not expose a structured field")
        field = session.interact(
            field, "fill", field_id=field["field"]["id"], value="temporary", submit=False)
        session.interact(field, "cancel")
        session.press(action=session.require_action("INVENTORY_FILTER"))
        field = session.interaction()
        session.interact(
            field, "fill", field_id=field["field"]["id"], value="lighter", submit=True)
        session.press(action=session.require_action("QUIT"))
        session.press(action=session.require_action("save"))
        warning = session.interaction()
        warning = session.acknowledge(warning)
        confirmation = session.choose_action(warning, "YES")
        if session.tool("bn.state")["game_ready"]:
            raise RuntimeError("Saving did not return to the main menu")
        (output / "final-state.json").write_text(json.dumps(final, indent=2) + "\n")
        (output / "fallbacks.json").write_text(json.dumps(fallbacks, indent=2) + "\n")
        quit_confirmation = session.interact(confirmation, "cancel")
        yes = [choice for choice in quit_confirmation["choices"]
               if choice["description"] == "YES"]
        if len(yes) != 1:
            raise RuntimeError("Normal quit confirmation did not expose YES")
        session.interact_and_wait_for_exit(
            quit_confirmation, "choose", choice_id=yes[0]["id"])
    finally:
        session.close()
    if session.process.returncode != 0:
        raise RuntimeError(f"Recording exited with {session.process.returncode}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "out/build/linux-full/src/cataclysm-bn")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--waits", type=int, default=5)
    parser.add_argument("--playback-client", choices=("mcp", "tiles"), default="mcp")
    args = parser.parse_args()
    if args.waits < 0:
        parser.error("--waits must not be negative")
    binary = args.binary.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    initial = output / "initial"
    (initial / "config").mkdir(parents=True)
    options = {"USE_LANG": "en_US", "ANIMATIONS": "false", "AUTOSAVE": "false",
               "COMPUTE_ACCELERATION": "cpu", "TERMINAL_X": "106", "TERMINAL_Y": "58"}
    (initial / "config/options.json").write_text(json.dumps([
        {"name": name, "value": value} for name, value in options.items()
    ]))
    (initial / "config/preload.json").write_text(json.dumps({"compute_acceleration": "cpu"}))
    for name in ("record", "playback"):
        shutil.copytree(initial, output / name / "user")
    base_command = [str(binary), "--basepath", str(ROOT) + "/"]
    replay = output / "inputs.jsonl"
    record_profile = output / "record/user"
    playback_profile = output / "playback/user"
    record_command = base_command + [
        "--client=mcp", *profile_arguments(record_profile), "--seed", "engine-client-replay-1",
        "--replay-record", str(replay),
    ]
    playback_command = base_command + [
        "--client=" + args.playback_client, *profile_arguments(playback_profile),
        "--replay-play", str(replay),
    ]
    verify_profile_paths(
        base_command + ["--client=mcp"], record_profile, output / "record/paths.txt")
    verify_profile_paths(
        base_command + ["--client=" + args.playback_client], playback_profile,
        output / "playback/paths.txt")
    record_tutorial(record_command, output / "record", args.waits)
    with (output / "playback/game.stderr").open("wb") as errors:
        result = subprocess.run(
            playback_command, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=errors, timeout=600,
        )
    if result.returncode != 0:
        raise RuntimeError(f"Playback exited with {result.returncode}; see playback/game.stderr")
    recorded_data = saved_data(output / "record/user")
    played_data = saved_data(output / "playback/user")
    if recorded_data != played_data:
        differences = save_differences(recorded_data, played_data)
        (output / "save-diff.json").write_text(json.dumps(differences, indent=2) + "\n")
        raise RuntimeError(f"Saved-game mismatch; first differences: {differences}")
    recorded = saved_state(recorded_data)
    played = saved_state(played_data)
    summary = {
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "scenario": (
            "tutorial inventory quantity, target, semantic crafting, advanced inventory round trip, common direction, construction, waits, east/west, inventory filter, save/quit, application quit"
            if args.waits > 0 else
            "tutorial inventory quantity, target, semantic crafting, advanced inventory round trip, common direction, construction, inventory filter, save/quit, application quit before first turn"
        ),
        "playback_client": args.playback_client,
        "waits": args.waits, "recorded": recorded, "played": played,
        "pickup_crafting": json.loads((output / "record/pickup-crafting.json").read_text()),
        "advanced_inventory": json.loads((output / "record/advanced-inventory.json").read_text()),
        "crafting": json.loads((output / "record/crafting.json").read_text()),
        "construction": json.loads((output / "record/construction.json").read_text()),
        "record_sha256": hashlib.sha256(replay.read_bytes()).hexdigest(),
        "save_json_sha256": canonical_json_sha256(recorded_data),
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
