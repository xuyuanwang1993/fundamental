option(IMPORT_GTEST "import gtest" ON)
if(IMPORT_GTEST)
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY http://192.168.50.101:8787/rdd/googletest.git
  GIT_TAG        58d77fa8070e8cec2dc1ed015d66b454c8d78850 # release-1.12.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)


function(declare_gtest_exe target_name)

target_link_libraries(${target_name}
PRIVATE
  gtest
)
gtest_discover_tests(${target_name})
endfunction()

endif()