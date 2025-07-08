import { baseCli } from "$catjazz/mod.ts"
import { z } from "$catjazz/deps/zod.ts"

/**
 * [PR#4214](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/4211)
 *
 * for items with `SPEAR` flag,
 * 1. if it has `REACH_ATTACK`, also add `STAB` flag.
 * 2. if it does **NOT** have `REACH_ATTACK`, replace `SPEAR` with `STAB`
 */
export const spearToStab = (xs: string[]): string[] => {
  // if it already has STAB, it's already been converted
  const hasStab = xs.includes("STAB")
  if (hasStab) return xs

  const hasSpear = xs.includes("SPEAR")
  if (!hasSpear) return xs

  const hasReachAttack = xs.includes("REACH_ATTACK")
  if (hasReachAttack) return [...xs, "STAB"]

  if (!hasSpear && !hasReachAttack) return xs
  return xs.map((x) => x === "SPEAR" ? "STAB" : x)
}

export const schema = z
  .object({ flags: z.array(z.string()).transform(spearToStab) })
  .passthrough()

if (import.meta.main) {
  const main = baseCli({
    desc: "Migrate SPEAR without REACH_ATTACK to STAB",
    schema,
  })

  await main().parse(Deno.args)
}
