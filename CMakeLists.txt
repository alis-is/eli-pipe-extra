cmake_minimum_required (VERSION 2.6)
project (eli_pipe_extra)

file(GLOB eli_pipe_extra_sources ./src/**.c)
SET(eli_pipe_extra ${eli_pipe_extra_sources})

ADD_LIBRARY (eli_pipe_extra ${eli_pipe_extra})
TARGET_LINK_LIBRARIES (eli_pipe_extra)