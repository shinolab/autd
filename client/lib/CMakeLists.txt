cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)

# Eigen3
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/deps)
set(Matlab_INCLUDE_DIRS "")

find_package( Eigen3 REQUIRED )
find_package( Boost REQUIRED )
find_package( Threads REQUIRED )
find_package( MATLAB )

include_directories(../include ${EIGEN3_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${Ads_INCLUDE_DIR} ${Matlab_INCLUDE_DIRS})

add_library(autd3 STATIC
  controller.cpp
  link.cpp
  ethercat_link.cpp
  geometry.cpp
  gain.cpp
  matlabgain.cpp
  holo_gain.cpp
  modulation.cpp
  link.hpp
  ethercat_link.hpp
  privdef.hpp
  ../include/autd3.hpp
)


if (Matlab_FOUND AND Matlab_MX_LIBRARY)
    add_definitions(-DMATLAB_ENABLED)
    target_link_libraries(autd3 ${Matlab_MX_LIBRARY} ${Matlab_MAT_LIBRARY})
endif()
target_link_libraries(autd3 ${Boost_LIBRARIES} ${Ads_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

INSTALL(TARGETS autd3 LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
INSTALL(FILES ../include/autd3.hpp DESTINATION include)