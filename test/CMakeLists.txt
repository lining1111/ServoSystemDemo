LIST(TRANSFORM SRC_FILES PREPEND ../)

add_executable(test_com test_com.cpp
        ../utils/utils.cpp)
target_link_libraries(test_com ${LIBS})

add_executable(test_websocketServer test_websocketServer.cpp
        ${SRC_FILES})
target_link_libraries(test_websocketServer ${LIBS})

add_executable(test_websocketClient test_websocketClient.cpp
        ${SRC_FILES})
target_link_libraries(test_websocketClient ${LIBS})