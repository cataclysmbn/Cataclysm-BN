# new mutation system

At the moment the system only covers untargeted mutations. The targeted ones (alpha mutagen etc.)
still use the old one, with the only difference being that it doesn't force negative mutations 33%
of the time and doesn't support Robust Genetics, which would turn those 33% forced negative
mutations into forced positive ones.

You can use the old system by adding the "Old mutations" mod to a world. This can be safely done to
already generated worlds. Same for removal of this mod.

## Accumulated mutagen

The character passively accumulates inactive "mutagenic potential" over time, which is all turned
into new mutations the moment character mutates uncontrollably for any reason. The potential is then
cleared.

This potential caps at 7.333 potential mutations, which takes 22 days (1 mutation per 3 days).

## Mutagenic flesh

Flesh of mutants contains mutagenic toxins, which will cause uncontrollable mutations over a period
of time if allowed to accumulate sufficiently.

The character loses about 630 kcal of mutant flesh of mutagen per day. If a starved character gorged
on mutant flesh, it would take a little more than 5000 kcal to start mutating.

### Uncontrollable mutations

Once the character starts mutating, all the mutagenic potential is immediately turned into new
mutations and the character will continue mutating for at least 4 days.

The character will gain mutagenic potential at a faster rate and will gain about 2 new mutations by
the end of it, assuming no more mutagenic toxins. Mutagenic toxins will extend the process and may
even speed it up to up to over 1 mutation per day.

## Mutation selection

Overall the mutations picked will trend towards positive, until the character has a set of traits
that is "more good than bad".

Picked mutations trend towards a character with +6 trait score overall. For example, a character
with just Quick (+4) and Fleet Footed (+2) traits will receive positive and negative traits about
equally, while a character with no positive traits and -12 worth of negative ones will be more
likely to get Quick over Fleet Footed and very unlikely to get Junkfood Intolerance.
