cmake_minimum_required(VERSION 3.22)

project(powerpatch VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/startup_guard.cpp
    src/dashboard.cpp
    src/update_service.cpp
    src/m365_update/m365_update.cpp
    src/store_update/store_update.cpp
    src/windows_update/windows_update.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Concurrent)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_definitions(${PROJECT_NAME} PRIVATE POWERPATCH_VERSION="${PROJECT_VERSION}")

qt_add_resources(${PROJECT_NAME} app_resources
    PREFIX "/icons"
    FILES
        assets/img/powerpatch_1024.png
        assets/img/powerpatch_2048.png
        assets/img/powerpatch.png
        assets/img/check_dark.png
        assets/img/check_light.png
)

if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/app_resources.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE Shell32 Ole32 OleAut32 UIAutomationCore)
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
    if(WIN32)
        qt_add_executable(startup_probe tests/startup_probe.cpp src/startup_guard.cpp)
        target_include_directories(startup_probe PRIVATE src)
        target_link_libraries(startup_probe PRIVATE Qt6::Core)
        qt_add_executable(startup_checks tests/startup_checks.cpp)
        target_link_libraries(startup_checks PRIVATE Qt6::Core)
        add_dependencies(startup_checks startup_probe)
        add_test(NAME startup_checks COMMAND startup_checks)
        set_tests_properties(startup_checks PROPERTIES TIMEOUT 30
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
    qt_add_executable(dashboard_checks
        tests/dashboard_checks.cpp
        src/dashboard.cpp
        src/update_service.cpp
        src/m365_update/m365_update.cpp
        src/store_update/store_update.cpp
        src/windows_update/windows_update.cpp
    )
    target_include_directories(dashboard_checks PRIVATE src)
    target_link_libraries(dashboard_checks PRIVATE Qt6::Widgets Qt6::Concurrent)
    if(WIN32)
        target_link_libraries(dashboard_checks PRIVATE Shell32 Ole32 OleAut32 UIAutomationCore)
    endif()
    qt_add_resources(dashboard_checks test_resources
        PREFIX "/icons"
        FILES assets/img/powerpatch_1024.png assets/img/check_dark.png
    )
    add_test(NAME dashboard_checks COMMAND dashboard_checks "${CMAKE_BINARY_DIR}/previews")
    add_test(NAME dashboard_checks_150 COMMAND dashboard_checks "${CMAKE_BINARY_DIR}/previews-150")
    add_test(NAME dashboard_checks_200 COMMAND dashboard_checks "${CMAKE_BINARY_DIR}/previews-200")
    set_tests_properties(dashboard_checks dashboard_checks_150 dashboard_checks_200 PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )
    if(WIN32)
        set_tests_properties(dashboard_checks dashboard_checks_150 dashboard_checks_200 PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
    set_property(TEST dashboard_checks_150 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1.5")
    set_property(TEST dashboard_checks_200 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=2")
endif()
