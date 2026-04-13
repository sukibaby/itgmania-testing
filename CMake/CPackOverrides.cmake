if(CPACK_GENERATOR STREQUAL "NSIS")
  # NSIS absolutely can't handle any backticks and needs the registry key manually set.
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "ITGmania")
  set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "ITGmania")
elseif(CPACK_GENERATOR STREQUAL "WIX")
  # WiX needs this to enable a non-Program Files install location. The OS handles the registry key etc automatically via the standard Windows install framework.
  set(CPACK_WIX_SKIP_PROGRAM_FOLDER ON)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "C:/Games/ITGmania")
elseif(CPACK_GENERATOR STREQUAL "AppImage")
  # The project does not currently install AppStream metadata, so skip that validation.
  set(CPACK_APPIMAGE_NO_APPSTREAM ON)
  set(CPACK_APPIMAGE_DESKTOP_FILE "share/applications/itgmania.desktop")
  set(CPACK_PACKAGE_ICON "itgmania")
  set(CPACK_INSTALL_SCRIPTS "${CPACK_APPIMAGE_DEPENDENCIES_SCRIPT}")
endif()
