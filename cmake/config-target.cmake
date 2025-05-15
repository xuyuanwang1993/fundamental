if(__FUNDAMENTAL_CONFIG_TARGET__)
    return()
endif()
set(__FUNDAMENTAL_CONFIG_TARGET__ TRUE)

include(clang-tidy-helper)
option(F_BUILD_STATIC "build static fundamental lib" ON)
option(F_BUILD_SHARED "build dynamic fundamental lib" OFF)
option(ENABLE_DEBUG_MEMORY_TRACE "enable memory track" ON)
option(CLANG_BUILD_WITH_STD_CXX "clang build with libstdc++" ON)
option(F_ENABLE_COMPILE_OPTIMIZE "enable compile optimize" ON)

set(RTTR_LIB RTTR::Core_Lib CACHE STRING "use rttr static lib")
set(GLOB_NAMESPACE "fh::" CACHE STRING "generated lib namespace")
if(F_BUILD_SHARED)
    set(STATIC_LIB_SUFFIX _s CACHE STRING "static lib suffix")
endif()

#make these values invisible
mark_as_advanced(F_BUILD_STATIC)

if(TARGET BuildSettings)
    return()
endif()
add_library(BuildSettings INTERFACE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "build on linux")
    add_definitions(-DTARGET_PLATFORM_LINUX=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_definitions(-DTARGET_PLATFORM_WINDOWS=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_definitions(-DTARGET_PLATFORM_MAC=1)
else()
    message(FATAL_ERROR "Unknown platform.")
endif()

target_precompile_headers(BuildSettings INTERFACE "${CMAKE_CURRENT_LIST_DIR}/platform.h.in")

include(CheckCXXSourceCompiles)

## 
macro(CHECK_COMPILE_DEFINITIONS definition_name)
    check_cxx_source_compiles("
        #ifndef ${definition_name}
        #error ${definition_name} not defined
        #endif
        int main() { return 0; }
    "        ${definition_name}_DEFINED_)
endmacro(CHECK_COMPILE_DEFINITIONS)

macro(ADD_COMPILE_DEFINITION config_type definition_name)
    if(CMAKE_BUILD_TYPE STREQUAL "${config_type}")
        CHECK_COMPILE_DEFINITIONS(${definition_name})
        if(${definition_name}_DEFINED_)
            message(STATUS "${definition_name} is already defined in ${config_type} configuration.")
        else()
            message(STATUS "Adding ${definition_name}=1 for ${config_type} configuration")
            target_compile_definitions(BuildSettings INTERFACE
                $<$<CONFIG:${config_type}>:${definition_name}=1>
            )
        endif()
    endif()
endmacro(ADD_COMPILE_DEFINITION)

#add compile macro
ADD_COMPILE_DEFINITION(Debug DEBUG)
ADD_COMPILE_DEFINITION(Release NDEBUG)
target_compile_options(BuildSettings INTERFACE
    "$<$<CONFIG:Debug>:-O0;-Wall;-g2;-ggdb;-fno-omit-frame-pointer>"
    "$<$<CONFIG:RelWithDebInfo>:-O2;-Wall;-g>"
    "$<$<CONFIG:Release>:-O3;-Wall>"
    "-fPIC"
)

if(F_ENABLE_COMPILE_OPTIMIZE)
    # Release 编译器通用优化
    target_compile_options(BuildSettings INTERFACE
        $<$<CONFIG:Release>:
        # GCC/Clang
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -fomit-frame-pointer # 省略帧指针
        -fno-strict-aliasing # 宽松别名规则(某些代码需要)
        -funroll-loops # 循环展开
        -ffast-math # 快速数学计算
        -fno-trapping-math # 禁用浮点陷阱
        >

        # MSVC
        $<$<CXX_COMPILER_ID:MSVC>:
        /Oy- # 禁用帧指针省略(调试更友好)
        /fp:fast # 快速浮点模型
        /Qpar # 自动并行化
        /GL # 全程序优化
        >
        >
    )
    target_compile_options(BuildSettings INTERFACE
        $<$<CONFIG:Release>:
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -finline-functions # 内联简单函数
        -finline-limit=200 # 提高内联阈值
        >
        $<$<CXX_COMPILER_ID:MSVC>:
        /Ob2 # 任意适合的内联
        /inline # 启用内联展开
        >
        >
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x86_64|AMD64")
        target_compile_options(BuildSettings INTERFACE
            $<$<CONFIG:Release>:
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -mavx2 -mbmi2 -mfma
            >
            $<$<CXX_COMPILER_ID:MSVC>:
            /arch:AVX2
            >
            >
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        target_compile_options(BuildSettings INTERFACE
            $<$<CONFIG:Release>:
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -mcpu=native -mtune=native
            >
            >
        )
    endif()
endif()
target_compile_features(BuildSettings INTERFACE
    "cxx_std_17"
)

target_compile_definitions(BuildSettings INTERFACE
    "$<$<CONFIG:Debug>:VERBOSE_LOGGING=1>"
    "$<$<CONFIG:Release>:OPTIMIZED=1>"
)

if(ENABLE_DEBUG_MEMORY_TRACE)
    target_compile_definitions(BuildSettings INTERFACE
        "$<$<CONFIG:Debug>:WITH_MEMORY_TRACK>"
    )
endif()

#add optimize config

target_compile_options(BuildSettings INTERFACE
    # LTO 编译选项（-flto 或 /GL）
    $<$<NOT:$<CONFIG:Debug>>:
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-flto=auto>
    $<$<CXX_COMPILER_ID:MSVC>:/GL>
    >
    # GCC/Clang 分节选项（为 --gc-sections 做准备）
    $<$<NOT:$<CONFIG:Debug>>:
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    -ffunction-sections
    -fdata-sections
    >
    >
)

target_link_options(BuildSettings INTERFACE
    # LTO 链接选项（-flto 或 /LTCG）
    $<$<NOT:$<CONFIG:Debug>>:
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-flto=auto>
    $<$<CXX_COMPILER_ID:MSVC>:/LTCG>
    >
    # 无用代码剥离（--gc-sections 或 /OPT:REF）
    $<$<NOT:$<CONFIG:Debug>>:
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wl,--gc-sections>
    $<$<CXX_COMPILER_ID:MSVC>:/OPT:REF>
    >
    # 可选：MSVC 的代码段合并（类似 ICF）
    $<$<NOT:$<CONFIG:Debug>>:
    $<$<CXX_COMPILER_ID:MSVC>:/OPT:ICF>
    >

)
if(F_ENABLE_COMPILE_OPTIMIZE)
    target_link_options(BuildSettings INTERFACE
        "$<$<CONFIG:RelWithDebInfo>:-Wl,-O2>"
        "$<$<CONFIG:Release>:-Wl,-O3>"
    )
endif()

set_target_properties(BuildSettings PROPERTIES POSITION_INDEPENDENT_CODE ON)



target_link_libraries(BuildSettings INTERFACE
    pthread
)
target_enable_clang_tidy(BuildSettings)

# enable memory access check for debug mode
function(config_enable_sanitize_address_check target_name)
    if(DISABLE_DEBUG_SANITIZE_ADDRESS_CHECK)
        return()
    endif()
    target_compile_options(${target_name} PRIVATE
        "$<$<CONFIG:Debug>:-fstack-protector>"
    )
    target_compile_options(${target_name} PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address>"
    )

    target_link_options(${target_name} PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address>"
    )
    #you shoulde install libasan5 in ubuntu
    target_link_libraries(
        ${target_name} PRIVATE
        "$<$<CONFIG:Debug>:asan>"
    )
endfunction()

# enable memory profiling check for debug mode
# export MALLOC_CONF="prof:true,prof_active:true,lg_prof_sample:0,prof_leak:true,prof_accum:true"
function(config_enable_jemalloc_memory_profiling target_name)
    if(NOT ENABLE_JEMALLOC_MEMORY_PROFILING)
        return()
    endif()
    target_compile_definitions(${target_name} PRIVATE
        "$<$<CONFIG:Debug>:ENABLE_JEMALLOC_MEMORY_PROFILING=1>"
    )

    #you shoulde install libjemalloc-dev in ubuntu
    target_link_libraries(
        ${target_name} PRIVATE
        "$<$<CONFIG:Debug>:jemalloc>"
    )
endfunction()


# disable rtti for no debug mode
function(config_disable_rtti target_name)
    # TODO fix no rtti compile error
    # target_compile_options(${target_name} PRIVATE
    #     "$<$<NOT:$<CONFIG:Debug>>:$<$<CXX_COMPILER_ID:GNU,Clang>:-fno-rtti>>"
    #     "$<$<NOT:$<CONFIG:Debug>>:$<$<CXX_COMPILER_ID:MSVC>:/GR->>"
    # )
endfunction()

# enable profiling
# run you program to generate gmon.out
# then  'gprof program gmon.out > analysis.txt'
function(config_enable_pg_profiling target_name)
    target_compile_options(${target_name} PRIVATE "$<$<CONFIG:Debug>:-pg>"
    )
    target_link_options(${target_name} PRIVATE "$<$<CONFIG:Debug>:-pg>"
    )
endfunction()

#strip all debug info for no debug mode
function(config_strip_debug_info target_name)
    # 检查目标是否为可执行程序
    get_property(target_type TARGET ${target_name} PROPERTY TYPE)
    if(NOT (target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY"))
        return()
    endif()
    target_link_options(${target_name} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wl,--strip-all>
        $<$<CXX_COMPILER_ID:MSVC>:/link /RELEASE>
        >
    )
endfunction()


function(add_plugin plugin_name)
    if(PLUGIN_USE_STATIC)
        add_library(${plugin_name} STATIC ${ARGN})
    else()
        add_library(${plugin_name} SHARED ${ARGN})
        target_compile_definitions(${plugin_name} PRIVATE __PLUGIN_DLL_EXPORTS_)
        target_compile_definitions(${plugin_name} PUBLIC PLUGIN_BUILD_SHARED)
        if(CMAKE_COMPILER_IS_GNUCXX)
            # we have to use this flag, otherwise dlclose does not call the destructor function
            # of the library
            target_compile_options(${plugin_name} PRIVATE "-fno-gnu-unique")
        endif()
    endif()
    # Add plugin name
    target_compile_definitions(${plugin_name}
        PRIVATE
        PLUGIN_NAME= "${plugin_name}"
    )
endfunction()

#add specific compile options 
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "use gnu compiler")
    if(MINGW)
        set(GNU_STATIC_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
    else()
        set(GNU_STATIC_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "use clang compiler")
    add_compile_options(-Wno-implicit-const-int-float-conversion)
    if(CLANG_BUILD_WITH_STD_CXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
        set(CLANG_STATIC_LINKER_FLAGS "-stdlib=libstdc++ -static-libstdc++")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CLANG_STATIC_LINKER_FLAGS "-stdlib=libc++ -static-libc++")
    endif()
endif()

function(enable_origin_rpath app_target_name)
    set_target_properties(${app_target_name} PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endfunction()

