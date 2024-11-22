set(samples_dir ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(samples_dir_name ${samples_dir} NAME)
add_subdirectory(TestEvents)
add_subdirectory(TestRttr)
add_subdirectory(TestDelayQueue)
add_subdirectory(TestLog)

