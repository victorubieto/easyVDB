cmake_minimum_required(VERSION 3.9)

project(easyVDB VERSION 1.0.0 LANGUAGES CXX)

# Directories
set(DIR_VDB "${CMAKE_CURRENT_LIST_DIR}/easyVDB")

# enable FetchContent
include(FetchContent)

FetchContent_Declare(
    zlib-cmake
    URL https://github.com/jimmy-park/zlib-cmake/archive/main.zip
)

FetchContent_MakeAvailable(zlib-cmake)

# Sources
macro(VDB_SOURCES_APPEND)
    file(GLOB FILES_APPEND CONFIGURE_DEPENDS ${ARGV0}/*.h)
    list(APPEND VDB_HEADERS ${FILES_APPEND})
    file(GLOB FILES_APPEND CONFIGURE_DEPENDS ${ARGV0}/*.cpp)
    list(APPEND VDB_SOURCES ${FILES_APPEND})
endmacro()

VDB_SOURCES_APPEND(${DIR_VDB})

add_library(${PROJECT_NAME} STATIC ${VDB_SOURCES} ${VDB_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${DIR_VDB})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

# glm
if (NOT TARGET glm)
    add_subdirectory(libraries/glm)

    target_compile_definitions(glm PUBLIC GLM_ENABLE_EXPERIMENTAL)
    target_compile_definitions(glm PUBLIC GLM_FORCE_QUAT_DATA_XYZW)
    target_compile_definitions(glm PUBLIC GLM_FORCE_QUAT_CTOR_XYZW)

    set_property(TARGET glm PROPERTY CXX_STANDARD 20)
    set_property(TARGET glm PROPERTY FOLDER "External/glm")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE glm)