set(RTTR_LIB RTTR::Core_Lib CACHE STRING "use rttr static lib")

add_library(BuildSettings INTERFACE)
target_precompile_headers(BuildSettings INTERFACE "${CMAKE_CURRENT_LIST_DIR}/platform.h.in")
target_compile_options(BuildSettings INTERFACE
    -std=c++17
    "$<$<CONFIG:Debug>:-DDEBUG_MODE -O0 -Wall -g2 -ggdb>"
    "$<$<CONFIG:Release>:-DNDEBUG -O3 -Wall>"
)

target_compile_definitions(BuildSettings INTERFACE
    "$<$<CONFIG:Debug>:DEBUG_MODE>"
    "$<$<CONFIG:Debug>:VERBOSE_LOGGING>"
    "$<$<CONFIG:Release>:NDEBUG>"
    "$<$<CONFIG:Release>:OPTIMIZED>"
)

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