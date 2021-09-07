add_executable(autoapp
        autoapp.cpp
        UI/connectdialog.ui
        UI/ConnectDialog.cpp
        UI/mainwindow.ui
        UI/MainWindow.cpp
        UI/settingswindow.ui
        UI/SettingsWindow.cpp
        assets/resources.qrc
        ${CMAKE_SOURCE_DIR}/include/autoapp/UI/ConnectDialog.hpp
        ${CMAKE_SOURCE_DIR}/include/autoapp/UI/MainWindow.hpp
        ${CMAKE_SOURCE_DIR}/include/autoapp/UI/SettingsWindow.hpp
        )

target_include_directories(autoapp PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
        )

target_link_libraries(autoapp
        openauto
        )

if(RPI_BUILD AND NOT GST_BUILD)
    target_link_libraries(autoapp omx)
endif()

set_target_properties(autoapp
        PROPERTIES INSTALL_RPATH_USE_LINK_PATH 1)

install(TARGETS autoapp
        RUNTIME DESTINATION bin)
