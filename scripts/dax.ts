/**
 * @module
 *
 * Shared Dax instance for scripts that need restricted subprocess permissions.
 */

import { build$ } from "@david/dax"

const pathEnv = (): Record<string, string> => {
  try {
    return { PATH: Deno.env.get("PATH") ?? "" }
  } catch (error) {
    if (error instanceof Deno.errors.NotCapable) return {}
    throw error
  }
}

const $ = build$({
  commandBuilder: (builder) => builder.clearEnv().env(pathEnv()),
})

export default $
