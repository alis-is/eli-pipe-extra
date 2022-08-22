project (eli_pipe_extra)

file(GLOB eli_pipe_extra_sources ./src/**.c)
set(eli_pipe_extra ${eli_pipe_extra_sources})

add_library(eli_pipe_extra ${eli_pipe_extra})
target_link_libraries(eli_pipe_extra)