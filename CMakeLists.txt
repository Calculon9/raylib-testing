cmake_minimum_required(VERSION 3.24...3.30)
project(raylib-game)

include(FetchContent)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
set(RAYLIB_VERSION 5.5)

FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(raylib)

set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UNINSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_VERSION_SO OFF CACHE BOOL "" FORCE)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    cjson
    URL https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.18.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(cjson)
unset(CMAKE_POLICY_VERSION_MINIMUM)
target_include_directories(cjson PUBLIC "$<BUILD_INTERFACE:${cjson_SOURCE_DIR}>")

# Mini-XML uses Autotools, so build its upstream library sources directly.
set(MINI_XML_VERSION 4.0.6)
FetchContent_Populate(
    mxml
    URL https://github.com/michaelrsweet/mxml/releases/download/v${MINI_XML_VERSION}/mxml-${MINI_XML_VERSION}.tar.gz
    URL_HASH SHA256=ec1af6f7a752f63649ad00cf7355cf36366ccf729658a1a4696270de8bd74854
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

include(CheckIncludeFile)
check_include_file(pthread.h MINI_XML_PTHREAD_HEADER_FOUND)
if(MINI_XML_PTHREAD_HEADER_FOUND)
    find_package(Threads REQUIRED)
endif()

set(MINI_XML_CONFIG_HEADER
    "#ifndef MXML_CONFIG_H\n#define MXML_CONFIG_H\n\n#define MXML_VERSION \"Mini-XML v${MINI_XML_VERSION}\"\n"
)
if(MINI_XML_PTHREAD_HEADER_FOUND AND CMAKE_USE_PTHREADS_INIT)
    string(APPEND MINI_XML_CONFIG_HEADER "#define HAVE_PTHREAD_H 1\n")
endif()
string(APPEND MINI_XML_CONFIG_HEADER "\n#endif\n")
file(MAKE_DIRECTORY "${mxml_BINARY_DIR}")
file(WRITE "${mxml_BINARY_DIR}/config.h" "${MINI_XML_CONFIG_HEADER}")

add_library(mxml STATIC
    "${mxml_SOURCE_DIR}/mxml-attr.c"
    "${mxml_SOURCE_DIR}/mxml-file.c"
    "${mxml_SOURCE_DIR}/mxml-get.c"
    "${mxml_SOURCE_DIR}/mxml-index.c"
    "${mxml_SOURCE_DIR}/mxml-node.c"
    "${mxml_SOURCE_DIR}/mxml-options.c"
    "${mxml_SOURCE_DIR}/mxml-search.c"
    "${mxml_SOURCE_DIR}/mxml-set.c"
    "${mxml_SOURCE_DIR}/mxml-private.c"
)
target_include_directories(mxml
    PUBLIC "${mxml_SOURCE_DIR}"
    PRIVATE "${mxml_BINARY_DIR}"
)
target_compile_features(mxml PUBLIC c_std_99)
if(MINI_XML_PTHREAD_HEADER_FOUND AND CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(mxml PUBLIC Threads::Threads)
endif()
# Our Project
add_executable(${PROJECT_NAME})
add_subdirectory(src)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

if ("${PLATFORM}" STREQUAL "Web")
    add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/../resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/engine/prefabs $<TARGET_FILE_DIR:${PROJECT_NAME}>/../prefabs
    )
    #DEPENDS ${PROJECT_NAME}
else()
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/engine/prefabs $<TARGET_FILE_DIR:${PROJECT_NAME}>/prefabs
    )
    #DEPENDS ${PROJECT_NAME}
endif()

#set(raylib_VERBOSE 1)
target_link_libraries(${PROJECT_NAME} raylib)
target_link_libraries(${PROJECT_NAME} cjson)
target_link_libraries(${PROJECT_NAME} mxml)

# Web Configurations
if ("${PLATFORM}" STREQUAL "Web")
    # Tell Emscripten to build an example.html file.
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PUBLIC -sUSE_GLFW=3 PUBLIC --preload-file resources PUBLIC --preload-file prefabs)
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()
