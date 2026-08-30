# Get git version information and generate a header file
# This script is run at build time to capture the version string and build date
#
# Uses git describe --tags to produce version strings like:
#   v0.5.0             (exact tag, clean tree)
#   v0.5.0-3-gabc1234  (3 commits after tag)
#   v0.5.0-dirty       (exact tag with uncommitted changes)
#   abc1234             (no tags found, fallback to hash)

function(get_git_version OUTPUT_DIR)
    # Get version string: tag + commits + hash + dirty flag
    execute_process(
        COMMAND git describe --tags --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE BUILD_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT BUILD_VERSION)
        set(BUILD_VERSION "unknown")
    endif()

    # Get current date and time (ISO 8601 format)
    string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M:%S UTC" UTC)

    # Generate header file
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/git_version.h.in
        ${OUTPUT_DIR}/git_version.h
        @ONLY
    )

    message(STATUS "Version: ${BUILD_VERSION} (Built: ${BUILD_DATE})")
endfunction()
