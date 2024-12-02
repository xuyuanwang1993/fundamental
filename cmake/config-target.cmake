if(__FUNDAMENTAL__)
  return()
endif()
set(__FUNDAMENTAL__ TRUE)

option(F_BUILD_STATIC "build static fundamental lib" ON)
option(F_BUILD_SHARED "build dynamic fundamental lib" OFF)
option(ENABLE_DEBUG_MEMORY_TRACE "enable memory track" ON)

set(RTTR_LIB RTTR::Core_Lib CACHE STRING "use rttr static lib")
set(GLOB_NAMESPACE "fh::" CACHE STRING "generated lib namespace")
if(F_BUILD_SHARED)
set(STATIC_LIB_SUFFIX _s CACHE STRING "static lib suffix")
endif()

#make these values invisible
mark_as_advanced(F_BUILD_STATIC)


add_library(BuildSettings INTERFACE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
message(STATUS "build on linux")
add_definitions(-DTARGET_PLATFORM_LINUX=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
add_definitions(-DTARGET_PLATFORM_WINDOWS=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
add_definitions(-DTARGET_PLATFORM_MAC=1)
else()
message(FATAL_ERROR "Unknown platform.")
endif()

target_precompile_headers(BuildSettings INTERFACE "${CMAKE_CURRENT_LIST_DIR}/platform.h.in")
target_compile_options(BuildSettings INTERFACE
    -std=c++17
    "$<$<CONFIG:Debug>:-DDEBUG_MODE -O0 -Wall -g2 -ggdb>"
    "$<$<CONFIG:Release>:-DNDEBUG -O3 -Wall>"
)

target_compile_definitions(BuildSettings INTERFACE
    "$<$<CONFIG:Debug>:VERBOSE_LOGGING>"
    "$<$<CONFIG:Release>:NDEBUG>"
    "$<$<CONFIG:Release>:OPTIMIZED>"
)

if(ENABLE_DEBUG_MEMORY_TRACE)
target_compile_definitions(BuildSettings INTERFACE
    "$<$<CONFIG:Debug>:WITH_MEMORY_TRACK>"
)
endif()

set_target_properties(BuildSettings PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(BuildSettings INTERFACE
    pthread
)

function(add_plugin plugin_name)
if(PLUGIN_USE_STATIC)
add_library(${plugin_name} STATIC ${ARGN})
else()
add_library(${plugin_name} SHARED ${ARGN})
target_compile_definitions(${plugin_name} PRIVATE __PLUGIN_DLL_EXPORTS_)
target_compile_definitions(${plugin_name} PUBLIC PLUGIN_BUILD_SHARED)
if (CMAKE_COMPILER_IS_GNUCXX)
    # we have to use this flag, otherwise dlclose does not call the destructor function
    # of the library
    target_compile_options(${plugin_name} PRIVATE "-fno-gnu-unique")
endif()
endif()
# Add plugin name
target_compile_definitions(${plugin_name}
    PRIVATE
        PLUGIN_NAME="${plugin_name}"
)
endfunction()