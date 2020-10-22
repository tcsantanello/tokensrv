#ifndef _SESSION_DETECT_HH_
#define _SESSION_DETECT_HH_

#include "api/http/session/plain.hh"
#include "api/http/session/secure.hh"
#include <boost/logic/tribool.hpp>
#include <ctype.h>

namespace token {
  namespace api {
    namespace http {

      template < typename T >
      void hexdump( std::ostream &os, T begin, T end ) {
        static const char *alpha = "0123456789ABCDEF";

        for ( T iter = begin; iter != end; ) {
          char  line[ 81 ] = "";
          char *hex        = line;
          char *raw        = line + 48 + 8;

          std::memset( line, ' ', sizeof( line ) / sizeof( line[ 0 ] ) - 1 );

          for ( T lend = iter + 16; ( iter != end ) && ( iter != lend ); ++iter ) {
            *( hex++ ) = alpha[ ( *iter & 0xF0 ) >> 4 ];
            *( hex++ ) = alpha[ ( *iter & 0x0F ) >> 0 ];
            *( hex++ ) = ' ';
            *( raw++ ) = ( std::isprint( *iter ) ) ? *iter : '.';

            hex += ( iter == lend - 8 ) * 3;
          }

          os << line << "\n";
        }
      }

      void hexdump( std::ostream &os, boost::asio::const_buffer buffer ) {
        auto *ptr = reinterpret_cast< const uint8_t * >( buffer.data( ) );
        hexdump( os, ptr, ptr + buffer.size( ) );
      }

      template < typename Traits = DefaultTypeTraits >
      class DetectSession : public Session< DetectSession< Traits >, Traits >,
                            public std::enable_shared_from_this< DetectSession< Traits > > {
        using self_type   = DetectSession< Traits >;
        using base_type   = Session< self_type, Traits >;
        using plain_type  = PlainSession< Traits >;
        using secure_type = SecureSession< Traits >;
        using config_type = typename base_type::config_type;

       public:
        DetectSession( io_service_type *                io,
                       networking::ssl::context &       _ctx,
                       std::shared_ptr< config_type >   _rc,
                       std::shared_ptr< executor_type > _executor )
          : base_type( io, std::move( _rc ), std::move( _executor ) )
          , target_length( sizeof( ssl_record_type ) )
          , ctx( _ctx ) {}

        /**
         * @brief Accept session data to determine session type
         */
        virtual void start( ) {
          networking::async_read( this->sock,
                                  this->buffer.prepare( target_length ),
                                  std::bind( &self_type::read_completion,
                                             this->shared_from_this( ),
                                             std::placeholders::_1,
                                             std::placeholders::_2 ),
                                  networking::bind_executor( this->strand,
                                                             std::bind( &self_type::on_data, //
                                                                        this->shared_from_this( ),
                                                                        std::placeholders::_1,
                                                                        std::placeholders::_2 ) ) );
        }

        /**
         * @brief Identify if this is an SSL or plain session
         * @note Will create a new, proper, session when identified
         * @param ec boost error code
         * @param received bytes received
         */
        void on_data( boost::system::error_code ec, std::size_t received ) {
          if ( ec ) {
            LOG( this->logger, //
                 critical,
                 "Failed in processing data for session: {}",
                 ec.message( ) );
          } else {
            this->do_connect( );

            this->buffer.commit( received );

            auto is_ssl = is_handshake( this->buffer.data( ) );
            if ( boost::indeterminate( is_ssl ) ) {
              start( ); // Keep getting more data till we're sure
            } else {
              if ( !is_ssl ) {
                std::make_shared< plain_type >( std::move( this->buffer ),
                                                std::move( this->sock ),
                                                std::move( this->strand ),
                                                std::move( this->route_config ),
                                                std::move( this->executor ),
                                                std::move( this->logger ),
                                                std::move( this->local ),
                                                std::move( this->remote ) )
                  ->start( );
              } else {
                std::make_shared< secure_type >( std::move( this->buffer ),
                                                 std::move( this->sock ),
                                                 ctx,
                                                 std::move( this->strand ),
                                                 std::move( this->route_config ),
                                                 std::move( this->executor ),
                                                 std::move( this->logger ),
                                                 std::move( this->local ),
                                                 std::move( this->remote ) )
                  ->start( );
              }

              // Get out of here ''this'' is invalid!
              return;
            }
          }
        }

        tcp_type::socket &transport( ) { return this->sock; }

       protected:
#pragma pack( push, 1 )
        struct ssl_record_type {
          std::uint8_t  type;
          std::uint8_t  major;
          std::uint8_t  minor;
          std::uint16_t len;
        };
#pragma pack( pop )

        /**
         * @brief Identify if there is enough to identify the session type
         * @param ec boost error code
         * @param received number of bytes read
         * @return number of bytes left to obtain
         */
        size_t read_completion( boost::system::error_code ec, std::size_t received ) {
          return target_length < received ? 0 : target_length - received;
        }

        /**
         * @brief Identify if the received data is an SSL handshake
         * @return true for ssl, false for plain, indeterminate for not yet known
         */
        boost::tribool is_handshake( boost::asio::const_buffer buff ) {
          ssl_record_type record;
          std::size_t     nbytes = buff.size( );

          /*
           * Identify if the SSL/TLS record layer conforms with a handshake message
           *
           * u8  - content type  [22 - handshake]
           * u8  - version major
           * u8  - version minor
           * u16 - message length
           * *   - message body
           */

          if ( nbytes < sizeof( record ) ) {
            target_length = sizeof( record );
            return boost::indeterminate;
          } else {
            networking::buffer_copy(
              networking::buffer( reinterpret_cast< uint8_t * >( &record ), sizeof( record ) ),
              buff );

            record.len = ntohs( record.len );

            if ( record.type != 22 ) {
              return false;
            } else if ( record.len + sizeof( record ) > nbytes ) {
              // We want the whole handshake message...
              target_length = record.len;
              return boost::indeterminate;
            } else {
              return true;
            }
          }
        }

        size_t                    target_length;
        networking::ssl::context &ctx;
      };
    } // namespace http
  }   // namespace api
} // namespace token

#endif // _SESSION_DETECT_HH_
