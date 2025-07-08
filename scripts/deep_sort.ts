// deno-lint-ignore-file no-explicit-any

/**
 * Deeply sort the properties of an object ('obj') based on the key order
 * of a reference object ('reference').
 *
 * If a property exists in both 'obj' and 'reference', and both values are
 * plain objects, the sorting is applied recursively.
 *
 * Properties in 'obj' that are not present in 'reference' are placed after
 * the sorted properties, maintaining their original relative order.
 *
 * Properties within nested objects in 'obj' whose keys are not specified
 * in the corresponding nested 'reference' object retain their original
 * relative order within that nested object.
 *
 * @param ref - The object defining the desired key order.
 * @param obj - The object to sort.
 * @returns A new object with properties sorted according to the reference.
 */
export const deepSort = <T extends Record<string, any>>(ref: SortReference, obj: T): T => {
  if (!isSortReference(ref) || !isPlainObject(obj)) {
    throw new TypeError("Both reference and obj must be plain objects.")
  }
  const result: Record<string, any> = {}
  const refKeys = ref instanceof Map ? ref.keys() : Object.keys(ref)

  // 1. Add properties based on the order in 'reference'
  for (const refKey of refKeys) {
    if (!Object.hasOwn(obj, refKey)) continue

    const refValue = ref instanceof Map ? ref.get(refKey)! : ref[refKey]
    const objValue = obj[refKey]

    // Check if both corresponding values are objects for recursive sort
    // Otherwise, just copy the value from 'obj'
    result[refKey] = (isSortReference(refValue) && isPlainObject(objValue))
      ? deepSort(refValue, objValue)
      : objValue
  }

  // 2. Add remaining properties from 'obj' that were not in 'reference'
  for (const objKey of Object.keys(obj)) {
    if (hasKey(ref, objKey)) continue
    // These keys were not in reference, so we just append them.
    // Their internal structure (if they are objects) is not sorted
    // further by the reference object at this level.
    result[objKey] = obj[objKey]
  }

  return result as T
}

type SortReference = Record<string, any> | Map<string, any>

const hasKey = (reference: SortReference, key: string): boolean =>
  reference instanceof Map ? reference.has(key) : Object.hasOwn(reference, key)

const isSortReference = (
  value: unknown,
): value is SortReference => (value instanceof Map || isPlainObject(value))

/**
 * Checks if a value is a plain JavaScript object (and not null or an array).
 * @param value - The value to check.
 * @returns True if the value is a plain object, false otherwise.
 */
const isPlainObject = (value: unknown): value is Record<string, any> =>
  typeof value === "object" && value !== null && !Array.isArray(value)
