# SPDX-License-Identifier: Apache-2.0
include(ECMAddTests)

ecm_add_tests(
    packagefiletest.cpp
    hostenvironmenttest.cpp
    hostcommandtest.cpp
    permissionstest.cpp
    flathubclienttest.cpp
    existinginstalltest.cpp
    LINK_LIBRARIES installer_core Qt6::Test
)

ecm_add_test(installtest.cpp
    TEST_NAME installtest
    LINK_LIBRARIES installer_core Qt6::Test
)

foreach(test packagefiletest permissionstest installtest)
    target_compile_definitions(${test} PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
endforeach()
