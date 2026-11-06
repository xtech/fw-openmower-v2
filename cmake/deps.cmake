# Third-party sources that are fetched at configure time.
#
# This file is the single source of truth for them and, at the same time, the
# cache key of the "deps" stage in docker/Dockerfile: as long as it does not
# change, an image build reuses the sources that were already fetched instead of
# downloading them again. Add or bump a dependency here, not in ext/.
Include(FetchContent)

# LVGL ---------------------------------------------------------------- LVGL
# v9.4.0 as a tag archive instead of a git clone: a clone pulls 11 886 commits
# (689 MB checkout, 521 MB of it .git) that the build never uses. LVGL takes its
# version from lv_version.h, so nothing depends on it being a repository.
# The CONFIG_LV_* variables have to be set before LVGL is populated.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CONFIG_LV_BUILD_EXAMPLES OFF)
set(CONFIG_LV_BUILD_DEMOS OFF)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF)
FetchContent_Declare(
        lvgl
        URL      https://github.com/lvgl/lvgl/archive/refs/tags/v9.4.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ETL ------------------------------------------------------------------ ETL
# GIT_REPOSITORY on purpose, not an archive URL: ETL derives its version with
# `git describe` in its own tree and only falls back to version.txt when that
# finds nothing. Without a .git it walks up into the enclosing repository, picks
# up *our* tag ("v1.4.3-14-g0dfeb6e") and turns it into an invalid project
# VERSION (the leading "v" breaks its regex), which aborts the configure step.
FetchContent_Declare(
        etl
        GIT_REPOSITORY https://github.com/ETLCPP/etl
        GIT_TAG        743ed78223c77ea7e3353d8476f386488f5e5743
)

FetchContent_MakeAvailable(lvgl etl)
