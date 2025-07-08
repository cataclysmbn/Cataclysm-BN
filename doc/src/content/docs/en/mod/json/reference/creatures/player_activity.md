---
title: Activities
---

Activities are long term actions, that can be interrupted and (optionally) continued. This allows
the avatar or an npc to react to events (like monsters appearing, getting hurt) while doing
something that takes more than just one turn.

## Adding new activities

1. `player_activities.json` Define properties that apply to all instances of the activity in
   question.

2. `activity_actor.h` Create a new subclass of `activity_actor` (e.g. move_items_activity_actor) to
   store state and handle turns of the new activity.

3. `activity_actor.cpp` Define the `start`, `do_turn`, and `finish` functions needed for the new
   actor as well as the required serialization functions. Don't forget to add the deserialization
   function of your new activity actor to the `deserialize_functions` map towards the bottom of
   `activity_actor.cpp`. Define `canceled` function if activity modifies some complex state that
   should be restored upon cancellation / interruption.

4. If this activity is resumable, `override` `activity_actor::can_resume_with_internal`

5. Construct your activity actor and then pass it to the constructor for `player_activity`. The
   newly constructed activity can then be assigned to the character and started using
   `Character::assign_activity`.

## JSON Properties

- verb: A descriptive term to describe the activity to be used in the query to stop the activity,
  and strings that describe it, for example: `"verb": "mining"` or
  `"verb": { "ctxt": "instrument", "str": "playing" }`.

- suspendable (true): If true, the activity can be continued without starting from scratch again.
  This is only possible if `can_resume_with()` returns true.

- rooted (false): If true, then during the activity, recoil is reduced, and plant mutants sink their
  roots into the ground. Should be true if the activity lasts longer than a few minutes, and can
  always be accomplished without moving your feet.

- special (false): activity is considered special and expects to have unconventional logic,
  compared to other activities

- complex_moves(false):
  - if false - activity expects to have no speed calculations and do 100 moves per turn,
    in JSON it's specified by absence of `complex_moves` block;
  - if true - activity expects to have complex speed/moves calculations, based on several factors:
    - max_assistants(0): activity can be assisted by provided maximum of other creatures(range is 0-32);
    - bench(false): activity can be done using workbench;
    - light(false): activity speed is affected by current light level;
    - speed(false): activity speed is affected by creature's speed;
    - skills: activity speed is affected by skills provided in by pairs `skill_name: modifier`
      or `"skills": true` if you want to explicitely show that activity expects to have modifications
      based on skills, but those will have to be determine on go (like crafting or constructing):
      - `"skills": true`
      - `"skills": ["fabrication", 5]`
    - stats: activity speed is affected by skills provided in by pairs `stat_name: modifier`
      or `"stats": true` if you want to explicitely show that activity expects to have modifications
      based on stats, but those will have to be determine on go (like crafting or constructing):
      - `"stats": true`
      - `"stats": ["DEX", 5]`
    - qualities: activity speed is affected by qualities provided in by pairs `q_name: modifier`
      or `"qualities": true` if you want to explicitely show that activity expects to have modifications
      based on qualities, but those will have to be determine on go (like crafting or constructing):
      - `"qualities": true`
      - `"qualities": ["CUT_FINE", 5]`
    - morale(false): activity speed is affected by creature's current morale level.

    Example for whole block:
    "complex_moves": {
    "max_assistants": 2,
    "bench": true,
    "light": true,
    "speed": true,
    "stats": true,
    "skills": [ ], - //same as `"skills": true`
    "qualities": [ ["CUT_FINE", "5"] ],
    "morale": true
    }

- morale_blocked(false): activity won't be performed if creature's morale level is below certain level.

- verbose_tooltip(true): activity will have an expanded progress window, showing a lot of information

- no_resume (false): Rather than resuming, you must always restart the activity from scratch.

- multi_activity(false): This activity will repeat until it cannot do any more work, used for NPC
  and avatar zone activities.

- refuel_fires( false ): If true, the character will automatically refuel fires during the long
  activity.

- auto_needs( false ) : If true, the character will automatically eat and drink from specific
  auto_consume zones during long activities.

## Termination

There are several ways an activity can be ended:

1. Call `player_activity::set_to_null()`

   This can be called if it finished early or something went wrong, such as corrupt data,
   disappearing items, etc. The activity will not be put into the backlog.

2. `moves_left` <= 0

   Once this condition is true, the finish function, if there is one, is called. The finish function
   should call `set_to_null()`. If there isn't a finish function, `set_to_null()` will be called for
   you (from activity_actor::do_turn).

3. `progress.complete()`

   Basically the same as `moves_left` <= 0, but with extra checks and using a progress system.

4. `Character::cancel_activity`

   Canceling an activity prevents the `activity_actor::finish` function from running, and the
   activity does therefore not yield a result. Instead, `activity_actor::canceled` is called. If
   activity is suspendable, a copy of it is written to `Character::backlog`.

## Progress

`progress_counter` - class specialize on tracking progress of and activity

- targets: queue of targets that are expected to be processed, stores target name, moves_total and
  moves_left for the target;

- moves_total (0): Total number of moves required to complete the activity aka all the tasks;

- moves_left (): The number of moves remaining in this activity before it is complete aka all the tasks;

- idx (1): 1-based index of currently prcessing task;

- total_tasks (0): Counts total amount of tasks - done and in queue.

## Notes

While the character performs the activity, `activity_actor::do_turn` is called on each turn.
Depending on the JSON properties, this will do some stuff. It will also call the do_turn function,
and if `moves_left` is non-positive, the finish function.

Some activities (like playing music on a mp3 player) don't have an end result but instead do
something each turn (playing music on the mp3 player decreases its batteries and gives a morale
bonus).

If the activity needs any information during its execution or when it's finished (like _where_ to do
something, _which_ item to use to do something, ...), simply add data members to your activity actor
as well as serialization functions for the data so that the activity can persist across save/load
cycles.

Be careful when storing coordinates as the activity may be carried out by NPCS. If its, the
coordinates must be absolute not local as local coordinates are based on the avatars position.

### `activity_actor::start`

This function is called exactly once when the activity is assigned to a character. It is useful for
setting `player_activity::moves_left`/`player_activity::moves_total` in the case of an activity
based on time or speed.

### `activity_actor::do_turn`

To prevent an infinite loop, ensure that one of the following is satisfied:

- The `player_activity::progress.moves_left` is decreased in `do_turn`

- The the activity is stopped in `do_turn` (see 'Termination' above)

For example, `move_items_activity_actor::do_turn` will either move as many items as possible given
the character's current moves or, if there are no target items remaining, terminate the activity.

### `activity_actor::finish`

This function is called when the activity has been completed (`moves_left` <= 0). It must call
`player_activity::set_to_null()` or assign a new activity. One should not call
`Character::cancel_activity` for the finished activity, as this could make a copy of the activity in
`Character::backlog`, which allows resuming an already finished activity.
