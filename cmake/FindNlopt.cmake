set(FIND_NLOPT_PATHS
        3rdparty/nlopt-*)

find_path(NLOPT_INCLUDE_DIR nlopt.hpp
        PATH_SUFFIXES include
        PATHS ${FIND_NLOPT_PATHS})

find_library(NLOPT_LIBRARY
        NAMES nlopt
        PATH_SUFFIXES lib
        PATHS ${FIND_NLOPT_PATHS})