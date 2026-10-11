cmake_minimum_required(VERSION 3.16)
project(StockSenseAI VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STOCKSENSE_BUILD_TESTS "Build unit tests" ON)
option(STOCKSENSE_USE_POSTGRES "Enable PostgreSQL backend" ON)

include(FetchContent)

# ---------- Asio (header-only via ZIP — avoids git/subbuild failures on MSVC) ----------
FetchContent_Declare(
  asio
  URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
  FetchContent_Populate(asio)
endif()
# Zip extracts to asio-asio-1-30-2/ (repo-name-tag)
if(EXISTS "${asio_SOURCE_DIR}/asio/include")
  set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)
else()
  set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)
endif()
set(asio_FOUND TRUE CACHE BOOL "" FORCE)
message(STATUS "Asio include: ${ASIO_INCLUDE_DIR}")

# ---------- Crow ----------
FetchContent_Declare(
  crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0
  GIT_SHALLOW    TRUE
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_USE_BOOST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(crow)

# ---------- nlohmann/json ----------
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# ---------- spdlog ----------
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.14.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# ---------- libpqxx (PostgreSQL C++ client) ----------
if(STOCKSENSE_USE_POSTGRES)
  find_package(PostgreSQL REQUIRED)
  FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG        7.9.2
    GIT_SHALLOW    TRUE
  )
  set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
  set(BUILD_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(libpqxx)
endif()

# ---------- Sources ----------
set(STOCKSENSE_SOURCES
  src/main.cpp
  src/services/ConfigService.cpp
  src/services/MarketDataIngester.cpp
  src/services/IndicatorEngine.cpp
  src/services/TelegramNotifier.cpp
  src/services/AlertEngine.cpp
  src/services/MlScorer.cpp
  src/services/ExplanationService.cpp
  src/api/HealthController.cpp
  src/api/StockController.cpp
  src/api/IngestController.cpp
  src/api/IndicatorController.cpp
  src/api/AlertController.cpp
  src/api/DashboardController.cpp
)

if(STOCKSENSE_USE_POSTGRES)
  list(APPEND STOCKSENSE_SOURCES
    src/db/Database.cpp
    src/repositories/StockRepository.cpp
  )
endif()

add_executable(stocksense_api ${STOCKSENSE_SOURCES})

target_include_directories(stocksense_api PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${ASIO_INCLUDE_DIR}
)

target_link_libraries(stocksense_api PRIVATE
  Crow::Crow
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)

target_compile_definitions(stocksense_api PRIVATE
  ASIO_STANDALONE
  _WIN32_WINNT=0x0A00
)

if(STOCKSENSE_USE_POSTGRES)
  target_link_libraries(stocksense_api PRIVATE pqxx PostgreSQL::PostgreSQL)
  target_compile_definitions(stocksense_api PRIVATE STOCKSENSE_HAS_POSTGRES=1)
endif()

if(MSVC)
  target_compile_options(stocksense_api PRIVATE /W4 /bigobj)
else()
  target_compile_options(stocksense_api PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------- Tests ----------
if(STOCKSENSE_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(stocksense_tests
    tests/test_main.cpp
    tests/test_health.cpp
  )
  target_include_directories(stocksense_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(stocksense_tests PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
  )
  include(GoogleTest)
  gtest_discover_tests(stocksense_tests)
endif()

install(TARGETS stocksense_api RUNTIME DESTINATION bin)
