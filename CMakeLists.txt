# Config
cmake_minimum_required( VERSION 2.8 )
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Project
project(WisdomSite)

add_executable(WisdomSite 
  src/index.cpp
)
# Dependencies

# nginx handles SSL
find_package(SQLite3 REQUIRED)

include(FetchContent)
FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow
  GIT_TAG v1.3.3
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
set(CROW_BUILD_EXAMPLES Off)
set(CROW_BUILD_TOOLS Off)
set(CROW_BUILD_TESTS Off)
set(CROW_BUILD_DOCS Off)
set(CROW_ENABLE_COMPRESSION On)
set(CROW_ENABLE_SSL Off)
FetchContent_MakeAvailable(Crow)

FetchContent_Declare(
  md4c
  GIT_REPOSITORY https://github.com/mity/md4c
  GIT_TAG release-0.5.3
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(md4c)

FetchContent_Declare(
  pugixml
  GIT_REPOSITORY https://github.com/zeux/pugixml
  GIT_TAG v1.16
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(pugixml)

FetchContent_Declare(
  libbcrypt
  GIT_REPOSITORY https://github.com/trusch/libbcrypt
  GIT_TAG master
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(libbcrypt)

FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib
  GIT_TAG v1.3.2
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
  sqlitecpp
  GIT_REPOSITORY https://github.com/srombauts/sqlitecpp
  GIT_TAG 3.3.3
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
set(SQLITECPP_INTERNAL_SQLITE Off)
FetchContent_MakeAvailable(sqlitecpp)

target_link_libraries( WisdomSite
  PRIVATE
  #OpenSSL::SSL
  #OpenSSL::Crypto
  zlib
  Crow
  md4c-html
  bcrypt
  pugixml
  sqlite3
  SQLiteCpp
)
