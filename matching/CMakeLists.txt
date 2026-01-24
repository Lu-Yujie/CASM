set(SUBGRAPH_MATCHING_FILE_LIST
        EvaluateQuery.cpp
        BuildEdgeIndex.cpp
        CSMEngine.cpp
        CSMIndex.cpp
        main.cpp)

add_executable(BS ${SUBGRAPH_MATCHING_FILE_LIST})
target_link_libraries(BS graph utils)
target_link_libraries(BS gmp)
