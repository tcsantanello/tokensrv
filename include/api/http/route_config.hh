
#ifndef __TOKENIZATION_ROUTE_CONFIG_HH__
#define __TOKENIZATION_ROUTE_CONFIG_HH__

#include "base.hh"

namespace token {
  namespace api {
    namespace http {
      template < typename Traits = DefaultTypeTraits >
      class RouteConfig {
        using response_type  = typename Traits::response_type;
        using request_type   = typename Traits::request_type;
        using handler_type   = typename Traits::handler_type;
        using param_map_type = typename Traits::param_map_type;
        using route_type     = typename Traits::route_type;
        using route_map      = typename Traits::route_map_type;

        route_map    routes;
        handler_type defaultHandler;

       protected:
        std::shared_ptr< spdlog::logger > logger;

        void addRoute( verbs verb, const std::string &resource, handler_type handler ) {
          routes.insert( std::make_pair( verb, std::make_pair( resource, std::move( handler ) ) ) );
        }

       public:
        void get( const std::string &resource, handler_type handler ) {
          addRoute( verbs::get, resource, std::move( handler ) );
        }

        void put( const std::string &resource, handler_type handler ) {
          addRoute( verbs::put, resource, std::move( handler ) );
        }

        void post( const std::string &resource, handler_type handler ) {
          addRoute( verbs::post, resource, std::move( handler ) );
        }

        void del( const std::string &resource, handler_type handler ) {
          addRoute( verbs::delete_, resource, std::move( handler ) );
        }

        void         setDefault( handler_type handler ) { defaultHandler = std::move( handler ); }
        handler_type getDefault( ) { return defaultHandler; }

        using route_parsed = std::pair< std::vector< std::string >, handler_type >;

        handler_type getRoute( verbs verb, std::string resource, param_map_type &params ) {
          auto range = routes.equal_range( verb );
          auto last  = std::unique( // Sort out & remove double '/'
            resource.begin( ),
            resource.end( ),
            []( const std::string::value_type &lhs, const std::string::value_type &rhs ) -> bool {
              return ( ( lhs == '/' ) && ( rhs == '/' ) );
            } );

          last -= *( last - 1 ) == '/'; // Remove trailing too
          resource.erase( last, resource.end( ) );

          for ( auto route = range.first; route != range.second; ++route ) {
            if ( route->first == verb ) {
              param_map_type _p;
              const auto &   entry          = route->second;
              auto &         entry_resource = entry.first;
              auto           er_begin       = entry_resource.begin( );
              auto           iter           = resource.begin( );

              do {
                auto set = std::mismatch( er_begin, entry_resource.end( ), iter );

                // Come to the end of the registered resource
                if ( set.first == entry_resource.end( ) ) {
                  // ... and the submitted request path
                  if ( set.second == resource.end( ) ) {
                    // Yay, we found it!
                    params = std::move( _p );
                    return entry.second;
                  }
                  break;
                }

                if ( *set.first != '{' ) {
                  break;
                } else if ( *( set.first - 1 ) != '/' ) {
                  break;
                } else if ( *( set.second - 1 ) != '/' ) {
                  break;
                } else {
                  auto end = std::find( set.first + 1, entry_resource.end( ), '}' );

                  if ( end == entry_resource.end( ) ) {
                    goto next;
                  }

                  auto anchor = std::string( set.first + 1, end );
                  auto slash  = std::find( set.second, resource.end( ), '/' );

                  er_begin = end + 1;
                  iter     = slash;

                  assert( er_begin == entry_resource.end( ) || *er_begin == '/' );
                  assert( iter == resource.end( ) || *iter == '/' );

                  _p[ anchor ] = std::string( set.second, slash );
                }
              } while ( true );
            next:;
            }
          }

          return defaultHandler;
        }
      };
    } // namespace http
  }   // namespace api
} // namespace token

#endif //__TOKENIZATION_ROUTE_CONFIG_HH__
