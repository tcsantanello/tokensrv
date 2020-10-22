#ifndef __MARSHAL_H_
#define __MARSHAL_H_

#include "api/http.hh"
//#include "api/http/rest.hh"
#include "api/marshal/types/state.hh"
#include "api/marshal/types/status.hh"
#include "api/marshal/types/token.hh"
#include "token/api/manager.hh"

/*
 * datetime: YYYYmmdd'T'HH:MM:SS
 *
 * [
 *   { Token operations: Request
 *     "userId": <string>,
 *     "value": <string>,
 *     "expiration": <datetime>
 *     "properties": [ { "name": <string>, "value": <string> } ]
 *   }
 *
 *   { Token operations: Response
 *     "userId": <string>,
 *     "token": <string>,
 *     "value": <string>,
 *     "mask": <string>,
 *     "expiration": <datetime>
 *     "properties": [ { "name": <string>, "value": <string> } ]
 *     "lastUsed": <datetime>,
 *     "lastUpdated": <datetime>
 *   }
 *
 *   { Operational Inquiry: Response
 *     "status": <string>
 *   }
 *
 *   { Error result
 *     "userId": <string>,
 *     "error": { "message": <string> }
 *   }
 * ]
 */

namespace token {
  namespace api {
    namespace marshal {
      class MarshalError : public std::exception {
        const std::string &msg_;

       public:
        MarshalError( const std::string &msg )
          : msg_( msg ) {}

        const char *what( ) const noexcept { return msg_.c_str( ); };
      };

      /**
       * Request / response value marshalling
       * @param body_type message body type
       */
      template < typename BodyType, typename Mapper >
      struct Marshal {
        using body_type     = BodyType;
        using wire_setter   = typename Mapper::setter;
        using wire_getter   = typename Mapper::getter;
        using status_getter = status::getter;
        using token_setter  = token::setter;
        using token_getter  = token::getter;
        using self          = Marshal< BodyType, Mapper >;
        using token_entry   = ::token::api::TokenEntry;
        using status_result = ::token::api::Status;
        using state_type    = detail::state;
        using state_getter  = detail::state_getter;
        using state_setter  = detail::state_setter;

        state_type state;

        Marshal( ) {}

        explicit Marshal( const body_type &request ) { from( request ); }
        explicit Marshal( const token_entry &entry ) { from( entry ); }
        explicit Marshal( const status_result &_status ) { from( _status ); }

        /**
         * @brief Perform the marshalling based on the return value of a method
         * @param function method to execute
         * @param args... function arguments
         * @return this
         */
        template < typename Function, typename... Args >
        self &from( Function function, Args... args ) {
          std::string message;
          unsigned    code;

          try {
            from( function( args... ) );
            return *this;
          } catch ( MarshalError &ex ) {
            spdlog::critical( "Failed to marshal message data for client: {}", ex.what( ) );

            message = ex.what( );
            code    = ( unsigned ) boost::beast::http::status::bad_request;
          } catch ( ::token::exceptions::InvalidTokenFormat &ex ) {
            spdlog::critical( "Invalid token format has been configured" );

            message = "Vault configuration error; please contact administrator";
            code    = ( unsigned ) boost::beast::http::status::service_unavailable;
          } catch ( ::token::exceptions::TokenCryptographyError &ex ) {
            spdlog::critical( "Failed to perform cryptography for client: {}", ex.what( ) );

            message = "Cryptographic error; please contact administrator";
            code    = ( unsigned ) boost::beast::http::status::internal_server_error;
          } catch ( ::token::exceptions::TokenGenerationError &ex ) {
            spdlog::critical( "Failed to generate token for client: {}", ex.what( ) );

            message = "Cryptographic error; please contact administrator";
            code    = ( unsigned ) boost::beast::http::status::internal_server_error;
          } catch ( ::token::exceptions::TokenNoVaultError &ex ) {
            spdlog::critical( "Invalid token vault specified by client: {}", ex.what( ) );

            message = "Vault not found";
            code    = ( unsigned ) boost::beast::http::status::not_found;
          } catch ( ::token::exceptions::TokenRangeError &ex ) {
            spdlog::critical( "Unable to access token for client: {}", ex.what( ) );

            message = "Error accessing token";
            code    = ( unsigned ) boost::beast::http::status::payload_too_large;
          } catch ( ::token::exceptions::TokenSQLError &ex ) {
            spdlog::critical( "Database error while processing request for client: {}",
                              ex.what( ) );

            message = "Vault storage error";
            code    = ( unsigned ) boost::beast::http::status::internal_server_error;
          } catch ( ::std::exception &ex ) {
            spdlog::critical( "Exception encountered while processing client request: {}",
                              ex.what( ) );
            code    = ( unsigned ) boost::beast::http::status::internal_server_error;
            message = "Internal error";
          }

          state_setter{ state }.error(
            { { "message", message }, { "code", std::to_string( code ) } } );

          return *this;
        }

        /**
         * @brief Retrieve the values provided by the user
         * @param request REST request body
         * @return this
         */
        self &from( const body_type &request ) {
          to( state_setter{ state }, wire_getter{ request } );

          spdlog::trace( "Received from the client:" );
          spdlog::trace( "UserID: {}", state.userId );
          spdlog::trace( "Value: {}", state.value );
          spdlog::trace( "Expiration: {}", state.expiration );
          spdlog::trace( "Properties: {}", [ & ]( ) {
            std::stringstream ss;
            for ( auto &iter : state.properties ) {
              ss << iter.first << "=" << iter.second << " ";
            }
            return ss.str( );
          }( ) );

          return *this;
        }

        /**
         * @brief Proxy response values from the token governer
         * @param entry token governer response entry
         * @return this
         */
        self &from( const token_entry &entry ) {
          return to( state_setter{ state }, token_getter{ entry } );
        }

        /**
         * @brief Marshal the token governer status message
         * @param status status value
         * @return this
         */
        self &from( const status_result &status ) {
          return to( state_setter{ state }, status_getter{ status } );
        }

        /**
         * @brief Copy results from one form to another
         * @param dest destination
         * @param src source
         * return this
         */
        template < class Setter, class Getter >
        self &to( Setter dest, Getter src ) {
          dest.clear( );
          dest.set( src );
          return *this;
        }

        /**
         * @brief Proxy request values to the token governer
         * @param entry token governer processing entry
         * @return this
         */
        self &to( token_entry &entry ) {
          return to( token_setter{ entry }, state_getter{ state } );
        }

        /**
         * @brief Proxy the response fields to the REST response
         * @param response REST response body
         * @return this
         */
        self &to( body_type &response ) {
          return to( wire_setter{ response }, state_getter{ state } );
        }

        /**
         * @brief Set the user supplied token value
         * @param _token token value
         * @return this
         */
        self &setToken( std::string _token ) {
          state.token = std::move( _token );
          return *this;
        }
      };
    } // namespace marshal
  }   // namespace api
} // namespace token

#endif // __MARSHAL_H_
