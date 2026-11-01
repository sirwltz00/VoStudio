add_executable(vostudio-render main.cpp)
target_link_libraries(vostudio-render PRIVATE VoStudio::Engine)
target_compile_features(vostudio-render PRIVATE cxx_std_20)
