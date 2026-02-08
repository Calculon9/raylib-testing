# Find all files relative to this folder
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.c)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS *.h)

# Add the sources to the target defined in the root
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

# Tell the target where the root include folder is
# We use PUBLIC if we want Raylib or other modules to potentially see our types
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")