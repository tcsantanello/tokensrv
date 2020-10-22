#ifndef __SESSION_BASIC_H_
#define __SESSION_BASIC_H_

#include "api/http/route_config.hh"
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>

#ifndef LOG
#define LOG( logger, lvl, ... )                                                                    \
  do {                                                                                             \
    if ( logger->should_log( spdlog::level::lvl ) ) {                                              \
      SPDLOG_LOGGER_CALL( logger, spdlog::level::lvl, __VA_ARGS__ );                               \
    }                                                                                              \
  } while ( 0 )
#endif

namespace token {
  namespace api {
    namespace http {
#define TOKEN_API_HTTP_SESSION_LOG_ID "token::api::http::session"

      template < class sub_type, typename traits = DefaultTypeTraits >
      class Session {
       public:
        using self_type      = Session< sub_type, traits >;
        using config_type    = RouteConfig< traits >;
        using response_type  = typename traits::response_type;
        using request_type   = typename traits::request_type;
        using handler_type   = typename traits::handler_type;
        using route_type     = typename traits::route_type;
        using route_map      = typename traits::route_map_type;
        using param_map_type = typename traits::param_map_type;

        Session( buffer_type &&                    _buffer,
                 socket_type &&                    _sock,
                 strand_type &&                    _strand,
                 std::shared_ptr< config_type >    _rc,
                 std::shared_ptr< executor_type >  _exec,
                 std::shared_ptr< spdlog::logger > _logger,
                 std::string &&                    _local,
                 std::string &&                    _remote )
          : buffer( std::move( _buffer ) )
          , sock( std::move( _sock ) )
          , strand( std::move( _strand ) )
          , route_config( std::move( _rc ) )
          , executor( std::move( _exec ) )
          , logger( std::move( _logger ) )
          , local( std::move( _local ) )
          , remote( std::move( _remote ) ) {}

        Session( io_service_type *                io,
                 std::shared_ptr< config_type >   _rc,
                 std::shared_ptr< executor_type > _executor )
          : sock( *io )
          , strand( *io )
          , route_config( std::move( _rc ) )
          , executor( std::move( _executor ) )
          , logger( token::api::create_logger( TOKEN_API_HTTP_SESSION_LOG_ID, { } ) ) {}

        socket_type &               socket( ) { return sock; }
        sub_type &                  subclass( ) { return static_cast< sub_type & >( *this ); }
        std::shared_ptr< sub_type > shared( ) { return subclass( ).shared_from_this( ); }
        // return std::dynamic_pointer_cast< self_type >( subclass( ).shared_from_this( ) );

        /**
         * @brief Get the remote address, as a string
         * @return ip and port as a string
         */
        std::string address_remote( ) const { return remote; }

        /**
         * @brief Get the local address, as a string
         * @return ip and port as a string
         */
        std::string address_local( ) const { return local; }

        /**
         * @brief Start processing data from this session
         */
        virtual void start( ) { perform_read( ); }

       protected:
        /**
         * @brief Set values on initial connection
         */
        void on_connect( ) {
          local  = address_format( sock.local_endpoint( ) );
          remote = address_format( sock.remote_endpoint( ) );
        }

        /**
         * @brief Perform an on_connect
         */
        void do_connect( ) {
          if ( local.empty( ) ) {
            on_connect( );
          }
        }

        /**
         * @brief Set the http status and description
         * @param resp response message
         * @param status http status code
         */
        void error_set( response_type *resp, http::status status ) {
          resp->result( status );
          resp->reason( http::detail::status_to_string( static_cast< unsigned >( status ) ) );
        }

        /**
         * @brief Stringify the endpoint address
         * @brief ep tcp endpoint
         * @return ip and port as a string
         */
        std::string address_format( tcp_type::endpoint ep ) const {
          return fmt::format( ( ep.protocol( ) == tcp_type::v6( ) ) ? "[{}]:{}" : "{}:{}", //
                              ep.address( ).to_string( ),
                              ep.port( ) );
        }

        /**
         * @brief Perform an asynchronous write of a response
         * @param resp http response
         */
        virtual void perform_write( std::shared_ptr< response_type > resp ) {
          // Write it out, using the bound std::function to extend lifetime
          http::async_write(
            subclass( ).transport( ),
            *resp,
            networking::bind_executor( strand,
                                       std::bind( &self_type::on_write,
                                                  shared( ), // Extend this object's lifetime
                                                  resp,      // Auto extend 'resp' lifetime here
                                                  std::placeholders::_1,
                                                  std::placeholders::_2,
                                                  resp->need_eof( ) ) ) );
        }

        /**
         * @brief Initiate an asynchronous read that invokes 'on_read' on successful HTTP message
         * parse
         */
        virtual void perform_read( ) {
          auto req = std::make_shared< request_type >( );
          http::async_read(
            subclass( ).transport( ),
            buffer,
            *req,
            networking::bind_executor( strand,
                                       std::bind( &self_type::on_read, //
                                                  shared( ), // Extend this object's lifetime
                                                  req,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2 ) ) );
        }

       public:
        /**
         * @brief Beast write completion notification
         * @param response message sent in response - bound reference
         * @param ec boost error code
         * @param bytes number of bytes sent
         * @param eof expected end of session
         */
        void on_write( std::shared_ptr< response_type > &response,
                       boost::system::error_code         ec,
                       std::size_t                       bytes,
                       bool                              eof ) {
          boost::ignore_unused( bytes );

          if ( ( ec == beast::http::error::end_of_stream ) || ( eof ) ) {
            sock.shutdown( tcp_type::socket::shutdown_send, ec );
          } else if ( !ec ) {
            perform_read( );
          }
        }

        /**
         * @brief Capture, route match and queue an incoming request for processing
         * @param request reference to bound shared pointer - bound reference
         */
        void on_read( std::shared_ptr< request_type > &request,
                      boost::system::error_code        ec,
                      std::size_t ) {
          if ( ec ) {
            if ( ec == http::error::end_of_stream ) {
              sock.shutdown( tcp_type::socket::shutdown_send, ec );
            } else {
              LOG(
                logger, err, "Error encountered while reading from {}: {}", remote, ec.message( ) );
            }

            return;
          }

          do_connect( );

          const std::string method   = http::to_string( request->method( ) ).to_string( );
          std::string       resource = request->target( ).to_string( );
          param_map_type    params;

          resource.erase( std::find( resource.begin( ), resource.end( ), '?' ), resource.end( ) );

          handler_type handler = route_config->getRoute( request->method( ), resource, params );

          if ( !handler ) {
            LOG( logger,
                 info,
                 "Unknown route for request from {} for {} '{}'",
                 address_remote( ),
                 method,
                 resource );
            handler = [ & ]( param_map_type &, request_type &, response_type &resp ) {
              error_set( &resp, http::status::not_found );
              return true;
            };
          } else {
            LOG( logger, info, "Request from {} for {} '{}'", address_remote( ), method, resource );
          }

          executor->add( std::bind( &self_type::perform, //
                                    shared( ),
                                    std::move( request ),
                                    handler,
                                    std::move( params ) ) );
        }

        void dostuff( std::shared_ptr< request_type > req ) {}

        /**
         * @brief Process an http request action
         * @param req http request
         * @param handler route handler
         * @param params query (path) parameters
         */
        void perform( std::shared_ptr< request_type > req,
                      handler_type                    handler,
                      param_map_type                  params ) {
          auto resp = std::make_shared< response_type >( );

          try {
            // Track our progress...
            LOG( logger,
                 info,
                 "Processing request from {} for {}",
                 address_local( ),
                 http::to_string( req->method( ) ).to_string( ),
                 req->target( ).to_string( ) );

            // Respond in kind to the client's keep alive request
            resp->keep_alive( req->keep_alive( ) );

            // Handle the request
            if ( !handler( params, *req, *resp ) ) {
              LOG( logger, info, "Failed to process a request from {}", address_local( ) );
              error_set( resp.get( ), http::status::internal_server_error );
            }

            // Check out work
            LOG( logger, trace, resp->body( ) );
          } catch ( std::exception &ex ) {
            // Danger Will Robinson, Danger!
            LOG( logger,
                 info,
                 "Exception encountered while processing request from {}: {}",
                 address_local( ),
                 ex.what( ) );
            error_set( resp.get( ), http::status::internal_server_error );
          } catch ( ... ) {
            LOG( logger,
                 info,
                 "Unknown error encountered while processing request from {}",
                 address_local( ) );
            error_set( resp.get( ), http::status::internal_server_error );
          }

          // Stage for write
          resp->prepare_payload( );

          perform_write( std::move( resp ) );
        }

       protected:
        buffer_type                       buffer;
        socket_type                       sock;
        strand_type                       strand;
        std::shared_ptr< config_type >    route_config;
        shared_executor                   executor;
        std::shared_ptr< spdlog::logger > logger;
        std::string                       local;
        std::string                       remote;
      };
    } // namespace http
  }   // namespace api
} // namespace token

#endif // __SESSION_BASIC_H_
