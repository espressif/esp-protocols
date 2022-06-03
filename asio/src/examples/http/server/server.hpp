#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <asio.hpp>
#include <string>
#include <boost/noncopyable.hpp>
#include "connection.hpp"
#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace http {
namespace server {

/// The top-level class of the HTTP server.
class server
  : private boost::noncopyable
{
public:
  /// Construct the server to listen on the specified TCP port and serve up
  /// files from the given directory.
  explicit server(short port, const std::string& doc_root);

  /// Run the server's demuxer loop.
  void run();

  /// Stop the server.
  void stop();

private:
  /// Handle completion of an asynchronous accept operation.
  void handle_accept(const asio::error& e);

  /// Handle a request to stop the server.
  void handle_stop();

  /// The demuxer used to perform asynchronous operations.
  asio::demuxer demuxer_;

  /// Acceptor used to listen for incoming connections.
  asio::socket_acceptor acceptor_;

  /// The connection manager which owns all live connections.
  connection_manager connection_manager_;

  /// The next connection to be accepted.
  connection_ptr new_connection_;

  /// The handler for all incoming requests.
  request_handler request_handler_;
};

} // namespace server
} // namespace http

#endif // HTTP_SERVER_HPP
