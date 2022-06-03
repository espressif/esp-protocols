#include <ctime>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <asio.hpp>

using asio::ip::udp;

std::string make_daytime_string()
{
  using namespace std; // For time_t, time and ctime;
  time_t now = time(0);
  return ctime(&now);
}

int main()
{
  try
  {
    asio::io_service io_service;

    udp::socket socket(io_service, udp::endpoint(udp::v4(), 13));

    for (;;)
    {
      boost::array<char, 1> recv_buf;
      udp::endpoint remote_endpoint;
      asio::error error;
      socket.receive_from(asio::buffer(recv_buf),
          remote_endpoint, 0, asio::assign_error(error));

      if (error && error != asio::error::message_size)
        throw error;

      std::string message = make_daytime_string();

      socket.send_to(asio::buffer(message),
          remote_endpoint, 0, asio::ignore_error());
    }
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
