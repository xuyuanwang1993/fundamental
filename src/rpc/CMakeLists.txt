# Set lib_name automatically to its folder name
get_filename_component(lib_name ${CMAKE_CURRENT_LIST_DIR} NAME)

file(GLOB lib_source
    basic/*.h
    basic/*.cpp
    basic/*.hpp
)

option(RPC_DISABLE_SSL "disable rpc ssl support." OFF)
list(APPEND lib_source ${services_source})

# Add to source group, TREE will enable cmake to config the folder layout automatically
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${lib_source})

add_library(${lib_name} INTERFACE)

target_include_directories(${lib_name} INTERFACE
    ${src_root_dir}
)

set_target_properties(${lib_name} PROPERTIES
    FOLDER ${src_root_dir}
)

# Config link dependencies
target_link_libraries(${lib_name} INTERFACE
    BuildSettings
    ${GLOB_NAMESPACE}network${STATIC_LIB_SUFFIX}
)

if(RPC_DISABLE_SSL)
    target_compile_definitions(${lib_name}
        INTERFACE
        RPC_DISABLE_SSL=1
    )
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${lib_name}
        INTERFACE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()
add_library(${GLOB_NAMESPACE}${lib_name} ALIAS ${lib_name})
