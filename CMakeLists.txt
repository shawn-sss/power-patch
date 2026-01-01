cmake_minimum_required(VERSION 3.21)

project(PowerPatch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    src/main.cpp
    src/m365_update/m365_update.cpp
    src/store_update/store_update.cpp
    src/windows_update/windows_update.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)
target_include_directories(${PROJECT_NAME} PRIVATE src)

qt_add_resources(${PROJECT_NAME} app_resources
    PREFIX "/icons"
    FILES
        assets/powerpatch_icon_1024.png
        assets/powerpatch_icon_2048.png
        assets/powerpatch_master.png
)

if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/app_icon.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE Shell32 Ole32 OleAut32 UIAutomationCore)
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
