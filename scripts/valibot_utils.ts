import { omit } from "@std/collections/omit"
import * as v from "@valibot/valibot"

export const looseObjectWithout = <const TEntries extends v.ObjectEntries>(
  entries: TEntries,
  keys: string[],
) =>
  v.pipe(
    v.looseObject(entries),
    v.transform((x) => omit(x, keys)),
  )

export const asUndefined = <
  const TSchema extends v.BaseSchema<unknown, unknown, v.BaseIssue<unknown>>,
>(
  schema: TSchema,
) => v.pipe(schema, v.transform(() => undefined))
