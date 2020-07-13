include_directories(
    ${PULSE_INCLUDE_DIRS}
)

set(pavucontrol-qt_HDRS
    pavucontrol.h
    mainwindow.h
    cardwidget.h
    channel.h
    devicewidget.h
    minimalstreamwidget.h
    rolewidget.h
    playbackwidget.h
    outputwidget.h
    sourceoutputwidget.h
    sourcewidget.h
    streamwidget.h
    wavplay.h
)

set(pavucontrol-qt_SRCS
    pavucontrol.cc
    mainwindow.cc
    cardwidget.cc
    channel.cc
    devicewidget.cc
    minimalstreamwidget.cc
    rolewidget.cc
    playbackwidget.cc
    outputwidget.cc
    sourceoutputwidget.cc
    sourcewidget.cc
    streamwidget.cc
    wavplay.cc
)

add_executable(pavucontrol-qt
    ../data.qrc
    ${pavucontrol-qt_SRCS}
    ${pavucontrol-qt_QM_FILES}
    ${pavucontrol-qt_QM_LOADER}
    ${DESKTOP_FILES}
)
set_property(
     TARGET pavucontrol-qt APPEND
     PROPERTY COMPILE_DEFINITIONS
     PAVUCONTROL_QT_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}"
)

target_link_libraries(pavucontrol-qt
    Qt5::Widgets
    ${PULSE_LDFLAGS}
)

install(TARGETS
    pavucontrol-qt
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Runtime
)

install(FILES
    ${DESKTOP_FILES}
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    COMPONENT Runtime
)
