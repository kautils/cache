cmake_minimum_required(VERSION 3.20)
set(__version 2.0.1)
project(KautilCache.${__version}
        LANGUAGES C CXX
        VERSION ${__version}
        )
unset(__version)
include(${CMAKE_CURRENT_LIST_DIR}/cache/cache.cmake)