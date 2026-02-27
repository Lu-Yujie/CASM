set(SUBGRAPH_MATCHING_FILE_LIST
    CSMEngine.cpp
    CSMIndex.cpp
    QueryEngine.cpp
    main.cpp
)

add_executable(BS ${SUBGRAPH_MATCHING_FILE_LIST})
target_link_libraries(BS graph utils)
target_link_libraries(BS gmp)
