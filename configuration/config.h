#ifndef SUBGRAPHMATCHING_CONFIG_H
#define SUBGRAPHMATCHING_CONFIG_H


/**
 * Set the maximum size of a query graph. By default, we set the value as 64.
 */
#define MAXIMUM_QUERY_GRAPH_SIZE 64
#define HASH_TABLE_RATIO 1.2

/**
 * Define ENABLE_FAILING_SET to enable the failing set pruning set intersection method.
 */
#define ENABLE_FAILING_SET

/**
 * Define ENABLE_EQUIVALENT_SET to enable the equivalent set pruning set intersection method.
 */
#define ENABLE_EQUIVALENT_SET

/**
 * Define ANALYZE_DUPLICATE to enable the record the duplicate information
 */
// #define ANALYZE_DUPLICATE


#define PRINT_SEPARATOR "------------------------------"

#endif //SUBGRAPHMATCHING_CONFIG_H
