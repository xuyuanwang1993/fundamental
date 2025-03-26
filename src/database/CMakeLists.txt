# Set lib_name automatically to its folder name
get_filename_component(lib_name ${CMAKE_CURRENT_LIST_DIR} NAME)

file(GLOB lib_source
    *.h
    *.cpp
    *.hpp
)

set(extra_include_dirs)
set(extra_libs)
if(FUNDAMENTAL_ENABLE_SQLITE3_SUPPORT)
    file(GLOB sqlite3_src
        sqlite3/*.h
        sqlite3/*.cpp
        sqlite3/*.hpp
    )
    list(APPEND lib_source ${sqlite3_src})
    # we should get sqlite3 with pkgconfig
    # libsqlite3-dev should be installed firstly 
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
    if(SQLITE3_FOUND)
        list(APPEND extra_libs ${SQLITE3_LIBRARIES})
        list(APPEND extra_include_dirs ${SQLITE3_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "you should install libsqlite3-dev package")
    endif()

endif()

# Add to source group, TREE will enable cmake to config the folder layout automatically
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${lib_source})

add_library(${lib_name} STATIC ${lib_source})

target_include_directories(${lib_name} PUBLIC
    ${src_root_dir}
    ${extra_include_dirs}
)

set_target_properties(${lib_name} PROPERTIES
    FOLDER ${src_root_dir}
)


# Config link dependencies
target_link_libraries(${lib_name} PUBLIC
    BuildSettings
    ${extra_libs}
)

add_library(${GLOB_NAMESPACE}${lib_name} ALIAS ${lib_name})
