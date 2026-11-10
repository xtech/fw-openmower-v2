# Minimal project for the "deps" stage of docker/Dockerfile: fetch the
# third-party sources into FETCHCONTENT_BASE_DIR without configuring or
# compiling the firmware. What is fetched is decided in cmake/deps.cmake, which
# is therefore also the cache key of that image layer.
cmake_minimum_required(VERSION 3.22)
project(openmower-fw-deps LANGUAGES NONE)

include(/project/cmake/deps.cmake)
