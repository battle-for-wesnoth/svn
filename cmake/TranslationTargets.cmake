MACRO (ADD_POT_TARGET DOMAIN)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/remove-potcdate.sed
                     COMMAND sed
                     ARGS -e "'/^#/d'" remove-potcdate.sin > remove-potcdate.sed
                     MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/remove-potcdate.sin
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  set(POTFILE ${DOMAIN}.pot)

  add_custom_target(update-pot-${DOMAIN}
                    COMMAND ${CMAKE_BINARY_DIR}/po/pot-update.sh ${DOMAIN}
                    DEPENDS remove-potcdate.sed
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#  add_custom_command(OUTPUT ${POTFILE}
#                    COMMAND ${CMAKE_SOURCE_DIR}/po/pot-update.sh ${DOMAIN}
#                    DEPENDS remove-potcdate.sed
#                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#  add_custom_target(update-pot-${DOMAIN} DEPENDS ${POTFILE})

  set_source_files_properties(${POTFILE} PROPERTIES GENERATED TRUE)
  add_dependencies(update-pot update-pot-${DOMAIN})

ENDMACRO (ADD_POT_TARGET DOMAIN)


MACRO (ADD_PO_TARGETS DOMAIN)
if(GETTEXT_MSGMERGE_EXECUTABLE AND GETTEXT_MSGINIT_EXECUTABLE)
  set(LINGUAS ${ARGN})

  add_custom_target(update-po-${DOMAIN})

  foreach(LANG ${LINGUAS})
    set(POFILE ${LANG}.po)
    set(UPDFILE ${LANG}.upd)

    add_custom_target(update-po-${DOMAIN}-${LANG}
                      COMMAND ${CMAKE_SOURCE_DIR}/po/po-update.sh 
                      ${DOMAIN} ${LANG} ${GETTEXT_MSGMERGE_EXECUTABLE} ${GETTEXT_MSGINIT_EXECUTABLE} 
                      DEPENDS ${DOMAIN}.pot
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_dependencies(update-po-${DOMAIN} update-po-${DOMAIN}-${LANG})
    #add_dependencies(update-po-${DOMAIN}-${LANG} )
 
    set_source_files_properties(${POFILE} PROPERTIES GENERATED TRUE)
  endforeach(LANG ${LINGUAS})

  add_dependencies(update-po update-po-${DOMAIN})

endif(GETTEXT_MSGMERGE_EXECUTABLE AND GETTEXT_MSGINIT_EXECUTABLE)
ENDMACRO (ADD_PO_TARGETS DOMAIN)


MACRO (ADD_MO_TARGETS DOMAIN)
if(GETTEXT_MSGFMT_EXECUTABLE)

  set(LINGUAS ${ARGN})
  set(MOFILES)

  foreach(LANG ${LINGUAS})
    set(POFILE ${LANG}.po)
    set(MOFILE ${CMAKE_CURRENT_BINARY_DIR}/${LANG}.gmo)
    set_source_files_properties(${POFILE} PROPERTIES GENERATED TRUE)

    add_custom_command(OUTPUT ${MOFILE} 
                       COMMAND ${GETTEXT_MSGFMT_EXECUTABLE}
                       ARGS -c -o ${MOFILE} ${POFILE} 
                       MAIN_DEPENDENCY ${POFILE}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MOFILE} DESTINATION ${LOCALE_INSTALL}/${LANG}/LC_MESSAGES/ RENAME ${DOMAIN}.mo)

    set(MOFILES ${MOFILES} ${MOFILE})

  endforeach(LANG ${LINGUAS})

  add_custom_target(update-gmo-${DOMAIN} ALL DEPENDS ${MOFILES})
  add_dependencies(update-gmo update-gmo-${DOMAIN})

endif(GETTEXT_MSGFMT_EXECUTABLE)
ENDMACRO (ADD_MO_TARGETS DOMAIN)
