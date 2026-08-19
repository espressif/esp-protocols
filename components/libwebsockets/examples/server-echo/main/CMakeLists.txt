set(SRC_FILES "lws-server-echo.c") # Initialize SRC_FILES as an empty list
set(INCLUDE_DIRS ".") # Define include directories
set(EMBED_FILES) # Initialize EMBED_FILES as an empty list

list(APPEND EMBED_FILES
    "certs/server_cert.pem"
    "certs/ca_cert.pem"
    "certs/server_key.pem")

idf_component_register(SRCS  "${SRC_FILES}"
                       INCLUDE_DIRS "${INCLUDE_DIRS}"
                       EMBED_TXTFILES "${EMBED_FILES}")
