# Tests

## General rule

Every code subtask ships with tests in the same commit unless the subtask is pure loader scaffolding that cannot be tested yet. If a subtask cannot be tested immediately, the next commit must add the tests before more feature work stacks on top.

## Unit tests

Cover these first:

- schema load
- terse key parse
- fact normalization
- query match by tag / flag / mat / itype
- fast formula evaluation
- fingerprint stability
- proc payload save/load
- stack key equality / inequality
- nested compact child summaries

## Builder tests

- candidate index by role
- slot min / max validation
- add/remove updates fast preview correctly
- builder keeps fact indexes instead of copying live items
- builder does not create recipe variants
- search finds proc builders by role tag

## Food tests

- 2 breads + fillings computes expected fast preview
- finalize Lua fold returns expected compiled blob
- name override works
- allergens union correctly
- food stacks by `fp + rot bucket`
- `none` food exact-uncraft is refused or falls back as designed

## Recoverable item tests

- compact spear uncraft restores child durability buckets
- nested proc child survives compact save/load
- compact and full do not stack together
- damage routed to child summaries changes restored parts

## Gun/tool regression tests

- existing gunmods still affect stats after proc core integration
- mags and ammo still serialize correctly
- toolmod compatibility is unchanged for non-proc tools

## UI behavior tests

- craft list count stays flat for proc builders
- builder open cost is bounded with large inventory fixtures
- no Lua call on pure cursor moves
- finalize calls Lua once per result
- malformed Lua output is rejected or normalized safely

## Suggested commit-local test slices

Match test files to feature slices:

1. loader tests
2. fact normalize tests
3. save/load tests
4. builder index tests
5. fast preview tests
6. food finalize tests
7. compact restore tests
8. regression tests
