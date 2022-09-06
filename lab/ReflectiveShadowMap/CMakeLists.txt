
add_executable(RSM main.cpp)

target_link_libraries(RSM PRIVATE CGUtils)

target_compile_features(RSM PRIVATE cxx_std_20)