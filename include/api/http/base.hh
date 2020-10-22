
#ifndef __TOKENIZATION_HTTP_BASE_HH__
#define __TOKENIZATION_HTTP_BASE_HH__

#include "executor.hh"
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/regex.hpp>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <token/api.hh>

#define LOG( logger, lvl, ... )                                                                    \
  do {                                                                                             \
    if ( logger->should_log( spdlog::level::lvl ) ) {                                              \
      SPDLOG_LOGGER_CALL( logger, spdlog::level::lvl, __VA_ARGS__ );                               \
    }                                                                                              \
  } while ( 0 )

namespace token {
  namespace api {
    namespace http {
      namespace networking  = boost::asio;
      namespace beast       = boost::beast;
      namespace http        = beast::http;
      using verbs           = http::verb;
      using thread_group    = std::vector< std::thread >;
      using io_service_type = networking::io_service;
      using strand_type     = io_service_type::strand;
      using tcp_type        = networking::ip::tcp;
      using acceptor_type   = tcp_type::acceptor;
      using endpoint_type   = tcp_type::endpoint;
      using socket_type     = tcp_type::socket;
      using buffer_type     = beast::flat_buffer;
      using executor_type   = token::async::Executor;
      using shared_executor = std::shared_ptr< executor_type >;

      template < typename body_type = http::string_body,
                 typename Req       = http::request< body_type >,
                 typename Resp      = http::response< body_type > >
      struct TypeTraits {
        using response_type  = Resp;
        using request_type   = Req;
        using param_map_type = std::unordered_map< std::string, std::string >;
        using handler_type =
          std::function< bool( param_map_type &, request_type &, response_type & ) >;
        using route_type     = std::pair< std::string, handler_type >;
        using route_map_type = std::unordered_multimap< http::verb, route_type >;
      };

      using DefaultTypeTraits = TypeTraits<>;
    } // namespace http
  }   // namespace api
} // namespace token

#endif //__TOKENIZATION_HTTP_BASE_HH__
