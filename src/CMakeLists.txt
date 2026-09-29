file(GLOB_RECURSE APP_SOURCES "*.cpp" "*.c")
set(LIB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.pio/libdeps/ErsaWearable")
file(GLOB_RECURSE GxEPD2_SOURCES "${LIB_ROOT}/GxEPD2/src/*.cpp")
file(GLOB GFX_SOURCES "${LIB_ROOT}/Adafruit GFX Library/*.cpp" "${LIB_ROOT}/Adafruit GFX Library/*.c")
file(GLOB BUSIO_SOURCES "${LIB_ROOT}/Adafruit BusIO/*.cpp")
file(GLOB_RECURSE RTCLIB_SOURCES "${LIB_ROOT}/RTClib/src/*.cpp")
file(GLOB_RECURSE ONEBUTTON_SOURCES "${LIB_ROOT}/OneButton/src/*.cpp")

idf_component_register(
    SRCS ${APP_SOURCES} ${GxEPD2_SOURCES} ${GFX_SOURCES} ${BUSIO_SOURCES} ${RTCLIB_SOURCES} ${ONEBUTTON_SOURCES}
    INCLUDE_DIRS "." "../include"
        "${LIB_ROOT}/GxEPD2/src"
        "${LIB_ROOT}/Adafruit GFX Library"
        "${LIB_ROOT}/Adafruit BusIO"
        "${LIB_ROOT}/RTClib/src"
        "${LIB_ROOT}/OneButton/src"
    REQUIRES framework-arduinoespressif32
)
