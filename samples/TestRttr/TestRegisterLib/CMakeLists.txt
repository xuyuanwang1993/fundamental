# Set lib_name automatically to its folder name
get_filename_component(lib_name ${CMAKE_CURRENT_LIST_DIR} NAME)

file(GLOB lib_source
    *.h
    *.cpp
    *.hpp
)

# Add to source group, TREE will enable cmake to config the folder layout automatically
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${lib_source})
activate_precompiled_headers(pch.h lib_source)

add_plugin(${lib_name} ${lib_source})

# Config common include and link
target_include_directories(${lib_name}
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)


set_target_properties(${lib_name} PROPERTIES
    FOLDER ${samples_dir_name}
)
# Config link dependencies
target_link_libraries(${lib_name}
    PUBLIC
    ${GLOB_NAMESPACE}fundamental
    BuildSettings
)