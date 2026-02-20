# Config
cmake_minimum_required( VERSION 3.21 )
add_compile_options(-std=c++20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Project
project(WisdomSite)

add_executable(WisdomSite 
src/index.cpp
)
# Dependencies

#find_package(OpenSSL REQUIRED)
# nginx handles SSL

# It took me like 2 HOURS to get A SINGLE hashing library TO LINK
# I HATE USING LIBRARIES IN C++
# ADD. CMAKE. TO. YOUR. PROJECT. PLEAAASEEEE *cough* *cough* argon2
add_subdirectory(${CMAKE_SOURCE_DIR}/external/libbcrypt)
include_directories(${CMAKE_SOURCE_DIR}/external/libbcrypt/include)

find_package(ZLIB REQUIRED)
find_package(md4c REQUIRED)
find_package(pugixml REQUIRED)
find_package(unofficial-sqlite3 REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)
CPMAddPackage(Crow
  VERSION 1.3.0
  GITHUB_REPOSITORY CrowCpp/Crow
  OPTIONS
  "CROW_BUILD_EXAMPLES Off"
  "CROW_BUILD_TOOLS Off"
  "CROW_BUILD_TESTS Off"
  "CROW_BUILD_DOCS Off"
  "CROW_ENABLE_COMPRESSION On"
  "CROW_ENABLE_SSL Off"
)

target_link_libraries( WisdomSite
  PRIVATE
  #OpenSSL::SSL
  #OpenSSL::Crypto
  ZLIB::ZLIB
  Crow::Crow
  md4c::md4c-html
  bcrypt
  pugixml::pugixml
  unofficial::sqlite3::sqlite3
)
