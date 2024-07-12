cmake_minimum_required(VERSION 3.9)

project(easyVDB VERSION 1.0.0 LANGUAGES CXX)

# Directories
set(VDB_DIR_SRC "${CMAKE_CURRENT_LIST_DIR}/src")
set(VDB_DIR_LIBS "${CMAKE_CURRENT_LIST_DIR}/libraries")

# Sources
macro(VDB_SOURCES_APPEND)
    file(GLOB FILES_APPEND CONFIGURE_DEPENDS ${ARGV0}/*.h)
    list(APPEND VDB_HEADERS ${FILES_APPEND})
    file(GLOB FILES_APPEND CONFIGURE_DEPENDS ${ARGV0}/*.cpp)
    list(APPEND VDB_SOURCES ${FILES_APPEND})
endmacro()

VDB_SOURCES_APPEND(${VDB_DIR_SRC})

add_library(${PROJECT_NAME} STATIC ${VDB_SOURCES} ${VDB_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${VDB_DIR_SRC})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

# zlib
add_subdirectory(libraries/martell-zlib)
target_link_libraries(${PROJECT_NAME} PUBLIC zlibstatic)
set_property(TARGET zlibstatic PROPERTY FOLDER "External/zlib")
if(EXISTS ${VDB_DIR_LIBS}/martell-zlib/zconf.h.included)
    message(STATUS "Renaming to zconf.h again")
    file(RENAME ${VDB_DIR_LIBS}/martell-zlib/zconf.h.included ${VDB_DIR_LIBS}/martell-zlib/zconf.h)
endif()

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