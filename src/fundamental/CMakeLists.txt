# Set lib_name automatically to its folder name
get_filename_component(lib_name ${CMAKE_CURRENT_LIST_DIR} NAME)

file(GLOB lib_source
    *.h
    *.cpp
    *.hpp
    basic/*.h
    basic/*.cpp
    basic/*.hpp
    delay_queue/*.h
    delay_queue/*.cpp
    delay_queue/*.hpp
    events/*.h
    events/*.cpp
    events/*.hpp
    rttr_handler/*.h
    rttr_handler/*.cpp
    rttr_handler/*.hpp
    thread_pool/*.h
    thread_pool/*.cpp
    thread_pool/*.hpp
)

list(APPEND lib_source ${impl_source})

# Add to source group, TREE will enable cmake to config the folder layout automatically
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${lib_source})

add_library(${lib_name}_property INTERFACE)

target_include_directories(${lib_name}_property INTERFACE
${src_root_dir}
)

set_target_properties(${lib_name}_property PROPERTIES
    FOLDER ${src_root_dir}
)

# Config link dependencies
target_link_libraries(${lib_name}_property INTERFACE
    ${RTTR_LIB}
    spdlog
    eventpp
    nlohmann::json
    BuildSettings
)
if(F_BUILD_STATIC)
# Create the static library
add_library(${lib_name}_s STATIC ${lib_source})

# Config link dependencies
target_link_libraries(${lib_name}_s
    PUBLIC
    ${lib_name}_property
)
add_library(fh::${lib_name}_s ALIAS ${lib_name}_s)
endif()

if(F_BUILD_SHARED)
# Create the shared library
add_library(${lib_name} SHARED ${lib_source})
# Config link dependencies
target_link_libraries(${lib_name}
    PUBLIC
    ${lib_name}_property
)

add_library(fh::${lib_name} ALIAS ${lib_name})
endif()