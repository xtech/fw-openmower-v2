Include(FetchContent)

# Tarball instead of GIT_REPOSITORY: FetchContent cloned LVGL's full history
# (11 886 commits, 689 MB checkout of which 521 MB is .git) on every CI run,
# because the Docker layer that configures is invalidated by COPY . on each
# commit. The tag archive is 111 MB and needs no git.
# See ext/etl/CMakeLists.txt for the same change on a pinned commit.
FetchContent_Declare(
        lvgl
        URL      https://github.com/lvgl/lvgl/archive/refs/tags/v9.4.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(lvgl)
