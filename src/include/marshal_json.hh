#ifndef __MARSHAL_JSON_H_
#define __MARSHAL_JSON_H_

#include "api/marshal/types/base.hh"
#include <nlohmann/json.hpp>

namespace token {
  namespace api {
    namespace marshal {
      struct json {
        using value_type = ::nlohmann::json;

        struct getter : public base::getter {
          const value_type &json;

          getter( const value_type &j )
            : json( j ) {}

          template < typename T >
          T get( std::string id ) {
            try {
              auto it = json.find( id );
              if ( it != json.end( ) ) {
                return it->get< T >( );
              }
            } catch ( nlohmann::detail::type_error &ex ) {
              throw MarshalError( ex.what( ) );
            } catch ( std::out_of_range &ex ) {
            }
            return { };
          }

          std::string                          userId( ) override { return get< std::string >( "userId" ); }
          std::string                          token( ) override { return get< std::string >( "token" ); }
          std::string                          value( ) override { return get< std::string >( "value" ); }
          std::string                          mask( ) override { return get< std::string >( "mask" ); }
          std::string                          expiration( ) override { return get< std::string >( "expiration" ); }
          std::map< std::string, std::string > properties( ) override {
            std::map< std::string, std::string > rc;
            auto                                 it = json.find( "properties" );

            if ( it != json.end( ) ) {
              for ( auto &aobj : it->items( ) ) {
                auto &entry           = aobj.value( );
                rc[ entry[ "name" ] ] = entry[ "value" ].get< std::string >( );
              }
            }

            return rc;
          }
        };

        struct setter : public base::setter {
          value_type &json;

          setter( value_type &j )
            : json( j ) {}

          void userId( const std::string &value ) override { json[ "userId" ] = value; }
          void token( const std::string &value ) override { json[ "token" ] = value; }
          void value( const std::string &value ) override { json[ "value" ] = value; }
          void mask( const std::string &value ) override { json[ "mask" ] = value; }
          void expiration( const std::string &value ) override { json[ "expiration" ] = value; }
          void lastUsed( const std::string &value ) override { json[ "lastUsed" ] = value; }
          void lastUpdated( const std::string &value ) override { json[ "lastUpdated" ] = value; }
          void status( const std::string &value ) override { json[ "status" ] = value; }
          void error( const std::map< std::string, std::string > &value ) override { json[ "error" ] = value; }
          void properties( const std::map< std::string, std::string > &value ) override {
            json[ "properties" ] = value;
          }

          void clear( ) override { json.clear( ); }
        };
      };
    } // namespace marshal
  }   // namespace api
} // namespace token

#endif // __MARSHAL_JSON_H_
