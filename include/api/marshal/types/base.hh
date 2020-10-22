#ifndef __BASETYPE_BASE_HH_
#define __BASETYPE_BASE_HH_

#include <map>
#include <string>

namespace token {
  namespace api {
    namespace marshal {
      namespace base {
        struct getter {
          virtual std::string                          userId( ) { return ""; }
          virtual std::string                          token( ) { return ""; }
          virtual std::string                          value( ) { return ""; }
          virtual std::string                          mask( ) { return ""; }
          virtual std::string                          expiration( ) { return ""; }
          virtual std::string                          lastUsed( ) { return ""; }
          virtual std::string                          lastUpdated( ) { return ""; }
          virtual std::string                          status( ) { return ""; }
          virtual std::map< std::string, std::string > error( ) { return { }; }
          virtual std::map< std::string, std::string > properties( ) { return { }; }
        };

        struct setter {
          virtual void userId( const std::string & ) {}
          virtual void token( const std::string & ) {}
          virtual void value( const std::string & ) {}
          virtual void mask( const std::string & ) {}
          virtual void expiration( const std::string & ) {}
          virtual void lastUsed( const std::string & ) {}
          virtual void lastUpdated( const std::string & ) {}
          virtual void status( const std::string & ) {}
          virtual void error( const std::map< std::string, std::string > & ) {}
          virtual void properties( const std::map< std::string, std::string > & ) {}
          virtual void clear( ) {}

          template < class Getter >
          void set( Getter getter ) {
            std::string                          string;
            std::map< std::string, std::string > map;

            if ( !( string = getter.userId( ) ).empty( ) ) {
              userId( string );
            }

            if ( !( string = getter.token( ) ).empty( ) ) {
              token( string );
            }

            if ( !( string = getter.value( ) ).empty( ) ) {
              value( string );
            }

            if ( !( string = getter.mask( ) ).empty( ) ) {
              mask( string );
            }

            if ( !( string = getter.expiration( ) ).empty( ) ) {
              expiration( string );
            }

            if ( !( string = getter.lastUsed( ) ).empty( ) ) {
              lastUsed( string );
            }

            if ( !( string = getter.lastUpdated( ) ).empty( ) ) {
              lastUpdated( string );
            }

            if ( !( string = getter.status( ) ).empty( ) ) {
              status( string );
            }

            if ( !( map = getter.error( ) ).empty( ) ) {
              error( map );
            }

            if ( !( map = getter.properties( ) ).empty( ) ) {
              properties( map );
            }
          }
        };
      } // namespace base
    }   // namespace marshal
  }     // namespace api
} // namespace token

#endif // __BASETYPE_BASE_HH_
