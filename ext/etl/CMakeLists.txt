Include(FetchContent)

FetchContent_Declare(
        etl
        GIT_REPOSITORY https://github.com/ETLCPP/etl
        GIT_TAG        20.39.4
)

FetchContent_MakeAvailable(etl)