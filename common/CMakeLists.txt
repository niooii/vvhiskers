# Common/shared game logic library

# Find all source files in common
file(GLOB_RECURSE COMMON_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Create the common library
add_library(vlib STATIC ${COMMON_SOURCES})

# Include directories for common library
target_include_directories(vlib
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/extern"
        "${CMAKE_SOURCE_DIR}/extern/hfsm2/include/"
)

# Add imgui as a library (header-only, needs manual setup)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
file(GLOB IMGUI_SOURCES
        ${IMGUI_DIR}/*.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

set(DAXA_ENABLE_UTILS_TASK_GRAPH ON CACHE BOOL "" FORCE)
set(DAXA_ENABLE_UTILS_PIPELINE_MANAGER_GLSLANG ON CACHE BOOL "" FORCE)

add_compile_definitions(DAXA_REMOVE_DEPRECATED=0)

add_subdirectory(
        ${CMAKE_SOURCE_DIR}/extern/daxa
        ${CMAKE_BINARY_DIR}/extern/daxa
)

# Link common dependencies
target_link_libraries(vlib
        PUBLIC
        reflectcpp::reflectcpp
        EnTT::EnTT
        spdlog::spdlog
        Taskflow::Taskflow
        SDL3::SDL3
        glm::glm
		absl::base
		absl::debugging
	 	absl::algorithm
    	absl::synchronization
    	absl::strings
    	absl::failure_signal_handler
        daxa
        imgui
)

# Link Tracy if enabled
if(TRACY_ENABLE)
    target_link_libraries(vlib PUBLIC TracyClient)
endif()

# Link Windows-specific libraries
if(WIN32)
    target_link_libraries(vlib PUBLIC ws2_32 winmm)
endif()

# Set output directories
set_target_properties(vlib
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
