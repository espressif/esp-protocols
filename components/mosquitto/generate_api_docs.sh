#!/usr/bin/env bash

doxygen Doxyfile

esp-doxybook -i doxygen/xml -o ./api.md
