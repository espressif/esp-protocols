# Remove everything, but
# - elf/bin files in the build dir
# - partition-table and bootloader binaries
# - flasher args
# - sdkconfigs (header and json)
# (Ignoring the command failure as it refuses to delete nonempty dirs)
find $1 ! -regex ".*/build/[^/]+.\(bin\|elf\)" -a ! -regex ".*\(bootloader\|partition-table\).bin" -a ! -name "flasher_args.json" -a ! -regex ".*/build/config/sdkconfig.\(h\|json\)" -delete || true
