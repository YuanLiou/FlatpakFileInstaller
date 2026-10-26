# SPDX-License-Identifier: Apache-2.0

# Compiles po/<lang>.po into <lang>/LC_MESSAGES/<domain>.mo for every
# language in LINGUAS. Run scripts/update-translations.sh to refresh the
# template and the .po files after changing user-visible text.
file(STRINGS LINGUAS LINGUAS REGEX "^[^#]")

set(MO_FILES)
foreach(lang IN LISTS LINGUAS)
    set(mo ${CMAKE_CURRENT_BINARY_DIR}/${lang}/LC_MESSAGES/${TRANSLATION_DOMAIN}.mo)
    add_custom_command(
        OUTPUT ${mo}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${lang}/LC_MESSAGES
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${mo} ${CMAKE_CURRENT_SOURCE_DIR}/${lang}.po
        DEPENDS ${lang}.po
    )
    list(APPEND MO_FILES ${mo})
    install(FILES ${mo} DESTINATION ${KDE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES)
endforeach()

add_custom_target(translations ALL DEPENDS ${MO_FILES})
