#ifndef CSM_CONFIG_H
#define CSM_CONFIG_H


/**
 * Set the maximum size of a query graph. By default, we set the value as 64.
 */
#define MAXIMUM_QUERY_GRAPH_SIZE 64
#define HASH_TABLE_RATIO 1.2

#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

/**
 * Define PRUNE_THRESHOLD_PERCENT to constrain the pruning propagation condition.
 */
#define PRUNE_THRESHOLD_PERCENT 30
#define SIGNIFICANT_DROP(old_size, new_size) \
    ((uint64_t)((old_size) - (new_size)) * 100 > (uint64_t)(PRUNE_THRESHOLD_PERCENT) * (old_size))

#define PRINT_SEPARATOR "------------------------------"

#endif //CSM_CONFIG_H
