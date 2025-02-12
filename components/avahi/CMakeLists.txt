set(avahi_path "avahi")
set(avahi_core "${avahi_path}/avahi-core")
set(avahi_common "${avahi_path}/avahi-common")
set(avahi_core_SRCS "${avahi_core}/server.c"
        "${avahi_common}/error.c"
        "${avahi_common}/malloc.c"
        "${avahi_common}/simple-watch.c"
        "${avahi_common}/domain.c"
        "${avahi_common}/timeval.c"
        "${avahi_common}/strlst.c"
        "${avahi_common}/address.c"
        "${avahi_common}/rlist.c"
        "${avahi_common}/utf8.c"
        "${avahi_core}/log.c"
        "${avahi_core}/rr.c"
        "${avahi_core}/response-sched.c"
        "${avahi_core}/probe-sched.c"
        "${avahi_core}/query-sched.c"
        "${avahi_core}/rrlist.c"
        "${avahi_core}/hashmap.c"
        "${avahi_core}/announce.c"
        "${avahi_core}/domain-util.c"
        "${avahi_core}/multicast-lookup.c"
        "${avahi_core}/iface.c"
        "${avahi_core}/entry.c"
        "${avahi_core}/cache.c"
        "${avahi_core}/util.c"
        "${avahi_core}/addr-util.c"
        "${avahi_core}/timeeventq.c"
        "${avahi_core}/querier.c"
        "${avahi_core}/browse.c"
        "${avahi_core}/dns.c"
        "${avahi_core}/wide-area.c"
        "${avahi_core}/prioq.c"
        "${avahi_core}/fdutil.c"
        "${avahi_core}/browse-domain.c"
        "${avahi_core}/browse-dns-server.c"
        "${avahi_core}/browse-service.c"
        "${avahi_core}/resolve-host-name.c"
        "${avahi_core}/resolve-address.c"
        "${avahi_core}/resolve-service.c"
        "${avahi_core}/browse-service-type.c"
        # "${avahi_core}/browse-record.c"
        # "${avahi_core}/resolve-service-group.c"
        # "${avahi_core}/resolve-record.c"
)

idf_component_register(SRCS "avahi-port/iface-esp32.c"
                            "avahi-port/socket.c"
                            ${avahi_core_SRCS}
                    INCLUDE_DIRS "avahi-port" "${avahi_core}" "${avahi_path}"
                    REQUIRES "esp_netif")

target_compile_definitions(${COMPONENT_LIB} PRIVATE "HAVE_CONFIG_H")
# ignore specific warnings on certain files
set_source_files_properties(${avahi_common}/domain.c PROPERTIES COMPILE_FLAGS -Wno-char-subscripts)
set_source_files_properties(${avahi_core}/rr.c PROPERTIES COMPILE_FLAGS -Wno-format)
set_source_files_properties(${avahi_core}/iface.c PROPERTIES COMPILE_FLAGS -Wno-format)
set_source_files_properties(${avahi_core}/entry.c PROPERTIES COMPILE_FLAGS -Wno-format)
set_source_files_properties(${avahi_common}/address.c PROPERTIES COMPILE_FLAGS -Wno-format)
