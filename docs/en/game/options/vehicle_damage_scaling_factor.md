# Vehicle damage scaling factor

The `Vehicle damage scaling factor` world option (`VEHICLE_DAMAGE`) controls how damaged
pre-spawned vehicles are. It ranges from `0.0` to `4.0` in steps of `0.1`, and defaults to `1.0`.

## When it applies

The option only affects vehicles spawned with a non-mint status, i.e. `status` of `-1` (light
damage, the default) or `1` (disabled). See [Spawning vehicles](/mod/json/reference/vehicles/vehicles_spawning/).
Vehicles spawned with `status: 0` are always intact.

Each vehicle part is rolled independently, so one vehicle usually has a mix of intact, damaged and
broken parts.

## How the roll works

For every part, the game computes two thresholds from the option value (`chance`) and rolls `4d8`:

```cpp
int broken = 8 * chance;  // truncated toward zero
int unhurt = 20 * chance; // truncated toward zero
int roll = dice( 4, 8 );  // 4..32, bell-shaped, mean 18
```

| Condition                | Resulting part HP                                      |
| ------------------------ | ------------------------------------------------------ |
| `roll >= unhurt`         | 100% (intact)                                          |
| `broken < roll < unhurt` | `(roll - broken) / (unhurt - broken)` of durability    |
| `roll <= broken`         | 0% (broken); batteries and fuel tanks are also emptied |

Things to note:

- Raising the option moves both thresholds up, so more of the bell curve falls into the damaged
  and broken ranges.
- A damaged part never keeps 100% HP. Its highest HP is `1 - 1 / (unhurt - broken)`, so HP jumps to
  100% at `roll == unhurt`.
- Values below `0.25` leave `unhurt` at 4 or less. Since the lowest possible roll is 4, no damage
  happens at all.
- At `4.0`, `broken` is 32, which is the highest possible roll. Every part of a damaged vehicle is
  broken.

## Reference values

| Factor | `broken` / `unhurt` | Broken | Damaged | Intact | Expected HP |
| ------ | ------------------- | ------ | ------- | ------ | ----------- |
| 0.0    | 0 / 0               | 0%     | 0%      | 100%   | 100%        |
| 0.5    | 4 / 10              | 0.0%   | 3.1%    | 96.9%  | 99.0%       |
| 0.8    | 6 / 16              | 0.4%   | 29.5%   | 70.1%  | 89.9%       |
| 1.0    | 8 / 20              | 1.7%   | 60.7%   | 37.6%  | 75.0%       |
| 1.5    | 12 / 30             | 12.0%  | 87.6%   | 0.4%   | 34.4%       |
| 2.0    | 16 / 40             | 37.6%  | 62.4%   | 0%     | 12.5%       |
| 3.0    | 24 / 60             | 91.9%  | 8.1%    | 0%     | 0.5%        |
| 4.0    | 32 / 80             | 100%   | 0%      | 0%     | 0%          |

## Interactive chart

Try any other value with the chart below. Each row is one possible `4d8` result, from the highest
roll at the top to the lowest at the bottom. The bar length is the chance of that roll, and its color
is the outcome:

- **Intact**: light green.
- **Damaged**: green, yellow-green, yellow, orange or red, one color per 20% of remaining HP.
- **Broken**: gray, like broken parts in the vehicle interface.

<style>
#vehicle-damage-tool {
  --vds-full: rgb(0 255 0);
  --vds-broken: #8c8c8c;
  --vds-line: currentColor;
  margin: 1.5rem 0;
}
#vehicle-damage-tool .vds-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.75rem;
}
#vehicle-damage-tool .vds-controls input[type="range"] {
  flex: 1 1 14rem;
}
#vehicle-damage-tool .vds-controls input[type="number"] {
  width: 5rem;
}
#vehicle-damage-tool .vds-summary {
  list-style: none;
  padding: 0;
  display: flex;
  flex-wrap: wrap;
  gap: 0.25rem 1.5rem;
}
#vehicle-damage-tool .vds-swatch {
  display: inline-block;
  width: 0.8em;
  height: 0.8em;
  margin-right: 0.35em;
  border-radius: 2px;
  vertical-align: middle;
}
#vehicle-damage-tool .vds-swatch.vds-full { background: var(--vds-full); }
#vehicle-damage-tool .vds-swatch.vds-damaged {
  background: linear-gradient(
    90deg,
    #d7301f 0 20%,
    #f08c1e 20% 40%,
    #e8c21a 40% 60%,
    #9ccb2f 60% 80%,
    #2e8b3e 80%
  );
}
#vehicle-damage-tool .vds-swatch.vds-broken { background: var(--vds-broken); }
#vehicle-damage-tool .vds-chart {
  font-family: monospace;
  font-size: 0.85rem;
}
#vehicle-damage-tool .vds-header,
#vehicle-damage-tool .vds-row {
  display: grid;
  grid-template-columns: 2.5rem 1fr 4.5rem 3.5rem;
  gap: 0.5rem;
  align-items: center;
}
#vehicle-damage-tool .vds-header {
  font-weight: bold;
  border-bottom: 1px solid var(--vds-line);
  margin-bottom: 0.25rem;
}
#vehicle-damage-tool .vds-row {
  min-height: 1.15rem;
}
#vehicle-damage-tool .vds-roll,
#vehicle-damage-tool .vds-prob,
#vehicle-damage-tool .vds-hp {
  text-align: right;
}
#vehicle-damage-tool .vds-bar {
  display: block;
  height: 0.8rem;
  border-radius: 2px;
}
#vehicle-damage-tool .vds-row.vds-full .vds-bar { background: var(--vds-full); }
#vehicle-damage-tool .vds-row.vds-broken .vds-bar { background: var(--vds-broken); }
</style>
<div id="vehicle-damage-tool">
  <div class="vds-controls">
    <label for="vehicle-damage-slider">Vehicle damage scaling factor</label>
    <input id="vehicle-damage-slider" type="range" min="0" max="4" step="0.1" value="1" />
    <input id="vehicle-damage-number" type="number" min="0" max="4" step="0.1" value="1.0" aria-label="Vehicle damage scaling factor" />
  </div>
  <ul id="vehicle-damage-summary" class="vds-summary"></ul>
  <div class="vds-chart">
    <div class="vds-header">
      <span class="vds-roll">roll</span>
      <span>distribution</span>
      <span class="vds-prob">P(roll)</span>
      <span class="vds-hp">HP</span>
    </div>
    <div id="vehicle-damage-chart"></div>
  </div>
</div>

<script type="module" src="/tools/vehicle_damage_scaling.js"></script>

## Older worlds

Before the range was reduced to `0.0`-`4.0`, the option accepted values up to `10.0`. Anything above
`4.0` already broke every part of a damaged vehicle. A world saved with a value outside the new
range loads with the default value `1.0` instead.
