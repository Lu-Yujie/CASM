set(utils_SRC
    graphoperations.cpp
    CLI11.hpp
    setOp.cpp
    quick/quickIndep.cpp
    quick/aux.cpp
)

add_library(utils SHARED ${utils_SRC})
