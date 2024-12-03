cmake_minimum_required(VERSION 3.16)

set(project_name fh-fundamental)
project(${project_name} VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enbale code IDE to use folders, this will enable source_group(TREE,...)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#add cmake search directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(FUNDAMENTAL_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake  CACHE INTERNAL "" )

#build with -fpic
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Disable the CTest targets
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

# Make sure third party is working
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# find_package() uses <PackageName>_ROOT variables
cmake_policy(SET CMP0074 NEW)
include(cmake/config-target.cmake)
include(cmake/import_gtest_config.cmake)
add_subdirectory(third-parties)
add_subdirectory(src)
add_subdirectory(samples)






