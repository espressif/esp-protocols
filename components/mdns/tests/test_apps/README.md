| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 |
| ----------------- | ----- | -------- | -------- |

# mDNS test_apps (retired from CI)

This ethernet DUT app previously ran bidirectional mDNS checks via
`pytest_mdns_app.py`. That pytest was retired because the multicast peer loop
was fragile under system mDNS, and the coverage moved elsewhere:

| Former case | Now covered by |
|-------------|----------------|
| Host → DUT A / delegated A | `examples/query_advertise/pytest_mdns.py` |
| Host → DUT service / subtype | `tests/host_test/pytest_mdns.py` (responder) |
| DUT → host `mdns_query_a` | `tests/host_test` `test_query_a_against_peer` |
| DUT → host async A | `tests/host_test` `test_query_a_async_against_peer` |
| DUT → host `mdns_query_srv` | `tests/host_test` `test_query_srv_against_peer` |

The C app remains for manual flash/debug. Prefer `tests/host_test` for
automated querier coverage.
