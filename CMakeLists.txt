cmake_minimum_required(VERSION 3.12)

# set the project name
project(weather)

find_package( CURL REQUIRED )
pkg_check_modules(JSON REQUIRED json-c)
include_directories( ${CURL_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS})

# add the executable
add_executable(weather weather.c)


target_link_libraries( weather ${CURL_LIBRARIES} ${JSON_LIBRARIES} )
