# Mosquitto tests

## Host test

A Catch2 unit test runs on Linux and verifies the broker can start, stop, restart,
and shut down cleanly while clients with a Last-Will and matching subscribers are
still connected. Build and run:

```bash
cd host_test
idf.py build
./build/mosquitto_host_test.elf
```

## IDF mqtt tests

We upcycle IDF mqtt tests and run them with the current version of mosquitto.
For that we need to update the IDF test project's `idf_component.yml` file to reference this actual version of mosquitto.
We also need to update some pytest decorators to run only relevant test cases. See the [replacement](./replace_decorators.py) script.
