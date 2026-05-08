set(TEST_USAGE_DB_SRC
    ${CMAKE_SOURCE_DIR}/plugin/usagedatabase.cpp
)

set(TEST_CATALOG_SRC
    ${CMAKE_SOURCE_DIR}/plugin/catalogloader.cpp
    ${CMAKE_SOURCE_DIR}/plugin/providerpricingcatalog.cpp
    ${CMAKE_SOURCE_DIR}/plugin/subscriptionplancatalog.cpp
)

set(TEST_PROVIDER_SRC
    ${TEST_CATALOG_SRC}
    ${CMAKE_SOURCE_DIR}/plugin/providerbackend.cpp
    ${CMAKE_SOURCE_DIR}/plugin/openaicompatibleprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/openaiprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/azureopenaiprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/anthropicprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/cohereprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/deepseekprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/googleprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/googleveoprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/groqprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/mistralprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/ollamacloudprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/openrouterprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/togetherprovider.cpp
    ${CMAKE_SOURCE_DIR}/plugin/xaiprovider.cpp
)

set(TEST_SUBSCRIPTION_SRC
    ${TEST_CATALOG_SRC}
    ${CMAKE_SOURCE_DIR}/plugin/subscriptiontoolbackend.cpp
    ${CMAKE_SOURCE_DIR}/plugin/localactivitymonitorbase.cpp
    ${CMAKE_SOURCE_DIR}/plugin/claudecodemonitor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/codexclimonitor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/copilotmonitor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/cursormonitor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/windsurfmonitor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/jetbrainsaimonitor.cpp
)

set(TEST_PROVIDER_BACKEND_ONLY_SRC
    ${TEST_CATALOG_SRC}
    ${CMAKE_SOURCE_DIR}/plugin/providerbackend.cpp
)

set(TEST_SUBTOOL_ONLY_SRC
    ${TEST_CATALOG_SRC}
    ${CMAKE_SOURCE_DIR}/plugin/subscriptiontoolbackend.cpp
    ${CMAKE_SOURCE_DIR}/plugin/localactivitymonitorbase.cpp
)

set(TEST_INTEGRATION_SRC
    ${CMAKE_SOURCE_DIR}/plugin/localmetricsserver.cpp
    ${CMAKE_SOURCE_DIR}/plugin/webhooknotifier.cpp
    ${CMAKE_SOURCE_DIR}/plugin/usagedatabase.cpp
    ${CMAKE_SOURCE_DIR}/plugin/awssigv4signer.cpp
)

set(TEST_UPDATECHECKER_SRC
    ${CMAKE_SOURCE_DIR}/plugin/updatechecker.cpp
)

# --- UsageDatabase series test ---
add_executable(test_usagedatabase_series
    test_usagedatabase_series.cpp
    ${TEST_USAGE_DB_SRC}
)

target_include_directories(test_usagedatabase_series
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_usagedatabase_series
    PRIVATE Qt6::Core Qt6::Test Qt6::Sql
)

add_test(NAME usagedatabase_series COMMAND test_usagedatabase_series)

# --- History mapping regression test ---
add_executable(test_history_mapping_regression
    test_history_mapping_regression.cpp
    ${TEST_USAGE_DB_SRC}
)

target_include_directories(test_history_mapping_regression
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_history_mapping_regression
    PRIVATE Qt6::Core Qt6::Test Qt6::Sql
)

add_test(NAME history_mapping_regression COMMAND test_history_mapping_regression)

# --- Providers mocked HTTP test (from remote) ---
add_executable(test_providers_mocked_http
    test_providers_mocked_http.cpp
    ${TEST_PROVIDER_SRC}
)

target_include_directories(test_providers_mocked_http
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_providers_mocked_http
    PRIVATE Qt6::Core Qt6::Test Qt6::Network KF6::I18n
)

add_test(NAME providers_mocked_http COMMAND test_providers_mocked_http)

# --- Subscription tools test (from remote) ---
add_executable(test_subscription_tools
    test_subscription_tools.cpp
    ${TEST_SUBSCRIPTION_SRC}
)

target_include_directories(test_subscription_tools
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_subscription_tools
    PRIVATE Qt6::Core Qt6::Test Qt6::Network KF6::I18n
)

add_test(NAME subscription_tools COMMAND test_subscription_tools)

# --- ProviderBackend unit test ---
add_executable(test_providerbackend
    test_providerbackend.cpp
    ${TEST_PROVIDER_BACKEND_ONLY_SRC}
)

target_include_directories(test_providerbackend
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_providerbackend
    PRIVATE Qt6::Core Qt6::Test Qt6::Network
)

add_test(NAME providerbackend COMMAND test_providerbackend)

# --- SubscriptionToolBackend unit test ---
add_executable(test_subscriptiontoolbackend
    test_subscriptiontoolbackend.cpp
    ${TEST_SUBTOOL_ONLY_SRC}
)

target_include_directories(test_subscriptiontoolbackend
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_subscriptiontoolbackend
    PRIVATE Qt6::Core Qt6::Test Qt6::Network
)

add_test(NAME subscriptiontoolbackend COMMAND test_subscriptiontoolbackend)

# --- Catalog loader tests ---
add_executable(test_catalogs
    test_catalogs.cpp
    ${TEST_CATALOG_SRC}
)

target_include_directories(test_catalogs
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_catalogs
    PRIVATE Qt6::Core Qt6::Test
)

add_test(NAME catalogs COMMAND test_catalogs)

# --- Local integrations / signer tests ---
add_executable(test_local_integrations
    test_local_integrations.cpp
    ${TEST_INTEGRATION_SRC}
)

target_include_directories(test_local_integrations
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_local_integrations
    PRIVATE Qt6::Core Qt6::Test Qt6::Network Qt6::Sql
)

add_test(NAME local_integrations COMMAND test_local_integrations)

# --- UpdateChecker unit test ---
add_executable(test_updatechecker
    test_updatechecker.cpp
    ${TEST_UPDATECHECKER_SRC}
)

target_include_directories(test_updatechecker
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_updatechecker
    PRIVATE Qt6::Core Qt6::Test Qt6::Network
)

add_test(NAME updatechecker COMMAND test_updatechecker)

# --- UsageDatabase extended test ---
add_executable(test_usagedatabase_extended
    test_usagedatabase_extended.cpp
    ${TEST_USAGE_DB_SRC}
)

target_include_directories(test_usagedatabase_extended
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_usagedatabase_extended
    PRIVATE Qt6::Core Qt6::Test Qt6::Sql
)

add_test(NAME usagedatabase_extended COMMAND test_usagedatabase_extended)

# --- UsageDatabase analyst test ---
add_executable(test_usagedatabase_analyst
    test_usagedatabase_analyst.cpp
    ${TEST_USAGE_DB_SRC}
)

target_include_directories(test_usagedatabase_analyst
    PRIVATE ${CMAKE_SOURCE_DIR}/plugin
)

target_link_libraries(test_usagedatabase_analyst
    PRIVATE Qt6::Core Qt6::Test Qt6::Sql
)

add_test(NAME usagedatabase_analyst COMMAND test_usagedatabase_analyst)

# --- Script-based tests ---
add_test(NAME version_consistency COMMAND ${CMAKE_SOURCE_DIR}/scripts/check_version_consistency.sh)
add_test(NAME no_hardcoded_versions COMMAND ${CMAKE_SOURCE_DIR}/scripts/check_no_hardcoded_versions.sh)
add_test(NAME provider_catalog COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_provider_catalog.py)
add_test(NAME subscription_catalog COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_subscription_catalog.py)
add_test(NAME no_hardcoded_pricing COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_no_hardcoded_pricing.py)
add_test(NAME config_portability COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_config_portability.py)
