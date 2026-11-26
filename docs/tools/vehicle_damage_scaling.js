// @ts-nocheck: browser-only script with dynamic DOM APIs.

const DICE = 4
const SIDES = 8
const TOTAL_OUTCOMES = SIDES ** DICE

/** Number of ways to roll each sum of `DICE`d`SIDES`, indexed by sum. */
const rollCounts = (() => {
  let counts = [1]
  for (let die = 0; die < DICE; die++) {
    const next = new Array(counts.length + SIDES).fill(0)
    counts.forEach((ways, sum) => {
      for (let face = 1; face <= SIDES; face++) next[sum + face] += ways
    })
    counts = next
  }
  return counts
})()

const minRoll = DICE
const maxRoll = DICE * SIDES

/** Mirrors `int x = 8 * chance;` in C++, where `chance` is a 32-bit float. */
const truncFloatProduct = (factor, chance) => Math.trunc(Math.fround(factor * Math.fround(chance)))

/** Mirrors the initial damage roll in `vehicle::init_state`. */
const classify = (roll, broken, unhurt) => {
  if (roll >= unhurt) return { kind: "full", hp: 1 }
  if (roll <= broken) return { kind: "broken", hp: 0 }
  return { kind: "damaged", hp: (roll - broken) / (unhurt - broken) }
}

/** Damaged bar colors in 20% HP steps, from 0-20% (red) up to 80-100% (green). */
const DAMAGED_COLORS = ["#d7301f", "#f08c1e", "#e8c21a", "#9ccb2f", "#2e8b3e"]

const damagedColor = (hp) => DAMAGED_COLORS[Math.min(Math.floor(hp * 5), 4)]

const percent = (value, digits = 1) => `${(value * 100).toFixed(digits)}%`

const root = document.getElementById("vehicle-damage-tool")
const slider = root.querySelector("#vehicle-damage-slider")
const number = root.querySelector("#vehicle-damage-number")
const summary = root.querySelector("#vehicle-damage-summary")
const chart = root.querySelector("#vehicle-damage-chart")

const maxProbability = Math.max(...rollCounts) / TOTAL_OUTCOMES

const render = (chance) => {
  const broken = truncFloatProduct(8, chance)
  const unhurt = truncFloatProduct(20, chance)

  const rows = []
  const totals = { full: 0, damaged: 0, broken: 0 }
  let expectedHp = 0

  for (let roll = maxRoll; roll >= minRoll; roll--) {
    const probability = rollCounts[roll] / TOTAL_OUTCOMES
    const { kind, hp } = classify(roll, broken, unhurt)
    totals[kind] += probability
    expectedHp += probability * hp

    const row = document.createElement("div")
    row.className = `vds-row vds-${kind}`

    const width = (probability / maxProbability) * 100
    const hpLabel = kind === "damaged" ? percent(hp, 0) : kind === "full" ? "100%" : "0%"
    const barColor = kind === "damaged" ? `;background:${damagedColor(hp)}` : ""
    row.innerHTML = `
      <span class="vds-roll">${roll}</span>
      <span class="vds-track">
        <span class="vds-bar" style="width:${width}%${barColor}"></span>
      </span>
      <span class="vds-prob">${percent(probability, 2)}</span>
      <span class="vds-hp">${hpLabel}</span>`
    rows.push(row)
  }

  chart.replaceChildren(...rows)
  summary.innerHTML = `
    <li><span class="vds-swatch vds-full"></span>Intact: <strong>${
    percent(totals.full)
  }</strong></li>
    <li><span class="vds-swatch vds-damaged"></span>Damaged: <strong>${
    percent(totals.damaged)
  }</strong></li>
    <li><span class="vds-swatch vds-broken"></span>Broken (0 HP): <strong>${
    percent(totals.broken)
  }</strong></li>
    <li>Expected HP per part: <strong>${percent(expectedHp)}</strong></li>`
}

const update = (source) => {
  const min = Number(slider.min)
  const max = Number(slider.max)
  const value = Math.min(max, Math.max(min, Number(source.value) || 0))
  slider.value = String(value)
  number.value = value.toFixed(1)
  render(value)
}

slider.addEventListener("input", () => update(slider))
number.addEventListener("change", () => update(number))
update(slider)
