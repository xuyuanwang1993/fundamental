include(CheckCXXCompilerFlag)


# 检查编译器是否支持 C++20
check_cxx_compiler_flag("-std=c++20" ENV_CXX20_SUPPORTED)
message(STATUS "ENV_CXX20_SUPPORTED=${ENV_CXX20_SUPPORTED}")
set(ENV_CXX20_SUPPORTED "${ENV_CXX20_SUPPORTED}" CACHE INTERNAL "")



include(CheckCXXSourceCompiles)

check_cxx_source_compiles("
  #include <filesystem>
  int main() { std::filesystem::path p(\".\"); return 0; }
"    HAS_STD_FILESYSTEM)
message(STATUS "HAS_STD_FILESYSTEM=${HAS_STD_FILESYSTEM}")
set(HAS_STD_FILESYSTEM "${HAS_STD_FILESYSTEM}" CACHE INTERNAL "")


check_cxx_source_compiles("
  #include <memory_resource>
  int main() { std::pmr::memory_resource s; return 0; }
"    HAS_STD_MEMORY_SOURCE)
message(STATUS "HAS_STD_MEMORY_SOURCE=${HAS_STD_MEMORY_SOURCE}")
set(HAS_STD_MEMORY_SOURCE "${HAS_STD_MEMORY_SOURCE}" CACHE INTERNAL "")