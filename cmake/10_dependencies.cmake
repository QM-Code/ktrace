find_package(spdlog CONFIG REQUIRED)
find_package(KcliSDK CONFIG REQUIRED)

# ktools local-build layout guard:
#
# Keep the core build layout compatible with kbuild demo discovery:
# build/<slot>/installed/<triplet>
if(DEFINED VCPKG_INSTALLED_DIR AND
   DEFINED VCPKG_TARGET_TRIPLET AND
   NOT VCPKG_TARGET_TRIPLET STREQUAL "")
    file(MAKE_DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()
