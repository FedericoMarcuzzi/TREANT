cmake_minimum_required(VERSION 3.9)
project (treant LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

IF (NOT WIN32)
    IF(NOT CMAKE_BUILD_TYPE)
        SET(CMAKE_BUILD_TYPE Release)
    endif(NOT CMAKE_BUILD_TYPE)
ENDIF ()

SET(CMAKE_CXX_STANDARD 17)

IF (WIN32)
    SET(CMAKE_CXX_FLAGS "/Zi /EHsc /MP /Wall /W4")
ELSE ()
    SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -pthread -fPIC -march=native -lm")
    SET(CMAKE_CXX_FLAGS_RELEASE "-Werror -O3 -s")
    SET(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
ENDIF ()

include_directories(include)

set(SOURCES
        # Headers
        include/utils.h
        include/types.h
        include/Logger.h
        include/OptParser.h
        include/Node.h
        include/DecisionTree.h
        include/Dataset.h
        include/SplitOptimizer.h
        include/Attacker.h
        include/Constraint.h
        include/BaggingClassifier.h

        # Sources
        src/utils.cpp
        src/Logger.cpp
        src/OptParser.cpp
        src/DecisionTree.cpp
        src/Dataset.cpp
        src/Node.cpp
        src/SplitOptimizer.cpp
        src/Attacker.cpp
        src/Constraint.cpp
        src/BaggingClassifier.cpp)


SET(TREANT_LIB treantLib)

add_library(${TREANT_LIB}
        SHARED
        ${SOURCES}
        )

include(FindPkgConfig)
find_package(Nlopt REQUIRED)
include_directories(${NLOPT_INCLUDE_DIR})
target_link_libraries(${TREANT_LIB}
        PRIVATE
        ${NLOPT_LIBRARY}
        )

# Build the executable in the main
add_executable(${CMAKE_PROJECT_NAME} main.cpp)
target_link_libraries(${CMAKE_PROJECT_NAME} ${TREANT_LIB})