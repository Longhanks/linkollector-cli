include(FetchContent)

FetchContent_Declare(
        gsl
        GIT_REPOSITORY    git@github.com:Microsoft/GSL.git
        GIT_TAG           v3.0.0
)

FetchContent_MakeAvailable(gsl)
