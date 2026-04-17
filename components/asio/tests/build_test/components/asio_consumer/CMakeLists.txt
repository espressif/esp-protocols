# Regression test component: must compile with `REQUIRES asio` only.
# If asio doesn't expose sock_utils as a public dependency, including
# "asio.hpp" fails because it transitively includes "socketpair.h".
idf_component_register(SRCS "asio_consumer.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES asio)
