set(samples_dir ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(samples_dir_name ${samples_dir} NAME)

if(F_BUILD_STATIC)
add_subdirectory(TestEvents)
add_subdirectory(TestDelayQueue)
add_subdirectory(TestLog)
endif()

if(F_BUILD_SHARED)
add_subdirectory(TestRttr)
endif()

if(ENABLE_DEBUG_MEMORY_TRACE)
add_subdirectory(TestMemoryTracker)
endif()
add_subdirectory(TestAsio)