if(NOT DEFINED APP OR NOT DEFINED ROOT)
    message(FATAL_ERROR "APP and ROOT are required")
endif()
find_program(PYTHON python3 REQUIRED)
execute_process(COMMAND "${PYTHON}" "${ROOT}/scripts/bundle_runtime.py" --app "${APP}"
    COMMAND_ERROR_IS_FATAL ANY)
