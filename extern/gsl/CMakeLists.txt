include(FetchContent)

FetchContent_Declare(
        gsl
        GIT_REPOSITORY    https://github.com/Microsoft/GSL
        GIT_TAG           v3.0.0
)

FetchContent_MakeAvailable(gsl)
