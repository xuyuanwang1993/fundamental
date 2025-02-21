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

add_library(${lib_name}_property INTERFACE)

target_include_directories(${lib_name}_property INTERFACE
    ${src_root_dir}
)

set_target_properties(${lib_name}_property PROPERTIES
    FOLDER ${src_root_dir}
)

# Config link dependencies
target_link_libraries(${lib_name}_property INTERFACE
    BuildSettings
    ${GLOB_NAMESPACE}network${STATIC_LIB_SUFFIX}
)

if(RPC_DISABLE_SSL)
    target_compile_definitions(${lib_name}_property
        INTERFACE
        RPC_DISABLE_SSL=1
    )
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${lib_name}_property
        INTERFACE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()
if(F_BUILD_STATIC)
    # Create the static library
    add_library(${lib_name}${STATIC_LIB_SUFFIX} STATIC ${lib_source})

    # Config link dependencies
    target_link_libraries(${lib_name}${STATIC_LIB_SUFFIX}
        PUBLIC
        ${lib_name}_property
    )
    add_library(${GLOB_NAMESPACE}${lib_name}${STATIC_LIB_SUFFIX} ALIAS ${lib_name}${STATIC_LIB_SUFFIX})
endif()

if(F_BUILD_SHARED)
    # Create the shared library
    add_library(${lib_name} SHARED ${lib_source})
    # Config link dependencies
    target_link_libraries(${lib_name}
        PUBLIC
        ${lib_name}_property
    )

    add_library(${GLOB_NAMESPACE}${lib_name} ALIAS ${lib_name})
endif()
