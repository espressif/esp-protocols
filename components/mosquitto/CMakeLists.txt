set(m_dir mosquitto)
set(m_src_dir ${m_dir}/src)
set(m_incl_dir ${m_dir}/include)
set(m_lib_dir ${m_dir}/lib)
set(m_deps_dir ${m_dir}/deps)

set(m_srcs
        ${m_lib_dir}/memory_mosq.c
        ${m_lib_dir}/util_mosq.c
        ${m_lib_dir}/net_mosq.c
        ${m_lib_dir}/will_mosq.c
        ${m_lib_dir}/alias_mosq.c
        ${m_lib_dir}/send_mosq.c
        ${m_lib_dir}/strings_mosq.c
        ${m_lib_dir}/packet_mosq.c
        ${m_lib_dir}/packet_datatypes.c
        ${m_lib_dir}/property_mosq.c
        ${m_lib_dir}/util_topic.c
        ${m_lib_dir}/send_publish.c
        ${m_lib_dir}/send_disconnect.c
        ${m_lib_dir}/handle_pubackcomp.c
        ${m_lib_dir}/handle_pubrec.c
        ${m_lib_dir}/handle_pubrel.c
        ${m_lib_dir}/handle_ping.c
        ${m_lib_dir}/time_mosq.c
        ${m_lib_dir}/utf8_mosq.c

        ${m_src_dir}/bridge.c
        ${m_src_dir}/bridge_topic.c
        ${m_src_dir}/conf_includedir.c
        ${m_src_dir}/context.c
        ${m_src_dir}/control.c
        ${m_src_dir}/database.c
        ${m_src_dir}/handle_auth.c
        ${m_src_dir}/handle_connack.c
        ${m_src_dir}/handle_connect.c
        ${m_src_dir}/handle_disconnect.c
        ${m_src_dir}/handle_publish.c
        ${m_src_dir}/handle_subscribe.c
        ${m_src_dir}/handle_unsubscribe.c
        ${m_src_dir}/keepalive.c
        ${m_src_dir}/logging.c
        ${m_src_dir}/loop.c
        ${m_src_dir}/memory_public.c
        ${m_src_dir}/mosquitto.c
        ${m_src_dir}/mux.c
        ${m_src_dir}/mux_epoll.c
        ${m_src_dir}/mux_poll.c
        ${m_src_dir}/net.c
        ${m_src_dir}/password_mosq.c
        ${m_src_dir}/persist_read.c
        ${m_src_dir}/persist_read_v234.c
        ${m_src_dir}/persist_read_v5.c
        ${m_src_dir}/persist_write.c
        ${m_src_dir}/persist_write_v5.c
        ${m_src_dir}/property_broker.c
        ${m_src_dir}/read_handle.c
        ${m_src_dir}/retain.c
        ${m_src_dir}/security.c
        ${m_src_dir}/security_default.c
        ${m_src_dir}/send_auth.c
        ${m_src_dir}/send_connack.c
        ${m_src_dir}/send_suback.c
        ${m_src_dir}/send_unsuback.c
        ${m_src_dir}/service.c
        ${m_src_dir}/session_expiry.c
        ${m_src_dir}/signals.c
        ${m_src_dir}/subs.c
        ${m_src_dir}/sys_tree.c
        ${m_src_dir}/topic_tok.c
        ${m_src_dir}/websockets.c
        ${m_src_dir}/will_delay.c
        ${m_src_dir}/xtreport.c)

idf_component_register(SRCS ${m_srcs}
                            port/callbacks.c port/config.c port/signals.c port/ifaddrs.c port/broker.c port/files.c
                    PRIV_INCLUDE_DIRS port/priv_include port/priv_include/sys ${m_dir} ${m_src_dir}
                                      ${m_incl_dir} ${m_lib_dir} ${m_deps_dir}
                    INCLUDE_DIRS ${m_incl_dir} port/include
                    PRIV_REQUIRES newlib
        )

target_compile_definitions(${COMPONENT_LIB} PRIVATE "WITH_BROKER")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

# Some mosquittos source unconditionally define `_GNU_SOURCE` which collides with IDF build system
# producing warning: "_GNU_SOURCE" redefined
# This workarounds this issue by undefining the macro for the selected files
set(sources_that_define_gnu_source ${m_lib_dir}/net_mosq.c ${m_src_dir}/loop.c ${m_src_dir}/mux_poll.c)
foreach(offending_src ${sources_that_define_gnu_source})
    set_source_files_properties(${offending_src} PROPERTIES COMPILE_OPTIONS "-U_GNU_SOURCE")
endforeach()
