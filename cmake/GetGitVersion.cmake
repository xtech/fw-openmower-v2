# Get git version information and generate a header file
# This script is run at build time to capture git hash and build date

function(get_git_version OUTPUT_DIR)
    # Get git hash (short form, 7 characters)
    execute_process(
        COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT GIT_HASH)
        set(GIT_HASH "unknown")
    endif()

    # Check if repository is dirty (has uncommitted changes)
    execute_process(
        COMMAND git status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DIRTY
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(GIT_DIRTY)
        string(APPEND GIT_HASH "-dirty")
    endif()

    # Get current date and time (ISO 8601 format)
    string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M:%S UTC" UTC)

    # Generate header file
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/git_version.h.in
        ${OUTPUT_DIR}/git_version.h
        @ONLY
    )

    message(STATUS "Git version: ${GIT_HASH} (Built: ${BUILD_DATE})")
endfunction()
