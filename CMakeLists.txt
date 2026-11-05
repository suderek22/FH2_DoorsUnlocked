cmake_minimum_required(VERSION 3.20)
project(DoorsUnlocked VERSION 1.0.0 LANGUAGES CXX RC)

add_library(DoorsUnlocked SHARED DoorsUnlocked.cpp DoorsUnlocked.rc)
target_compile_features(DoorsUnlocked PRIVATE cxx_std_17)
target_include_directories(DoorsUnlocked PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(DoorsUnlocked PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(DoorsUnlocked PRIVATE xinput)
if(MSVC)
    target_compile_options(DoorsUnlocked PRIVATE /W4 /WX /utf-8)
endif()
set_target_properties(DoorsUnlocked PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" u)
    set_target_properties(DoorsUnlocked PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${u} "${CMAKE_CURRENT_SOURCE_DIR}/dist"
        LIBRARY_OUTPUT_DIRECTORY_${u} "${CMAKE_CURRENT_SOURCE_DIR}/dist")
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(action_tests tests/actions.cpp)
    target_compile_features(action_tests PRIVATE cxx_std_17)
    target_include_directories(action_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME toggle_actions COMMAND action_tests)
    add_executable(settings_tests tests/settings.cpp)
    target_compile_features(settings_tests PRIVATE cxx_std_17)
    target_include_directories(settings_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_definitions(settings_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/settings-test")
    add_test(NAME platform_settings COMMAND settings_tests "$<TARGET_FILE:DoorsUnlocked>"
             "${CMAKE_CURRENT_BINARY_DIR}/settings-test")
    add_test(NAME saved_settings COMMAND settings_tests "$<TARGET_FILE:DoorsUnlocked>"
             "${CMAKE_CURRENT_BINARY_DIR}/saved-settings-test" saved)
    add_test(NAME invalid_settings COMMAND settings_tests "$<TARGET_FILE:DoorsUnlocked>"
             "${CMAKE_CURRENT_BINARY_DIR}/invalid-settings-test" invalid)
endif()
