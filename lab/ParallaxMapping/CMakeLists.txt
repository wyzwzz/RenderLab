
add_executable(PM main.cpp)

target_link_libraries(PM PRIVATE CGUtils)

target_compile_features(PM PRIVATE cxx_std_20)