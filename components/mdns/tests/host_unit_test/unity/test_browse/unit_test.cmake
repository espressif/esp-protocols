# Mock pcb to disable network queries. Use real browser and cache.
set(MOCK_FILES
        "mdns_pcb"
)

list(APPEND SOURCES unity/${UNIT_TESTS}/test_browse.c)
