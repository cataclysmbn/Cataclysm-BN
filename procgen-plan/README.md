# Procgen Plan

This folder replaces the old monolithic `PLAN.md`.

Goals:

- ship procedural food first
- keep the craft list small
- keep hot UI paths fast
- reuse BN modular item systems where they already fit
- allow `none | compact | full` for every proc category
- keep schema text terse and low-noise
- split implementation into many small commits

Plan order:

1. `00-core.md`
2. `01-rules.md`
3. `02-schema.md`
4. `03-recipe.md`
5. `04-ui.md`
6. `05-lua.md`
7. `06-save.md`
8. `07-food.md`
9. `08-items.md`
10. `09-tests.md`
11. `10-commits.md`

Global defaults:

- food starts first
- no recipe explosion
- builder preview stays incremental and cheap
- finalize can spend more work than preview
- prefer terse keys like `cat`, `res`, `hist`, `slot`, `drv`, `lua`
- keep an item-centric shell with a data-oriented proc core
- do not start implementation until commit order is accepted
