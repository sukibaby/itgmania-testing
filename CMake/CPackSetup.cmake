set(CPACK_PACKAGE_NAME "${SM_EXE_NAME}")
set(CPACK_PACKAGE_VENDOR "${SM_EXE_NAME} Developers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Rhythm Game Simulator")
set(CPACK_PACKAGE_VERSION_MAJOR "${SM_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${SM_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${SM_VERSION_PATCH}")
if(WITH_FULL_RELEASE)
  set(CPACK_PACKAGE_VERSION "${SM_VERSION_TRADITIONAL}")
else()
  set(CPACK_PACKAGE_VERSION "${SM_VERSION_GIT}")
endif()
set(CPACK_PACKAGE_EXECUTABLES "${SM_EXE_NAME}" "ITGmania")
set(CPACK_RESOURCE_FILE_README "${SM_ROOT_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${SM_CMAKE_DIR}/license_install.txt")

if(WIN32)
  set(CPACK_SYSTEM_NAME "Windows")
  # Generate both NSIS (EXE) and WIX (MSI) installers
  set(CPACK_GENERATOR "NSIS;WIX")

  # NSIS (EXE) configuration:
  # By setting these install keys manually, The default directory of "StepMania
  # major.minor.patch" is lost. This is currently done to maintain backwards
  # compatibility. However, removing these two will allow for multiple versions
  # of StepMania to be installed relatively cleanly.
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "ITGmania")
  set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "ITGmania")
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY "Program")
  set(CPACK_NSIS_INSTALL_ROOT "C:\\\\Games")

  set(CPACK_NSIS_HELP_LINK "https://github.com/itgmania/itgmania/issues")
  set(CPACK_NSIS_PACKAGE_NAME "${SM_EXE_NAME} ${CPACK_PACKAGE_VERSION}")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://www.itgmania.com/")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON "${SM_INSTALLER_DIR}/nsis/install.ico")
  set(CPACK_NSIS_MUI_UNIICON "${SM_INSTALLER_DIR}/nsis/uninstall.ico")
  set(CPACK_NSIS_MUI_HEADERIMAGE "${SM_INSTALLER_DIR}/nsis/header.bmp")
  set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${SM_INSTALLER_DIR}/nsis/welcome.bmp")
  set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")
  set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${SM_EXE_NAME}.exe")
  set(CPACK_NSIS_BRANDING_TEXT " ")

  # The header and welcome bitmaps require backslashes.
  string(REGEX
         REPLACE "/"
                 "\\\\\\\\"
                 CPACK_NSIS_MUI_HEADERIMAGE
                 "${CPACK_NSIS_MUI_HEADERIMAGE}")
  string(REGEX
         REPLACE "/"
                 "\\\\\\\\"
                 CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP
                 "${CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP}")

  # Custom items for NSIS go here.
  set(CPACK_SM_NSIS_PRODUCT_ID "ITGmania")
  set(CPACK_SM_NSIS_PRODUCT_VERSION "${SM_VERSION_TRADITIONAL}.0")
  set(CPACK_SM_NSIS_GIT_VERSION "${SM_VERSION_GIT}")

  # WiX (MSI) configuration:
  # https://cmake.org/cmake/help/latest/cpack_gen/wix.html
  # WiX represents the version in the x.x.x.x format, so we append a .0 
  set(CPACK_PACKAGE_VERSION
      "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.0")
  # The GUID is meant to be static; do not change it!
  set(CPACK_WIX_UPGRADE_GUID "a1dcf4ac-e756-4625-8b53-2584e8b7a69a")
  set(CPACK_WIX_SKIP_PROGRAM_FOLDER ON)
  set(INSTALL_ROOT "C:/Games/ITGmania")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "${INSTALL_ROOT}")
  # WixUI_InstallDir is the template used as the basis for this
  # installer. It provides a typical MSI installer interface.
  # The user experience is identical to that of the NSIS installer
  # in exe format as traditionally used in SM5/ITGm.
  set(CPACK_WIX_UI_REF "WixUI_InstallDir")
  set(CPACK_WIX_PRODUCT_ICON "${SM_INSTALLER_DIR}/wix/install.ico")
  set(CPACK_WIX_UI_BANNER "${SM_INSTALLER_DIR}/wix/header.bmp")
  set(CPACK_WIX_UI_DIALOG "${SM_INSTALLER_DIR}/wix/welcome.bmp")
  set(CPACK_WIX_PROGRAM_MENU_FOLDER "ITGmania")
  set(CPACK_WIX_HELP_LINK "https://github.com/itgmania/itgmania/issues")
  set(CPACK_WIX_PRODUCT_NAME "${SM_EXE_NAME} ${CPACK_PACKAGE_VERSION}")
  set(CPACK_WIX_URL_INFO_ABOUT "https://www.itgmania.com/")
  set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://www.itgmania.com/")
elseif(MACOSX)
  set(CPACK_GENERATOR DragNDrop)
  set(CPACK_DMG_VOLUME_NAME "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}")
  set(CPACK_DMG_FORMAT ULMO)  # lzma-compressed image

  if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(CPACK_SYSTEM_NAME "macOS-M1")
  elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(CPACK_SYSTEM_NAME "macOS-Intel")
  else()
    message(FATAL_ERROR
      "Unsupported macOS architecture: ${CMAKE_OSX_ARCHITECTURES}, set CMAKE_OSX_ARCHITECTURES to either arm64 or x86_64"
    )
  endif()
else()
  set(CPACK_GENERATOR TGZ)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(CPACK_SYSTEM_NAME "Linux")
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(CPACK_SYSTEM_NAME "Linux-arm64")
  else()
    message("Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_SYSTEM_NAME "Linux-unknown")
  endif()
endif()

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
if(NOT WITH_CLUB_FANTASTIC)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-no-songs")
endif()

include(CPack)
