//
// host_resolver_service.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003, 2004 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_IPV4_DETAIL_HOST_RESOLVER_SERVICE_HPP
#define ASIO_IPV4_DETAIL_HOST_RESOLVER_SERVICE_HPP

#include "asio/detail/push_options.hpp"

#include "asio/detail/push_options.hpp"
#include <cstring>
#include <boost/noncopyable.hpp>
#include "asio/detail/pop_options.hpp"

#include "asio/error.hpp"
#include "asio/detail/socket_ops.hpp"
#include "asio/detail/socket_types.hpp"
#include "asio/ipv4/host.hpp"

namespace asio {
namespace ipv4 {
namespace detail {

template <typename Demuxer>
class host_resolver_service
{
public:
  // Implementation structure for a host resolver.
  struct resolver_impl
    : private boost::noncopyable
  {
  };

  // The native type of the host resolver.
  typedef resolver_impl* impl_type;

  // Return a null host resolver implementation.
  static impl_type null()
  {
    return 0;
  }

  // Constructor.
  host_resolver_service(Demuxer& d)
    : demuxer_(d)
  {
  }

  // The demuxer type for this service.
  typedef Demuxer demuxer_type;

  // Get the demuxer associated with the service.
  demuxer_type& demuxer()
  {
    return demuxer_;
  }

  // Create a new host resolver implementation.
  void create(impl_type& impl)
  {
    impl = new resolver_impl;
  }

  // Destroy a host resolver implementation.
  void destroy(impl_type& impl)
  {
    if (impl != null())
    {
      delete impl;
      impl = null();
    }
  }

  /// Get host information for the local machine.
  template <typename Error_Handler>
  void get_local_host(impl_type& impl, host& h, Error_Handler error_handler)
  {
    char name[1024];
    if (asio::detail::socket_ops::gethostname(name, sizeof(name)) != 0)
      error_handler(asio::error(asio::detail::socket_ops::get_error()));
    else
      get_host_by_name(impl, name, h, error_handler);
  }

  /// Get host information for a specified address.
  template <typename Error_Handler>
  void get_host_by_address(impl_type& impl, host& h, const address& addr,
      Error_Handler error_handler)
  {
    hostent ent;
    char buf[8192] = ""; // Size recommended by Stevens, UNPv1.
    int error = 0;
    in_addr a;
    a.s_addr = asio::detail::socket_ops::host_to_network_long(addr.to_ulong());
    if (asio::detail::socket_ops::gethostbyaddr_r(
          reinterpret_cast<const char*>(&a), sizeof(in_addr), AF_INET, &ent,
          buf, sizeof(buf), &error) == 0)
      error_handler(asio::error(error));
    else if (ent.h_length != sizeof(in_addr))
      error_handler(asio::error(asio::error::host_not_found));
    else
      populate_host_object(h, ent);
  }

  /// Get host information for a named host.
  template <typename Error_Handler>
  void get_host_by_name(impl_type& impl, host& h, const std::string& name,
      Error_Handler error_handler)
  {
    hostent ent;
    char buf[8192] = ""; // Size recommended by Stevens, UNPv1.
    int error = 0;
    if (asio::detail::socket_ops::gethostbyname_r(name.c_str(), &ent, buf,
          sizeof(buf), &error) == 0)
      error_handler(asio::error(error));
    else if (ent.h_addrtype != AF_INET || ent.h_length != sizeof(in_addr))
      error_handler(asio::error(asio::error::host_not_found));
    else
      populate_host_object(h, ent);
  }

private:
  // Populate a host object from a hostent structure.
  void populate_host_object(host& h, hostent& ent)
  {
    host tmp;

    // Copy the host data to a temporary structure. These operations may throw
    // an exception, in which case we don't want the caller's host object to
    // be modified.
    tmp.name = ent.h_name;
    for (char** alias = ent.h_aliases; *alias; ++alias)
      tmp.aliases.push_back(*alias);
    for (char** addr = ent.h_addr_list; *addr; ++addr)
    {
      using namespace std; // For memcpy.
      in_addr a;
      memcpy(&a, *addr, sizeof(in_addr));
      tmp.addresses.push_back(
          address(asio::detail::socket_ops::network_to_host_long(a.s_addr)));
    }

    // The data was successfully copied, so swap with the caller's host object.
    h.name.swap(tmp.name);
    h.aliases.swap(tmp.aliases);
    h.addresses.swap(tmp.addresses);
  }

  // The demuxer used for dispatching handlers.
  Demuxer& demuxer_;
};

} // namespace detail
} // namespace ipv4
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_IPV4_DETAIL_HOST_RESOLVER_SERVICE_HPP
