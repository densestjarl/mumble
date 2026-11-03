# Configure from the parent project's tests with -Dvideo-pipeline-benchmark=ON.
add_executable(BenchmarkVideoPipeline BenchmarkVideoPipeline.cpp)
target_include_directories(BenchmarkVideoPipeline PRIVATE "${CMAKE_SOURCE_DIR}/src/mumble")
target_link_libraries(BenchmarkVideoPipeline PRIVATE mumble_client_lib ${CMAKE_DL_LIBS})

add_library(video_allocations SHARED allocations.c)
