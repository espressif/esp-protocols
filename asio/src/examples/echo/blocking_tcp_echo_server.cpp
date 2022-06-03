#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include "asio.hpp"

const int max_length = 1024;

typedef boost::shared_ptr<asio::stream_socket> stream_socket_ptr;

void session(stream_socket_ptr sock)
{
  try
  {
    char data[max_length];

    size_t length;
    while ((length = sock->recv(data, max_length)) > 0)
      if (asio::send_n(*sock, data, length) == 0)
        break;
  }
  catch (asio::error& e)
  {
    std::cerr << "Error in thread: " << e << "\n";
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception in thread: " << e.what() << "\n";
  }
}

void server(asio::demuxer& d, short port)
{
  asio::socket_acceptor a(d, asio::ipv4::tcp::endpoint(port));
  for (;;)
  {
    stream_socket_ptr sock(new asio::stream_socket(d));
    a.accept(*sock);
    asio::thread t(boost::bind(session, sock));
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: blocking_tcp_echo_server <port>\n";
      return 1;
    }

    asio::demuxer d;

    using namespace std; // For atoi.
    server(d, atoi(argv[1]));
  }
  catch (asio::error& e)
  {
    std::cerr << e << "\n";
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
