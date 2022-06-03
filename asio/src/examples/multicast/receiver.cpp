#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include "asio.hpp"

const short multicast_port = 30001;
const std::string multicast_addr = "225.0.0.1";

class receiver
{
public:
  receiver(asio::demuxer& d)
    : socket_(d)
  {
    // Create the socket so that multiple may be bound to the same address.
    socket_.open(asio::ipv4::udp());
    socket_.set_option(asio::socket_option::reuse_address(true));
    socket_.bind(asio::ipv4::udp::endpoint(multicast_port));

    // Join the multicast group.
    socket_.set_option(asio::ipv4::multicast::add_membership(multicast_addr));

    socket_.async_recvfrom(data_, max_length, sender_endpoint_,
        boost::bind(&receiver::handle_recvfrom, this, asio::arg::error,
          asio::arg::bytes_recvd));
  }

  void handle_recvfrom(const asio::error& error, size_t bytes_recvd)
  {
    if (!error)
    {
      std::cout.write(data_, bytes_recvd);
      std::cout << std::endl;

      socket_.async_recvfrom(data_, max_length, sender_endpoint_,
          boost::bind(&receiver::handle_recvfrom, this, asio::arg::error,
            asio::arg::bytes_recvd));
    }
  }

private:
  asio::dgram_socket socket_;
  asio::ipv4::udp::endpoint sender_endpoint_;
  enum { max_length = 1024 };
  char data_[max_length];
};

int main(int argc, char* argv[])
{
  try
  {
    asio::demuxer d;
    receiver s(d);
    d.run();
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
