#ifndef __SESSION_SECURE_HH_
#define __SESSION_SECURE_HH_

#include "api/http/session/basic.hh"
#include <boost/asio/ssl/stream.hpp>

namespace token {
  namespace api {
    namespace http {
      template < typename Traits = DefaultTypeTraits >
      class SecureSession : public Session< SecureSession< Traits >, Traits >,
                            public std::enable_shared_from_this< SecureSession< Traits > > {
        using self_type     = SecureSession< Traits >;
        using base_type     = Session< self_type, Traits >;
        using config_type   = RouteConfig< Traits >;
        using response_type = typename Traits::response_type;
        using request_type  = typename Traits::request_type;

       public:
        SecureSession( io_service_type *                _io,
                       networking::ssl::context &       _ctx,
                       std::shared_ptr< config_type >   _rc,
                       std::shared_ptr< executor_type > _executor )
          : base_type( _io, std::move( _rc ), std::move( _executor ) )
          , stream( this->sock, _ctx ) {}

        SecureSession( buffer_type &&                    _buffer,
                       socket_type &&                    _sock,
                       networking::ssl::context &        _ctx,
                       strand_type &&                    _strand,
                       std::shared_ptr< config_type >    _rc,
                       std::shared_ptr< executor_type >  _exec,
                       std::shared_ptr< spdlog::logger > _logger,
                       std::string &&                    _local,
                       std::string &&                    _remote )
          : base_type( std::move( _buffer ),
                       std::move( _sock ),
                       std::move( _strand ),
                       std::move( _rc ),
                       std::move( _exec ),
                       std::move( _logger ),
                       std::move( _local ),
                       std::move( _remote ) )
          , stream( this->sock, _ctx ) {}

        networking::ssl::stream< tcp_type::socket & > &transport( ) { return stream; }

        /**
         * @brief Process the SSL handshake
         */
        virtual void start( ) {
          auto bound      = std::bind( &self_type::on_handshake, //
                                  this->shared_from_this( ),
                                  std::placeholders::_2,
                                  std::placeholders::_1 );
          auto exec_bound = networking::bind_executor( this->strand, bound );

          stream.async_handshake( networking::ssl::stream_base::server, //
                                  this->buffer.data( ),
                                  exec_bound );
        }

       protected:
        /**
         * @brief Process the SSL handshake result, then start processing requests
         * @param used bytes consumed
         * @param ec handshake processing error code
         */
        void on_handshake( std::size_t used, boost::system::error_code ec ) {
          if ( ec ) {
            LOG( this->logger,
                 critical,
                 "Error encountered while processing SSL handshake: {}",
                 ec.message( ) );
            return; // Let the object die peacefully...
          }

          this->buffer.consume( used );

          base_type::perform_read( );
        }

        networking::ssl::stream< tcp_type::socket & > stream;
      };
    } // namespace http
  }   // namespace api
} // namespace token

#endif // __SESSION_SECURE_HH_
