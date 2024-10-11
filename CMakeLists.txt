# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/common/posix_compat"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/corePKCS11"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/coreMQTT"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/coreMQTT-Agent"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/coreJSON"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/coreHTTP"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/coreMQTT"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/backoffAlgorithm"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/device-shadow-for-aws-iot-embedded-sdk"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/jobs-for-aws-iot-embedded-sdk"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/Fleet-Provisioning-for-AWS-IoT-embedded-sdk"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries/device-defender-for-aws-iot-embedded-sdk"
    "${CMAKE_CURRENT_LIST_DIR}/esp/esp-idf-lib/components"
    )
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(aws-iot-esp)