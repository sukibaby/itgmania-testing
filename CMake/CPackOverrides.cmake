if(CPACK_GENERATOR STREQUAL "NSIS")
  # NSIS absolutely can't handle any backticks and needs the registry key manually set.
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "ITGmania")
  set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "ITGmania")
elseif(CPACK_GENERATOR STREQUAL "WIX")
  # WiX needs this to enable a non-Program Files install location. The OS handles the registry key etc automatically via the standard Windows install framework.
  set(CPACK_WIX_SKIP_PROGRAM_FOLDER ON)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "C:/Games/ITGmania")
endif()
