add_executable(vostudio_engine_tests
    test_main.cpp
)

target_link_libraries(vostudio_engine_tests PRIVATE VoStudio::Engine)
target_compile_features(vostudio_engine_tests PRIVATE cxx_std_20)

add_test(NAME VoStudio.Engine.Tests COMMAND vostudio_engine_tests)
