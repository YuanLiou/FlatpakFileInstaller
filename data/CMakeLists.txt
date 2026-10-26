# SPDX-License-Identifier: Apache-2.0

# The .desktop and metainfo files get their translations merged in from po/.
set(PO_DIR ${PROJECT_SOURCE_DIR}/po)
file(STRINGS ${PO_DIR}/LINGUAS LINGUAS REGEX "^[^#]")
list(TRANSFORM LINGUAS PREPEND ${PO_DIR}/ OUTPUT_VARIABLE PO_FILES)
list(TRANSFORM PO_FILES APPEND .po)

set(DESKTOP_FILE ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop)
set(METAINFO_FILE ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.metainfo.xml)

add_custom_command(
    OUTPUT ${DESKTOP_FILE}
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --desktop
        --template=${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop.in
        -d ${PO_DIR} -o ${DESKTOP_FILE}
    DEPENDS ${APP_ID}.desktop.in ${PO_DIR}/LINGUAS ${PO_FILES}
)
add_custom_command(
    OUTPUT ${METAINFO_FILE}
    COMMAND ${CMAKE_COMMAND} -E env GETTEXTDATADIRS=${PO_DIR}
        ${GETTEXT_MSGFMT_EXECUTABLE} --xml
        --template=${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml.in
        -d ${PO_DIR} -o ${METAINFO_FILE}
    DEPENDS ${APP_ID}.metainfo.xml.in ${PO_DIR}/LINGUAS ${PO_FILES} ${PO_DIR}/its/metainfo.its ${PO_DIR}/its/metainfo.loc
)
add_custom_target(data_files ALL DEPENDS ${DESKTOP_FILE} ${METAINFO_FILE})

install(FILES ${DESKTOP_FILE} DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${METAINFO_FILE} DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES icons/${APP_ID}.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

if(BUILD_TESTING)
    find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
    if(DESKTOP_FILE_VALIDATE)
        add_test(NAME validate-desktop-file COMMAND ${DESKTOP_FILE_VALIDATE} ${DESKTOP_FILE})
    endif()
    find_program(APPSTREAMCLI appstreamcli)
    if(APPSTREAMCLI)
        add_test(NAME validate-metainfo COMMAND ${APPSTREAMCLI} validate --no-net --explain ${METAINFO_FILE})
    endif()
endif()
