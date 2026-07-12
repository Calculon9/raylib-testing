set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



# Find active runtime source files relative to this folder
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.c)

# Keep deprecated and generated trees out of the build surface.
list(FILTER SOURCE_FILES EXCLUDE REGEX "/DEPRECATED/")
list(FILTER SOURCE_FILES EXCLUDE REGEX "/CMakeFiles/")

# ui_panel.c is an in-progress split of ui_system.c and currently duplicates
# global symbols and functions that still live in ui_system.c.
#list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_panel.c")

# Add the sources to the target defined in the root
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

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