cmake_minimum_required(VERSION 3.10)
project(ReVision)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable CMake integration with vcpkg
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

# Find packages
find_package(OpenCV REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)

# Set the source file
set(SOURCE_FILES src/ReVision.cpp)

# Add the executable
add_executable(ReVision ${SOURCE_FILES})

# Link OpenCV libraries
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(ReVision PRIVATE ${OpenCV_LIBS} nlohmann_json::nlohmann_json ZLIB::ZLIB Freetype::Freetype)

# Set output directory for the executable
set_target_properties(ReVision PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../build")

# Copy image.jpg to the output target directory after build
add_custom_command(TARGET ReVision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:ReVision>/res"
)
