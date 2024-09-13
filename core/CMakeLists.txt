set(CORE_TARGET nipovpn)
set(CMAKE_CXX_STANDARD 20)

set(CORE_TARGET_HEADERS
        src/agenthandler.hpp
        src/config.hpp
        src/general.hpp
        src/http.hpp
        src/log.hpp
        src/runner.hpp
        src/serverhandler.hpp
        src/tcpclient.hpp
        src/tcpconnection.hpp
        src/tcpserver.hpp
)

set(CORE_TARGET_SOURCES
        src/agenthandler.cpp
        src/config.cpp
        src/http.cpp
        src/log.cpp
        src/runner.cpp
        src/serverhandler.cpp
        src/tcpclient.cpp
        src/tcpconnection.cpp
        src/tcpserver.cpp
)


add_executable(${CORE_TARGET} src/main.cpp ${CORE_TARGET_SOURCES})

# Add directories
target_include_directories(${CORE_TARGET} PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(${CORE_TARGET} PUBLIC  ${YAML_CPP_INCLUDE_DIR})
target_include_directories(${CORE_TARGET} PUBLIC  ${OPENSSL_INCLUDE_DIR})

# linking dependencies
target_link_libraries(${CORE_TARGET} ${Boost_LIBRARIES})
target_link_libraries(${CORE_TARGET} ${YAML_CPP_LIBRARIES})
target_link_libraries(${CORE_TARGET} ${OPENSSL_LIBRARIES})
