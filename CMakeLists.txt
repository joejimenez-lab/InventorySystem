cmake_minimum_required(VERSION 3.15)
project(Project)

# Include vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(JSONCPP_LIBRARY "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-mingw-static/lib/libjsoncpp.a")
set(CPR_LIBRARY "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-mingw-static/lib/libcpr.a")
set(CURL_LIBRARY "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-mingw-static/lib/libcurl.a")

# Define the executable
add_executable(Project src/main.cpp src/user.cpp)

# Find packages
find_package(asio REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(curl REQUIRED)

# Link Crow and other libraries
target_link_libraries(Project PRIVATE Crow::Crow)
if (WIN32)
    target_link_libraries(Project PRIVATE ws2_32 mswsock)
endif()
target_link_libraries(Project PRIVATE odbc32)

# Specify the directory where the Argon2 static library is located
link_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2")

# Include Argon2 header files
include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/include")

# Link Argon2 static library
target_link_libraries(Project PRIVATE "${CMAKE_SOURCE_DIR}/phc-winner-argon2/libargon2.a")

# target_link_libraries(Project PRIVATE cpr::cpr)
# target_link_libraries(Project PRIVATE CURL::libcurl)

target_link_libraries(Project PRIVATE asio::asio)


target_link_libraries(Project PRIVATE ${JSONCPP_LIBRARY})
target_link_libraries(Project PRIVATE ${CPR_LIBRARY})
target_link_libraries(Project PRIVATE ${CURL_LIBRARY})