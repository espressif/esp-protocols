# Test mdns_cache in isolation. Browse-facing functions are mocked.
set(MOCK_FILES
        "mdns_browser"
)

list(APPEND SOURCES unity/${UNIT_TESTS}/test_cache.c)
