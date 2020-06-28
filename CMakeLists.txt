cmake_minimum_required(VERSION 3.9)

project(redistricter LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(Proj REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

set(SRCDIR cpp)

set(CORESRCS
  ${SRCDIR}/fileio.cpp
  ${SRCDIR}/Bitmap.cpp
  ${SRCDIR}/mmaped.cpp
  ${SRCDIR}/Solver.cpp
  ${SRCDIR}/District2.cpp
  ${SRCDIR}/PreThread.cpp
  ${SRCDIR}/renderDistricts.cpp
  ${SRCDIR}/LinearInterpolate.cpp
  ${SRCDIR}/GrabIntermediateStorage.cpp
  ${SRCDIR}/AbstractDistrict.cpp
  ${SRCDIR}/DistrictSet.cpp
  ${SRCDIR}/NearestNeighborDistrictSet.cpp
  ${SRCDIR}/protoio.cpp
  ${SRCDIR}/StatThing.cpp
  ${SRCDIR}/uf1.cpp
  ${SRCDIR}/logging.cpp
  ${SRCDIR}/BinaryStatLogger.cpp
  ${SRCDIR}/CountyCityDistricter.cpp
  ${SRCDIR}/placefile.cpp
)

PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders redata.proto)
add_library(redata ${ProtoSources} ${ProtoHeaders})
target_include_directories(redata PUBLIC ${CMAKE_BINARY_DIR})

message(STATUS ${ProtoHeaders})

add_executable(districter2 ${CORESRCS} ${SRCDIR}/nonguimain.cpp)
target_link_libraries(districter2 PRIVATE redata PNG::PNG Threads::Threads protobuf)

add_executable(linkfixup ${CORESRCS} ${SRCDIR}/linkfixup.cpp)
target_link_libraries(linkfixup PRIVATE proj redata Threads::Threads PNG::PNG protobuf)

add_executable(drend ${CORESRCS} ${SRCDIR}/drendmain.cpp ${SRCDIR}/MapDrawer.cpp)
target_link_libraries(drend PRIVATE redata PNG::PNG Threads::Threads protobuf)

add_executable(analyze ${CORESRCS} ${SRCDIR}/analyze.cpp ${SRCDIR}/popSSD.cpp)
target_link_libraries(analyze PRIVATE redata PNG::PNG Threads::Threads protobuf)

add_executable(dumpBinLog ${SRCDIR}/dumpBinLog.cpp)
target_link_libraries(dumpBinLog PRIVATE redata Threads::Threads protobuf)
