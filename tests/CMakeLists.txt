add_executable(test_mutils test_mutils.cpp)
target_link_libraries(test_mutils PRIVATE mutils)
add_test(NAME test_mutils COMMAND test_mutils)
