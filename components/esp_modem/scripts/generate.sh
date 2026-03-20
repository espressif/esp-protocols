#!/bin/bash

# Default list of files to process if no arguments are provided
default_files=("generate/include/cxx_include/esp_modem_command_library.hpp"
               "generate/include/cxx_include/esp_modem_dce_module.hpp"
               "generate/include/cxx_include/esp_modem_dce_generic.hpp"
               "generate/src/esp_modem_modules.cpp"
               "generate/include/esp_modem_api.h")

# Set the processing directory (defaults to the script's location/..)
script_dir="$(dirname "$(realpath "$0")")"
default_processing_dir="$(realpath "$script_dir/..")"

# Parse optional arguments
if [ -n "$1" ]; then
  # If one argument is provided, treat it as a single file to process
  files=("$1")
else
  # Use default list if no file argument is provided
  files=("${default_files[@]}")
fi

# If a second argument is provided, set it as the processing directory
if [ -n "$2" ]; then
  processing_dir="$(realpath "$2")"
else
  processing_dir="$default_processing_dir"
fi

# Process each file
for file in "${files[@]}"; do
  # Determine the input and output paths based on processing directory
  in_file="$processing_dir/$file"
  out_file="${processing_dir}/${file/generate/command}"
  current_file_dir="$(dirname "$in_file")"

  # Ensure the output directory exists
  mkdir -p "$(dirname "$out_file")"

  echo "Processing $in_file"

  # Process the header and includes -- just paste the content (without expanding)
  sed -n '1,/ESP-MODEM command module starts here/{/ESP-MODEM command module starts here/d;p}' "$in_file" > "$out_file"

  # Determine whether to use clang or clang++ based on file extension
  if [[ $file == *.cpp || $file == *.hpp ]]; then
    compiler="clang++ -E -P -CC -xc++"
  elif [[ $file == *.rst ]]; then
    compiler="clang -E -P -xc"
  else
    compiler="clang -E -P -CC -xc"
  fi

  # Preprocess everything else to expand command prototypes or implementations
  sed -n '1,/ESP-MODEM command module starts here/!p' "$in_file" | \
  $compiler -I"$script_dir" -I"$processing_dir/generate/include" -I"$processing_dir/include" -I"$current_file_dir" -  >> "$out_file"
  # Add potential footer (typically closing C++ sentinel)
  sed -n '1,/ESP-MODEM command module ends here/!p' "$in_file" >> "$out_file"
  if [[ $out_file != *.rst ]]; then
    astyle --style=otbs --attach-namespaces --attach-classes --indent=spaces=4 --convert-tabs --align-pointer=name --align-reference=name --keep-one-line-statements --pad-header --pad-oper --unpad-paren --quiet --max-continuation-indent=120 "$out_file"
  fi
done
