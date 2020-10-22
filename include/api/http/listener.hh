
#ifndef __TOKENIZATION_LISTENER_HH__
#define __TOKENIZATION_LISTENER_HH__

#include "base.hh"
#include "session.hh"

namespace token {
  namespace api {
    namespace http {
#define TOKEN_API_HTTP_LISTENER_LOG_ID "token::api::http::listener"

      template < typename Traits = DefaultTypeTraits >
      class Listener : public std::enable_shared_from_this< Listener< Traits > > {
        using self_type           = Listener< Traits >;
        using session_base_type   = Session< Traits >;
        using session_detect_type = DetectSession< Traits >;
        using session_plain_type  = PlainSession< Traits >;
        using config_type         = RouteConfig< Traits >;
        using response_type       = typename Traits::response_type;
        using request_type        = typename Traits::request_type;
        using handler_type        = typename Traits::handler_type;

        acceptor_type                      acceptor;
        networking::ssl::context *         ctx;
        std::shared_ptr< io_service_type > io;
        std::shared_ptr< config_type >     rc;
        std::shared_ptr< executor_type >   executor;
        std::shared_ptr< spdlog::logger >  logger;

        std::string address_format( boost::asio::ip::tcp::endpoint ep ) const {
          return fmt::format( ( ep.protocol( ) == tcp_type::v6( ) ) ? "[{}]:{}" : "{}:{}",
                              ep.address( ).to_string( ),
                              ep.port( ) );
        }

        void on_accept_tcp( std::shared_ptr< session_plain_type > &session,
                            const boost::system::error_code &      ec ) {
          if ( !ec ) { // No Error
            session->start( );
          } else {
            LOG( logger,
                 err,
                 "Failed to accept new connection on {}: {}",
                 address_format( acceptor.local_endpoint( ) ),
                 ec.message( ) );
          }
          start_tcp( );
        }

        void start_tcp( ) {
          auto session = std::make_shared< session_plain_type >( io.get( ), rc, executor );
          acceptor.async_accept( session->socket( ),                   //
                                 std::bind( &self_type::on_accept_tcp, //
                                            this->shared_from_this( ),
                                            session,
                                            std::placeholders::_1 ) );
        }

        void on_accept_dual( std::shared_ptr< session_detect_type > &session,
                             const boost::system::error_code &       ec ) {
          if ( !ec ) { // No Error
            session->start( );
          } else {
            LOG( logger,
                 err,
                 "Failed to accept new connection on {}: {}",
                 address_format( acceptor.local_endpoint( ) ),
                 ec.message( ) );
          }
          start_dual( );
        }

        void start_dual( ) {
          auto session = std::make_shared< session_detect_type >( io.get( ), *ctx, rc, executor );
          acceptor.async_accept( session->socket( ),                    //
                                 std::bind( &self_type::on_accept_dual, //
                                            this->shared_from_this( ),
                                            session,
                                            std::placeholders::_1 ) );
        }

       public:
        Listener( std::shared_ptr< io_service_type > &&_io,
                  std::shared_ptr< config_type > &&    _rc,
                  networking::ssl::context *           _ctx,
                  endpoint_type &&                     ep,
                  std::shared_ptr< executor_type > &   _executor )
          : acceptor( acceptor_type{ *_io, ep } )
          , ctx( _ctx )
          , io( _io )
          , rc( _rc )
          , executor( _executor )
          , logger( token::api::create_logger( TOKEN_API_HTTP_LISTENER_LOG_ID, { } ) ) {
          beast::error_code ec;
          acceptor.set_option( networking::socket_base::reuse_address( true ), ec );
          acceptor.listen( networking::socket_base::max_listen_connections, ec );
          LOG( logger, info, "Created listener on {}", address_format( ep ) );
        }

        void start( ) {
          if ( ctx ) {
            start_dual( );
          } else {
            start_tcp( );
          }
        }
      };
    } // namespace http
  }   // namespace api
} // namespace token

#endif //__TOKENIZATION_LISTENER_HH__
