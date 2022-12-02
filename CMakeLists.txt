cmake_minimum_required(VERSION 3.10)

project(qjsonrpc LANGUAGES CXX)

find_package(Qt5 COMPONENTS Core Network REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(private_headers
	src/qjsonrpcservice_p.h
	src/qjsonrpcsocket_p.h
	src/qjsonrpcabstractserver_p.h
	src/qjsonrpcservicereply_p.h
#	src/qjsonrpchttpserver_p.h
)

set(install_headers
	src/qjsonrpcmessage.h
	src/qjsonrpcservice.h
	src/qjsonrpcsocket.h
	src/qjsonrpcserviceprovider.h
	src/qjsonrpcabstractserver.h
	src/qjsonrpclocalserver.h
	src/qjsonrpctcpserver.h
	src/qjsonrpcglobal.h
	src/qjsonrpcservicereply.h
	src/qjsonrpchttpclient.h
#	src/qjsonrpchttpserver.h
)

set(sources
	src/qjsonrpcmessage.cpp
	src/qjsonrpcservice.cpp
	src/qjsonrpcsocket.cpp
	src/qjsonrpcserviceprovider.cpp
	src/qjsonrpcabstractserver.cpp
	src/qjsonrpclocalserver.cpp
	src/qjsonrpctcpserver.cpp
	src/qjsonrpcservicereply.cpp
	src/qjsonrpchttpclient.cpp
#	src/qjsonrpchttpserver.cpp
)

add_library(qjsonrpc ${sources} ${install_headers} ${private_headers})

target_link_libraries(qjsonrpc Qt5::Core Qt5::Network)

set_target_properties(qjsonrpc PROPERTIES VERSION 1.1.0 SOVERSION 1.1.0)

install(TARGETS qjsonrpc DESTINATION lib)
install(FILES ${install_headers} DESTINATION include/qjsonrpc)

