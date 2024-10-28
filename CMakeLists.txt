cmake_minimum_required(VERSION 3.21)

# set the project name
project(ca-imageio)

# set the output directories
set (ProdDir "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ProdDir}/lib/static")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ProdDir}/lib/shared")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ProdDir}/bin")

if(PROJECT_IS_TOP_LEVEL)
    # create a bin directory within the root of the project for convenience
    file(MAKE_DIRECTORY "bin")
    include(fetchContent)
    FetchContent_Declare(
        "ca-image"
        GIT_REPOSITORY "https://github.com/canadianavenger/ca-image"
        GIT_TAG "origin/main" # use origin/main to always use latest, use main to fetch once and forget
    )
    FetchContent_Declare(
        "ca-utils"
        GIT_REPOSITORY "https://github.com/canadianavenger/ca-util"
        GIT_TAG "origin/main" # use origin/main to always use latest, use main to fetch once and forget
    )
    FetchContent_MakeAvailable("ca-image" "ca-utils")
endif()


include_directories(
    "."
    "include"
    "${ca-utils_SOURCE_DIR}/include"
    "${ca-image_SOURCE_DIR}/include"
)

# to keep things clean keep the sources in groups
set (bmp
    "src/bmp/bmp_load.c"
    "src/bmp/bmp_save.c"
)

set (general 
)

# consolidate the groups
set (sources 
    ${general}
    ${bmp}
)

# generate the consolidated library
add_library (${PROJECT_NAME} ${sources})

if(PROJECT_IS_TOP_LEVEL)
    set (common
        "test/rawio.c"
    )

    set (executables 
        bmp2raw
        raw2bmp
    )

    #build all our program executables
    foreach(executable IN LISTS executables)
        add_executable(${executable} "test/${executable}.c" ${common})
        target_link_libraries(${executable} ${PROJECT_NAME} "ca-image" "ca-utils")
        # for convenience copy the executables into the projects bin directory
        add_custom_command(TARGET ${executable} POST_BUILD
            COMMAND cp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${executable}" "${CMAKE_SOURCE_DIR}/bin/${executable}"
        )
    endforeach(executable IN LISTS executables)

endif()
