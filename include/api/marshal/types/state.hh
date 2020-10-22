#ifndef __MARSHAL_STATE_HH_
#define __MARSHAL_STATE_HH_

#include "api/marshal/types/base.hh"

namespace token {
  namespace api {
    namespace marshal {
      namespace detail {
        struct state {
          std::string                          userId;      /**< User identifier          */
          std::string                          token;       /**< Token                    */
          std::string                          value;       /**< Value                    */
          std::string                          mask;        /**< Masked value             */
          std::string                          expiration;  /**< Expiration date/time     */
          std::map< std::string, std::string > properties;  /**< Entry properties         */
          std::string                          lastUsed;    /**< Last used date/time      */
          std::string                          lastUpdated; /**< Last updated date/time   */
          std::string                          status;      /**< Operational status value */
          std::map< std::string, std::string > error;       /**< Error code/string        */
        };

        struct state_getter : public base::getter {
          const state &s;

          explicit state_getter( const state &state_ )
            : s( state_ ) {}

          std::string                          userId( ) override { return s.userId; }
          std::string                          token( ) override { return s.token; }
          std::string                          value( ) override { return s.value; }
          std::string                          mask( ) override { return s.mask; }
          std::string                          expiration( ) override { return s.expiration; }
          std::string                          lastUsed( ) override { return s.lastUsed; }
          std::string                          lastUpdated( ) override { return s.lastUpdated; }
          std::string                          status( ) override { return s.status; }
          std::map< std::string, std::string > error( ) override { return s.error; }
          std::map< std::string, std::string > properties( ) override { return s.properties; }
        };

        struct state_setter : public base::setter {
          state &s;

          explicit state_setter( state &state_ )
            : s( state_ ) {}

          void userId( const std::string &value ) override { s.userId = value; }
          void token( const std::string &value ) override { s.token = value; }
          void value( const std::string &value ) override { s.value = value; }
          void mask( const std::string &value ) override { s.mask = value; }
          void expiration( const std::string &value ) override { s.expiration = value; }
          void lastUsed( const std::string &value ) override { s.lastUsed = value; }
          void lastUpdated( const std::string &value ) override { s.lastUpdated = value; }
          void status( const std::string &value ) override { s.status = value; }
          void error( const std::map< std::string, std::string > &value ) override {
            s.error.clear( );
            std::copy( value.begin( ), value.end( ), std::inserter( s.error, s.error.end( ) ) );
          }

          void properties( const std::map< std::string, std::string > &value ) override {
            s.properties.clear( );
            std::copy( value.begin( ), value.end( ), std::inserter( s.properties, s.properties.end( ) ) );
          }

          void clear( ) override {
            s.userId.clear( );
            s.token.clear( );
            s.value.clear( );
            s.mask.clear( );
            s.expiration.clear( );
            s.properties.clear( );
            s.lastUsed.clear( );
            s.lastUpdated.clear( );
            s.status.clear( );
            s.error.clear( );
          }
        };
      } // namespace detail
    }   // namespace marshal
  }     // namespace api
} // namespace token

#endif // __MARSHAL_STATE_HH_
