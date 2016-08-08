cmake_minimum_required(VERSION 2.8.9)

set(PROJECT dvb-hls)
set(FRONTEND_DIR "src/frontend")

project(${PROJECT} CXX)
add_definitions(-std=c++11)
add_definitions(-Wall)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
elseif (CMAKE_BUILD_TYPE MATCHES Debug)
  # Need to force cmake to apply -g
  add_definitions(-g)
endif()
set(CMAKE_CXX_FLAGS_DEBUG -DBUILD_DEBUG)
include_directories("include")

file(GLOB SOURCES "src/backend/*.cpp")
add_executable(${PROJECT} ${SOURCES})
target_link_libraries(${PROJECT} dvbpsi boost_program_options rt)

file(GLOB PHP_SOURCES "${FRONTEND_DIR}/*.php")
install(TARGETS ${PROJECT} DESTINATION bin COMPONENT backend)
install(FILES ${FRONTEND_DIR}/dvb_hls_apache.conf DESTINATION /etc/apache2/sites-available COMPONENT frontend RENAME dvb_hls.conf)
install(FILES ${PHP_SOURCES} DESTINATION /var/www COMPONENT frontend)
