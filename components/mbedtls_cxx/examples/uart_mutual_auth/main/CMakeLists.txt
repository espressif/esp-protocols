idf_component_register(SRCS "uart_mutual.cpp"
                    EMBED_TXTFILES ../../test_certs/ca.crt
                    EMBED_TXTFILES ../../test_certs/client.crt
                    EMBED_TXTFILES ../../test_certs/client.key
                    EMBED_TXTFILES ../../test_certs/srv.crt
                    EMBED_TXTFILES ../../test_certs/srv.key
                    INCLUDE_DIRS ".")
