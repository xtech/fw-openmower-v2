Include(FetchContent)

# Tarball instead of GIT_REPOSITORY (see ext/lvgl/CMakeLists.txt): the pinned
# commit is fetched as an archive, so no ~37 MB of history is downloaded and
# the build needs no git for its dependencies.
FetchContent_Declare(
        etl
        URL      https://github.com/ETLCPP/etl/archive/743ed78223c77ea7e3353d8476f386488f5e5743.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(etl)
