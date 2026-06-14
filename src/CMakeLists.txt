set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



# Find all files relative to this folder
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.c)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS *.h)

# Add the sources to the target defined in the root
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

# Tell the target where the root include folder is
# We use PUBLIC if we want Raylib or other modules to potentially see our types
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

# Force CMake to pass -DDEBUG when building in Debug mode
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug) # Fallback default if not specified
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(raylib-game PRIVATE DEBUG)
    message(STATUS "Build target configured with DEBUG logging active.")
endif()