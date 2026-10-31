add_library(vostudio_engine STATIC
    src/Analysis/PitchDetector.cpp
    src/AudioBuffer.cpp
    src/Diagnostics/AudioQualityAnalyzer.cpp
    src/DSP/CrossfadeProcessor.cpp
    src/DSP/FormantProcessor.cpp
    src/DSP/PSOLAPitchShifter.cpp
    src/DSP/TimeStretcher.cpp
    src/HybridSynthesisEngine.cpp
    src/PitchCurve.cpp
    src/WavFile.cpp
)

add_library(VoStudio::Engine ALIAS vostudio_engine)

target_include_directories(vostudio_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(vostudio_engine PUBLIC cxx_std_20)
set_target_properties(vostudio_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(vostudio_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(vostudio_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
