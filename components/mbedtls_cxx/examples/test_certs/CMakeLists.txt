idf_component_register(
        EMBED_TXTFILES srv.crt
        EMBED_TXTFILES srv.key
        EMBED_TXTFILES ca.crt
        EMBED_TXTFILES client.crt
        EMBED_TXTFILES client.key
        INCLUDE_DIRS "."
        REQUIRES mbedtls_cxx)
