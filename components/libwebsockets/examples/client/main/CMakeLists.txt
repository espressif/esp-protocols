set(SRC_FILES "lws-client.c") # Define source files
set(INCLUDE_DIRS ".") # Define include directories
set(EMBED_FILES "") # Initialize an empty list for files to embed

list(APPEND EMBED_FILES
    "certs/client_cert.pem"
    "certs/ca_cert.pem"
    "certs/client_key.pem")

idf_component_register(SRCS "${SRC_FILES}"
                       INCLUDE_DIRS "${INCLUDE_DIRS}"
                       EMBED_TXTFILES "${EMBED_FILES}")
