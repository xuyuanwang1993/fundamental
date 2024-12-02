function(native_copy_dlls app_target_name)
add_custom_command(TARGET ${app_target_name} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${app_target_name}> $<TARGET_RUNTIME_DLLS:${app_target_name}>
COMMAND_EXPAND_LISTS
)
endfunction(native_copy_dlls)

function(make_deploy_real_path INPUT_PATH)
  file(REAL_PATH ${${INPUT_PATH}} ACTUAL_PATH)
  set(${INPUT_PATH}
      "${ACTUAL_PATH}"
      PARENT_SCOPE)
endfunction()

if("${Custom_SystemRoot}" STREQUAL "")
set(TEMP_SYS_ROOT
      "${CMAKE_CURRENT_LIST_DIR}/../../sysroot")
else()
set(TEMP_SYS_ROOT
      "${Custom_SystemRoot}")    
endif()

make_deploy_real_path(TEMP_SYS_ROOT)
set(Custom_SystemRoot
      "${TEMP_SYS_ROOT}"
      CACHE STRING "custom system root")
set(CMAKE_INSTALL_PREFIX "${Custom_SystemRoot}")
message(STATUS "use Custom_SystemRoot=${Custom_SystemRoot}")

function(native_install_lib_package  lib_name version )
set(TARGET_CMAKE_OUTPUT_DIR "${Custom_SystemRoot}/lib/cmake")
set(TARGET_INCLUDE_OUTPUT_DIR "${Custom_SystemRoot}/lib/${lib_name}/include")
set(TARGET_LIB_OUTPUT_DIR "${Custom_SystemRoot}/lib/${lib_name}/lib")
set(TARGET_BIN_OUTPUT_DIR "${Custom_SystemRoot}/lib/${lib_name}/bin")


set_target_properties(${lib_name} PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${TARGET_BIN_OUTPUT_DIR}")
set_target_properties(${lib_name} PROPERTIES
LIBRARY_OUTPUT_DIRECTORY "${TARGET_LIB_OUTPUT_DIR}")
set_target_properties(${lib_name} PROPERTIES
ARCHIVE_OUTPUT_DIRECTORY "${TARGET_LIB_OUTPUT_DIR}")


set_target_properties(${lib_name} PROPERTIES
TARGET_CMAKE_OUTPUT_DIR "${TARGET_CMAKE_OUTPUT_DIR}"
TARGET_INCLUDE_OUTPUT_DIR "${TARGET_INCLUDE_OUTPUT_DIR}"
TARGET_LIB_OUTPUT_DIR "${TARGET_LIB_OUTPUT_DIR}"
TARGET_BIN_OUTPUT_DIR "${TARGET_BIN_OUTPUT_DIR}"
)
message(STATUS "TARGET_CMAKE_OUTPUT_DIR=${TARGET_CMAKE_OUTPUT_DIR}")
message(STATUS "TARGET_INCLUDE_OUTPUT_DIR=${TARGET_INCLUDE_OUTPUT_DIR}")
message(STATUS "TARGET_LIB_OUTPUT_DIR=${TARGET_LIB_OUTPUT_DIR}")
message(STATUS "TARGET_BIN_OUTPUT_DIR=${TARGET_BIN_OUTPUT_DIR}")

set(ExportTargets "${lib_name}Targets")
set(ExportConfigVersion "${lib_name}ConfigVersion")
set(ExportConfig "${lib_name}Config")
install(TARGETS ${lib_name} EXPORT ${ExportTargets}
    LIBRARY DESTINATION ${TARGET_LIB_OUTPUT_DIR}/$<CONFIG>
    ARCHIVE DESTINATION ${TARGET_LIB_OUTPUT_DIR}/$<CONFIG>
    RUNTIME DESTINATION ${TARGET_BIN_OUTPUT_DIR}/$<CONFIG>
    INCLUDES DESTINATION ${TARGET_INCLUDE_OUTPUT_DIR}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
      ${ExportConfigVersion}.cmake
    VERSION ${version}
    COMPATIBILITY AnyNewerVersion
)
#if you need install a lib with multi configurations
# you should config a lib with muliti configurations
message(STATUS "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}")
export(EXPORT ${ExportTargets}
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${ExportTargets}.cmake"
    NAMESPACE fh::
)

configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/TemplateLib.cmake.in ${ExportConfig}.cmake @ONLY)
install(EXPORT ${ExportTargets}
    FILE ${ExportTargets}.cmake
    NAMESPACE fh::
    DESTINATION ${TARGET_CMAKE_OUTPUT_DIR}
    COMPONENT "${lib_name}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${ExportConfig}.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${ExportConfigVersion}.cmake"
    COMPONENT "${lib_name}"
    DESTINATION ${TARGET_CMAKE_OUTPUT_DIR}
)

endfunction()

function(native_copy_extra_dependencies target_name lib_name)
add_custom_command(TARGET ${target_name}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${Custom_SystemRoot}/lib/${lib_name}/bin/$<CONFIG>" "$<TARGET_FILE_DIR:${target_name}>"
            COMMAND_EXPAND_LISTS
            VERBATIM
            )

endfunction()