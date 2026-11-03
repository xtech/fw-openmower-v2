Include(FetchContent)

# GIT_REPOSITORY on purpose, not a URL: ETL derives its version with
# `git describe` in its own tree and only falls back to version.txt when that
# finds nothing. Without a .git it walks up into the enclosing repository, picks
# up *our* tag ("v1.4.3-14-g0dfeb6e") and turns it into an invalid project
# VERSION (leading "v"), which aborts the configure step. Its clone is 65 MB -
# LVGL is the one that hurt, see ext/lvgl/CMakeLists.txt.
FetchContent_Declare(
        etl
        GIT_REPOSITORY https://github.com/ETLCPP/etl
        GIT_TAG        743ed78223c77ea7e3353d8476f386488f5e5743
)

FetchContent_MakeAvailable(etl)
