set(SUBGRAPH_MATCHING_FILE_LIST
        FilterVertices.cpp
        BuildEdgeIndex.cpp
        GenerateQueryPlan.cpp
        EvaluateQuery.cpp
        GenerateFilteringPlan.cpp
        main.cpp)

add_executable(BS ${SUBGRAPH_MATCHING_FILE_LIST})
target_link_libraries(BS graph utils)
target_link_libraries(BS gmp)
