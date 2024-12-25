LIST(TRANSFORM SRC_FILES PREPEND ../)

add_executable(test_com test_com.cpp
../utils/utils.cpp)
target_link_libraries(test_com ${LIBS})