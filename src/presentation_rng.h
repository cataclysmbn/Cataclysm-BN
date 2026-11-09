#pragma once

/// rng() for randomness that only changes what is drawn or played, such as rain drops or a sound
/// variant. It never draws from the game's engine, so how often or how large the screen is drawn
/// cannot change the game or a replay.
auto presentation_rng( int lo, int hi ) -> int;
