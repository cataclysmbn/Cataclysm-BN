#pragma once

enum class merge_comestible_t {
    merge_legacy,
    merge_liquid,
    merge_all,
};

/**
 * Merge similar comestibles.  Legacy: default behavior.  Liquid: Merge only liquid comestibles.  All: Merge all comestibles.
 */
extern merge_comestible_t merge_comestible_mode;

/**
 * Limit maximum allowed staleness difference when merging comestibles.
 * The lower the value, the more similar the items must be to merge.
 * 0.0: Only merge identical items.
 * 1.0: Merge comestibles regardless of its freshness.
 */
extern float similarity_threshold;


