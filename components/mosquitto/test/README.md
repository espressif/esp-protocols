# ESP32 mosquitto tests

Mosquitto component doesn't have any tests yet, but we upcycle IDF mqtt tests and run them with the current version of mosquitto.
For that we need to update the IDF test project's `idf_component.yml` file to reference this actual version of mosquitto.
We also need to update some pytest decorators to run only relevant test cases. See the [replacement](./replace_decorators.py) script.
