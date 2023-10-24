include(UserverTestsuite)

set(CONFIG_VARS_PATH "${CMAKE_SOURCE_DIR}/configs/config_vars_testing.yaml")
if (EXISTS "${CONFIG_VARS_PATH}")
    set(PYTEST_ARGS_CONFIG_VARS "--service-config-vars=${CONFIG_VARS_PATH}")
else()
    set(PYTEST_ARGS_CONFIG_VARS "")
endif()

set(SECDIST_PATH "${CMAKE_SOURCE_DIR}/configs/secdist.json")
if (EXISTS "${SECDIST_PATH}")
    set(PYTEST_ARGS_CONFIG_VARS ${PYTEST_ARGS_CONFIG_VARS} "--service-secdist=${SECDIST_PATH}")
endif()

userver_testsuite_add(
  SERVICE_TARGET telegram_bot
  REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PYTHONPATH ${CMAKE_CURRENT_BINARY_DIR}/../src/messages
  PYTEST_ARGS
  --service-config=${CMAKE_SOURCE_DIR}/configs/static_config.yaml
  --service-binary=${CMAKE_BINARY_DIR}/telegram_bot
  --config-fallback=${CMAKE_SOURCE_DIR}/configs/dynamic_config_fallback.json
  ${PYTEST_ARGS_CONFIG_VARS}
)
