#ifndef __TYPE_TOKEN_H_
#define __TYPE_TOKEN_H_

#include "api/marshal/types/base.hh"
#include <map>
#include <memory.h>
#include <string>
#include <time.h>
#include <token/api/manager.hh>

namespace token {
  namespace api {
    namespace marshal {
      namespace token {
        using TokenEntry = ::token::api::TokenEntry;

        /**
         * @brief Parse a date/time value
         * @param value date/time value
         * @return database date/time
         */
        dbcpp::DBTime toTime( const std::string &value ) {
          struct tm tm = { 0 };
          strptime( value.c_str( ), "%FT%T%z", &tm );
          return ::dbcpp::DBTime{ std::chrono::seconds( mktime( &tm ) ) };
        }

        /**
         * @brief Format a database date/time value
         * @param value database date/time
         * @return formatted date/time
         */
        std::string fromTime( const ::dbcpp::DBTime value ) {
          struct tm _tm          = { 0 };
          char      block[ 128 ] = "";
          time_t    val          = ::dbcpp::DBClock::to_time_t( value );

          if ( !val ) {
            return "";
          }

#ifdef __USE_POSIX
          gmtime_r( &val, &_tm );
#else
          memcpy( &_tm, &localtime( &val ), sizeof( _tm ) );
#endif

          ::strftime( block, sizeof( block ), "%FT%T%z", &_tm );
          return block;
        }

        struct getter : public base::getter {
          const TokenEntry &entry;

          explicit getter( const TokenEntry &entry_ )
            : entry( entry_ ) {}

          std::string                          token( ) { return entry.token; }
          std::string                          value( ) { return entry.value; }
          std::string                          mask( ) { return entry.mask; }
          std::string                          expiration( ) { return fromTime( entry.expiration ); }
          std::string                          lastUsed( ) { return ""; }    // fromTime( entry.lastUsed ); }
          std::string                          lastUpdated( ) { return ""; } // fromTime( entry.lastUpdated ); }
          std::map< std::string, std::string > properties( ) { return entry.properties; }
        };

        struct setter : public base::setter {
          TokenEntry &entry;

          explicit setter( TokenEntry &entry_ )
            : entry( entry_ ) {}

          void token( const std::string &token ) { entry.token = token; }
          void value( const std::string &value ) { entry.value = value; }
          void mask( const std::string &mask ) { entry.mask = mask; }
          void expiration( const std::string &expiration ) { entry.expiration = toTime( expiration ); }
          void properties( const std::map< std::string, std::string > &properties ) {
            entry.properties.clear( );

            for ( auto &&pair : properties ) {
              entry.properties[ pair.first ] = pair.second;
            }
          }
        };
      } // namespace token
    }   // namespace marshal
  }     // namespace api
} // namespace token

#endif // __TYPE_TOKEN_H_
