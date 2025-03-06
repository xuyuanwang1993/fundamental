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
    thread_pool/*.h
    thread_pool/*.cpp
    thread_pool/*.hpp
    tracker/*.h
    tracker/*.cpp
    tracker/*.hpp
)
set(extra_libs)
#prepare rttr
if(FUNDAMENTAL_BUILD_RTTR)
    file(GLOB rttr_source
        rttr_handler/*.h
        rttr_handler/*.cpp
        rttr_handler/*.hpp
    )
    list(APPEND lib_source ${rttr_source})
    list(APPEND extra_libs ${RTTR_LIB})
endif()

if(FUNDAMENTAL_BUILD_EVENTS)
    #prepare events
    file(GLOB events_source
        events/*.h
        events/*.cpp
        events/*.hpp
        application/*.h
        application/*.cpp
        application/*.hpp
    )
    list(APPEND lib_source ${events_source})
    list(APPEND extra_libs eventpp)
endif()



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
    ${extra_libs}
    spdlog
    nlohmann::json
    BuildSettings
)
if(F_BUILD_STATIC)
    # Create the static library
    add_library(${lib_name}${STATIC_LIB_SUFFIX} STATIC ${lib_source})
    config_enable_sanitize_address_check(${lib_name}${STATIC_LIB_SUFFIX})
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
    config_enable_sanitize_address_check(${lib_name})
    # Config link dependencies
    target_link_libraries(${lib_name}
        PUBLIC
        ${lib_name}_property
    )

    add_library(${GLOB_NAMESPACE}${lib_name} ALIAS ${lib_name})
endif()
