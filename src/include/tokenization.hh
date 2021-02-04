#ifndef __TOKENIZATION_HH_
#define __TOKENIZATION_HH_

#include "api/http.hh"
#include "api/marshal/marshal.hh"
#include "authdb.hh"
#include "config.hh"
#include "marshal_json.hh"
#include "options.hh"
#include <algorithm>
#include <boost/process.hpp>
#include <cctype>
#include <chrono>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>
#include <token/api/manager.hh>
#include <uri/uri.hh>

namespace token {
  namespace app {
    namespace beast = boost::beast;
    namespace http  = beast::http;

    using histogram_type = prometheus::Histogram;
    using gauge_type     = prometheus::Gauge;
    using counter_type   = prometheus::Counter;
    using histofam_type  = prometheus::Family< histogram_type >;
    using gaugefam_type  = prometheus::Family< gauge_type >;
    using countfam_type  = prometheus::Family< counter_type >;

    template < typename T >
    struct tracker {};

    template <>
    struct tracker< gauge_type > {
      gauge_type &m;

      explicit tracker( gauge_type &_m )
        : m( _m ) {
        m.Increment( );
      }
      ~tracker( ) { m.Decrement( ); }
    };

    template <>
    struct tracker< counter_type > {
      counter_type &m;

      explicit tracker( counter_type &_m )
        : m( _m ) {
        m.Increment( );
      }
    };

    template <>
    struct tracker< histogram_type > {
      using clock_type = std::chrono::high_resolution_clock;
      using time_type  = std::chrono::time_point< clock_type >;
      histogram_type &m;
      time_type       start;

      explicit tracker( histogram_type &_m )
        : m( _m )
        , start( clock_type::now( ) ) {}

      ~tracker( ) {
        std::chrono::duration< double, std::milli > durms = clock_type::now( ) - start;
        m.Observe( durms.count( ) );
      }
    };

    template < typename provider_type >
    class Tokenization {
      using self_type          = Tokenization< provider_type >;
      using service_type       = token::api::http::HttpService;
      using manager_type       = token::api::TokenManager;
      using database_type      = AuthTokenDB; // token::api::core::TokenDB;
      using executor_type      = token::async::Executor;
      using base_provider_type = token::crypto::Provider;
      using Marshal            = token::api::marshal::Marshal< nlohmann::json, //
                                                    ::token::api::marshal::json >;

      std::shared_ptr< base_provider_type > provider;
      Options                               options;
      token::app::Config                    config;
      bool                                  initCheck;
      std::shared_ptr< database_type >      tokenDB;
      std::shared_ptr< manager_type >       manager;
      std::shared_ptr< executor_type >      executor;
      std::shared_ptr< service_type >       service;
      boost::asio::ssl::context             ctx;
      histogram_type *                      resp_time;
      counter_type *                        req_good;
      counter_type *                        req_limit;
      counter_type *                        req_bad;
      gauge_type *                          req_count;
      prometheus::Registry                  registry;

      static bool check( Options &options, token::app::Config &config ) {
        options.parse( );

        if ( options.has( "version" ) ) {
          std::cout << PROJECT_NAME ": " PROJECT_VERSION
#if defined( COMMIT ) // clang-format off
                                    " (" COMMIT ")"
#endif // clang-format on
                                    "\n";
          exit( 0 );
        } else if ( options.wantHelp( ) ) {
          options.printHelp( );
          exit( 0 );
        }

        return true;
      }

      void processStartCmd( ) {
        if ( !options.has( "foreground" ) ) {
          setsid( );

          pid_t pid = fork( );

          switch ( pid ) {
            case -1: { // Error
              std::cerr << fmt::format(
                "Error backgrounding process (fork): ({}) {}\n", errno, strerror( errno ) );
              exit( 1 );
              break;
            }
            case 0: { // Child
              int fd = open( "/dev/null", O_RDWR );

              dup2( fd, STDIN_FILENO );
              dup2( STDIN_FILENO, STDOUT_FILENO );
              dup2( STDIN_FILENO, STDERR_FILENO );
              break;
            }
            default: { // Parent
              std::cout << fmt::format( "Successfully backgrounded process {}\n", pid );
              exit( 0 );
            }
          }
        }
      }

      void processStopCmd( ) {
        boost::process::ipstream is;
        boost::process::child    ps( boost::process::search_path( "ps" ),
                                  "-o",
                                  "pid,cmd", // comm
                                  "h",
                                  boost::process::std_out > is );
        std::string              line;
        pid_t                    self = getpid( );
        std::string              arg0 = options.parameters( )[ 0 ];

        /* Basename */
        arg0.erase( 0, std::find( arg0.rbegin( ), arg0.rend( ), '/' ).base( ) - arg0.begin( ) );

        /* Read all lines from ps */
        while ( ( ps.running( ) ) || ( std::getline( is, line ) ) ) {
          line.erase( line.begin( ), std::find_if( line.begin( ), line.end( ), []( char &val ) {
                        return !std::isspace( val );
                      } ) );

          /* Nothing ? */
          if ( line.empty( ) ) {
            continue;
          }

          auto  sep      = line.find( ' ' );
          pid_t line_pid = std::stol( line.substr( 0, sep ) );

          /* Don't bother if it is this process */
          if ( line_pid == self ) {
            continue;
          }

          auto line_cmd = line.substr( sep + 1 );
          auto space    = std::string::reverse_iterator( std::find_if(
            line_cmd.begin( ), line_cmd.end( ), []( char &val ) { return std::isspace( val ); } ) );

          auto slash     = std::find( space, line_cmd.rend( ), '/' ).base( );
          auto line_name = line_cmd.substr( slash - line_cmd.begin( ), space.base( ) - slash );

          /* Move along if another executable */
          if ( line_name != arg0 ) {
            continue;
          }

          for ( int num = 0; num < 3; ++num ) {
            if ( !kill( line_pid, num < 2 ? SIGTERM : SIGKILL ) ) {
              return;
            }
          }
        }
      }

      void processVaultCmd( ) {
        if ( !options.has( "vault-name" ) ) {
          std::cerr << "Must supply a vault name to perform vault operations\n";
          exit( 1 );
        }

        if ( !options.has( "vault-key" ) ) {
          std::cerr << "Must supply a vault key to perform vault operations\n";
          exit( 1 );
        }

        auto vaultName = options.get< std::string >( "vault-name" );
        auto encKey    = options.get< std::string >( "vault-key" );

        if ( options.has( "create-vault" ) ) {
          if ( !options.has( "vault-hmac" ) ) {
            std::cerr << "Must supply a vault hash key for vault creation\n";
            exit( 1 );
          }

          if ( !options.has( "vault-format" ) ) {
            std::cerr << "Must supply a token format id for vault creation\n";
            exit( 1 );
          }

          if ( !options.has( "data-length" ) ) {
            std::cerr << "Must supply a data length for vault creation\n";
            exit( 1 );
          }

          auto macKey = options.get< std::string >( "vault-hmac" );
          auto format = options.get< int >( "vault-format" );
          auto length = options.get< int >( "data-length" );

          if ( manager->createVault( vaultName, //
                                     encKey,
                                     macKey,
                                     format,
                                     length,
                                     !options.has( "single-use" ) ) ) {
            std::cout << "Successfully created " << vaultName << "\n";
          } else {
            std::cerr << "Unable to create vault " << vaultName << "\n";
            exit( 1 );
          }
        } else if ( options.has( "rekey" ) ) {
          if ( manager->rekeyVault( vaultName, encKey ) ) {
            std::cout << "Successfully re-keyed " << vaultName << "\n";
          } else {
            std::cerr << "Failed to re-key " << vaultName << "\n";
            exit( 1 );
          }
        }
      }

      void processUserCmd( ) {
        if ( !options.has( "user" ) ) {
          std::cerr << "Must supply a user name to perform user operations\n";
          exit( 1 );
        }

        std::string user = options.get< std::string >( "user" );

        if ( options.has( "create-user" ) ) {
          if ( !options.has( "password" ) && !options.has( "token" ) ) {
            std::cerr << "Must supply either a password or API token to create a user\n";
            exit( 1 );
          }

          tokenDB->create_user( user, //
                                options.get< std::string >( "password" ),
                                options.get< std::string >( "token" ) );
        }

        if ( options.has( "grant" ) ) {
          tokenDB->grant_user( user, options.get< std::string >( "vault" ) );
        }

        if ( options.has( "limit" ) ) {
          std::string period;
          int         count = 0;
          auto        rate  = options.get< std::string >( "rate" );
          auto        slash = rate.find( "/" );

          if ( slash == std::string::npos ) {
            std::cerr << "Improperly formatted rate: " << rate << "\n";
            exit( 1 );
          }

          count  = std::stoi( rate.substr( 0, slash ) );
          period = rate.substr( slash + 1 );

          tokenDB->limit_user( user, options.get< std::string >( "vault" ), count, period );
        }
      }

      void processCmd( ) {
        if ( options.has( "start" ) ) {
          processStartCmd( );
          return;
        } else if ( ( options.has( "create-vault" ) ) || ( options.has( "rekey" ) ) ) {
          processVaultCmd( );
        } else if ( options.has( "create-user" ) || //
                    options.has( "grant" ) ||       //
                    options.has( "limit" ) ) {
          processUserCmd( );
        } else if ( options.has( "stop" ) ) {
          processStopCmd( );
        }

        exit( 0 );
      }

      bool authorized( std::string                  vault,
                       service_type::request_type & request,
                       service_type::response_type &response,
                       uint32_t &                   limit ) {
        auto     auth  = request[ http::field::authorization ];
        auto     space = auth.find( ' ' );
        auto     type  = std::string( auth.substr( 0, space ) );
        auto     value = auth.substr( space + 1 );
        uint32_t uid   = 0;

        std::transform( type.begin( ), type.end( ), type.begin( ), []( uint8_t ch ) -> uint8_t {
          return std::tolower( ch );
        } );

        if ( type == "basic" ) {
          uid = tokenDB->authorizedBasic( std::move( std::string( value ) ) );
        } else if ( type == "bearer" ) {
          uid = tokenDB->authorizedToken( std::move( std::string( value ) ) );
        }

        if ( ( uid == 0 ) || ( !tokenDB->accessible( uid, vault ) ) ) {
          auto status = http::status::unauthorized;
          response.result( status );
          response.reason( http::detail::status_to_string( static_cast< unsigned >( status ) ) );
          response.body( ) =
            nlohmann::json::object(
              { { "message", "Attempted to access a secured resource with no valid access" },
                { "code", std::to_string( static_cast< unsigned >( status ) ) } } )
              .dump( 2 );
          return false;
        }

        limit = tokenDB->rate_limit( uid, vault, limit );

        return true;
      }

      uint32_t preliminary( std::string                  vault,
                            service_type::request_type & request,
                            service_type::response_type &response,
                            nlohmann::json *             body ) {
        uint32_t limit = ( body && body->is_array( ) ) ? body->size( ) : 1;

        if ( !authorized( vault, request, response, limit ) ) {
          limit = 0;
        } else if ( !limit ) {
          auto status = http::status::forbidden;
          response.result( status );
          response.reason( http::detail::status_to_string( static_cast< unsigned >( status ) ) );
          response.body( ) = nlohmann::json::object(
                               { { "code", std::to_string( static_cast< unsigned >( status ) ) },
                                 { "message", "Messages have been throttled due to overuse" } } )
                               .dump( 2 );
        }

        return limit;
      }

     public:
      Tokenization( int         argc,
                    const char *argv[],
                    std::string ssl_key  = "",
                    std::string ssl_cert = "" )
        : provider( std::make_shared< provider_type >( ) )
        , options( provider, argc, argv )
        , config( argc, argv )
        , ctx( boost::asio::ssl::context::sslv23 ) {

        check( options, config );
        tokenDB = std::make_shared< database_type >( config.databaseUrl( ), //
                                                     config.databasePoolSize( ) );
        manager = std::make_shared< manager_type >( provider, tokenDB );

        processCmd( );

        auto &processing = prometheus::BuildHistogram( )
                             .Name( "processing" )
                             .Help( "How long it took to process a request" )
                             .Register( registry );
        auto &requests = prometheus::BuildCounter( ) //
                           .Name( "request" )
                           .Help( "Number of processed requests" )
                           .Register( registry );
        auto &count = prometheus::BuildGauge( )
                        .Name( "requests" )
                        .Help( "Number of requests currently being processed" )
                        .Register( registry );

        resp_time = &processing.Add( { },
                                     histogram_type::BucketBoundaries{
                                       0.005,
                                       0.01,
                                       0.025,
                                       0.05,
                                       0.075,
                                       0.1,
                                       0.25,
                                       0.5,
                                       0.75,
                                       1.0,
                                       2.5,
                                       5.0,
                                       7.5,
                                       10.0,
                                     } );
        req_good  = &requests.Add( { { "result", "successful" } } );
        req_limit = &requests.Add( { { "result", "overdraw" } } );
        req_bad   = &requests.Add( { { "result", "failure" } } );
        req_count = &count.Add( { { "state", "processing" } } );

        executor = std::make_shared< executor_type >( config.workerPoolSize( ) );
        service  = std::make_shared< service_type >( executor, config.restPoolSize( ) );

        if ( ( ssl_key.empty( ) ) || ( ssl_cert.empty( ) ) ) {
          service->addListener( config.listenerAddress( ), config.listenerPort( ) );
        } else {
          ctx.set_password_callback(
            []( std::size_t, boost::asio::ssl::context_base::password_purpose ) {
              return "restsrv";
            } );

          ctx.set_options( boost::asio::ssl::context::default_workarounds |
                           boost::asio::ssl::context::no_sslv2 );

          ctx.use_certificate_chain( boost::asio::buffer( ssl_cert.data( ), ssl_cert.size( ) ) );
          ctx.use_private_key( boost::asio::buffer( ssl_key.data( ), ssl_key.size( ) ),
                               boost::asio::ssl::context::file_format::pem );

          service->addListener( config.listenerAddress( ), config.listenerPort( ), &ctx );
        }

        service->get( "/vaults/{vault}/token/{token}",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        tracker< gauge_type >     reqtrack( *req_count );
                        tracker< histogram_type > dur_track( *resp_time );
                        uint32_t                  limit = preliminary( params[ "vault" ], //
                                                      request,
                                                      response,
                                                      nullptr );

                        if ( !limit ) {
                          req_limit->Increment( );
                          return true;
                        }

                        auto obj = nlohmann::json::object( );

                        Marshal{ }
                          .from( [ & ]( ) -> token::api::TokenEntry {
                            return manager->detokenize( params[ "vault" ], params[ "token" ] );
                          } )
                          .to( obj );

                        response_set( response, obj );

                        return true;
                      } );

        service->put( "/vaults/{vault}/token/{token}",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        tracker< gauge_type >     reqtrack( *req_count );
                        tracker< histogram_type > dur_track( *resp_time );
                        auto                      body  = nlohmann::json::parse( request.body( ) );
                        uint32_t                  limit = preliminary( params[ "vault" ], //
                                                      request,
                                                      response,
                                                      &body );

                        if ( !limit ) {
                          req_limit->Increment( );
                          return true;
                        }

                        auto ret   = nlohmann::json::object( );
                        auto entry = token::api::TokenEntry{ };

                        Marshal{ body }
                          .to( entry )
                          .setToken( params[ "token" ] )
                          .from( [ & ]( ) -> token::api::TokenEntry {
                            return manager->tokenize( params[ "vault" ], entry.value, &entry );
                          } )
                          .to( ret );

                        response_set( response, ret );

                        return true;
                      } );

        service->post( "/vaults/{vault}/token",
                       [ this ]( service_type::param_map_type &params,
                                 service_type::request_type &  request,
                                 service_type::response_type & response ) -> bool {
                         tracker< gauge_type >     reqtrack( *req_count );
                         tracker< histogram_type > dur_track( *resp_time );
                         nlohmann::json            resp_array = nlohmann::json::array( );
                         nlohmann::json            req_array;
                         auto                      body  = nlohmann::json::parse( request.body( ) );
                         uint32_t                  limit = preliminary( params[ "vault" ], //
                                                       request,
                                                       response,
                                                       &body );

                         if ( !limit ) {
                           req_limit->Increment( );
                           return true;
                         }

                         auto ret = nlohmann::json::object( );

                         if ( body.is_array( ) ) {
                           req_array = body;
                         } else {
                           req_array = nlohmann::json::array( );
                           req_array.emplace_back( body );
                         }

                         for ( auto &rs : req_array.items( ) ) {
                           auto resp_entry = nlohmann::json::object( );
                           auto entry      = token::api::TokenEntry{ };

                           if ( !limit ) {
                             auto status = http::status::forbidden;

                             resp_entry[ "code" ] =
                               std::to_string( static_cast< unsigned >( status ) );
                             resp_entry[ "message" ] =
                               "Messages have been throttled due to overuse";
                           } else {
                             Marshal{ rs.value( ) }
                               .to( entry )
                               .from( [ & ]( ) -> token::api::TokenEntry {
                                 return manager->tokenize( params[ "vault" ], entry.value, &entry );
                               } )
                               .to( resp_entry );

                             --limit;
                           }

                           resp_array.emplace_back( resp_entry );
                         }

                         response_set( response, body.is_array( ) ? resp_array : resp_array[ 0 ] );
                         return true;
                       } );

        service->del( "/vaults/{vault}/token/{token}",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        tracker< gauge_type >     reqtrack( *req_count );
                        tracker< histogram_type > dur_track( *resp_time );
                        uint32_t                  limit = preliminary( params[ "vault" ], //
                                                      request,
                                                      response,
                                                      nullptr );

                        if ( !limit ) {
                          req_limit->Increment( );
                          return true;
                        }

                        auto resp = nlohmann::json::object( );

                        Marshal{ }
                          .from( [ & ]( ) -> token::api::TokenEntry {
                            return manager->remove( params[ "vault" ], params[ "token" ] );
                          } )
                          .to( resp );

                        response_set( response, resp );

                        return true;
                      } );

        service->get( "/vaults/{vault}/query",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        tracker< gauge_type >     reqtrack( *req_count );
                        tracker< histogram_type > dur_track( *resp_time );
                        uint32_t                  limit = preliminary( params[ "vault" ], //
                                                      request,
                                                      response,
                                                      nullptr );
                        size_t                    count = 0;

                        if ( !limit ) {
                          req_limit->Increment( );
                          return true;
                        }

                        auto                         resp     = nlohmann::json::object( );
                        auto                         results  = nlohmann::json::array( );
                        auto                         uri      = std::unique_ptr< Uri >( Uri::parse(
                          std::string( "http://host/" ) + request.target( ).to_string( ) ) );
                        size_t                       offset   = 0;
                        size_t                       max      = 0;
                        bool                         asc_sort = true;
                        auto                         expies   = uri->getQuery( "expiration" );
                        std::string                  sort_field;
                        std::vector< dbcpp::DBTime > expirations;

                        std::transform( expies.begin( ),
                                        expies.end( ),
                                        std::back_inserter( expirations ),
                                        &token::api::marshal::token::toTime );

                        auto offsets  = uri->getQuery( "offset" );
                        auto maxes    = uri->getQuery( "limit" );
                        auto sort     = uri->getQuery( "sort" );
                        auto sort_key = uri->getQuery( "sortField" );

                        if ( !offsets.empty( ) ) {
                          offset = std::stoll( offsets[ 0 ], nullptr );
                        }

                        if ( !maxes.empty( ) ) {
                          max = std::stoll( maxes[ 0 ], nullptr );
                        }

                        if ( !sort.empty( ) ) {
                          asc_sort = std::tolower( sort[ 0 ][ 0 ] ) == 'a';
                        }

                        if ( !sort_key.empty( ) ) {
                          sort_field = sort_key[ 0 ];
                        }

                        auto entries = manager->query( params[ "vault" ],
                                                       uri->getQuery( "token" ),
                                                       uri->getQuery( "value" ),
                                                       expirations,
                                                       sort_field,
                                                       asc_sort,
                                                       offset,
                                                       max,
                                                       &count );

                        for ( auto &entry : entries ) {
                          auto row = nlohmann::json::object( );
                          Marshal( ).from( entry ).to( row );
                          results.emplace_back( row );
                        }

                        resp[ "offset" ]  = offset;
                        resp[ "limit" ]   = max;
                        resp[ "count" ]   = count;
                        resp[ "results" ] = results;

                        response_set( response, resp );

                        return true;
                      } );

        service->get( "/vaults/{vault}/status",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        tracker< gauge_type >     reqtrack( *req_count );
                        tracker< histogram_type > dur_track( *resp_time );
                        auto                      resp = nlohmann::json::object( );
                        Marshal{ manager->status( params[ "vault" ] ) }.to( resp );
                        response_set( response, resp );
                        return true;
                      } );

        service->get( "/status",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        tracker< gauge_type >     reqtrack( *req_count );
                        tracker< histogram_type > dur_track( *resp_time );
                        auto                      resp = nlohmann::json::object( );
                        Marshal{ manager->status( ) }.to( resp );
                        response_set( response, resp );
                        return true;
                      } );

        service->get( "/metrics",
                      [ this ]( service_type::param_map_type &params,
                                service_type::request_type &  request,
                                service_type::response_type & response ) -> bool {
                        auto collected = registry.Collect( );

                        response.set( boost::beast::http::field::content_type, "text/plain" );
                        response.body( ) = prometheus::TextSerializer( ).Serialize( collected );
                        return true;
                      } );
      }

      void response_set( service_type::response_type &response, nlohmann::json &json ) {
        response.set( boost::beast::http::field::content_type, "application/json" );
        response.body( ) = json.dump( 2 );

        if ( json.find( "error" ) != json.end( ) ) {
          req_bad->Increment( );
        } else {
          req_good->Increment( );
        }
      }

      int run( ) {
        service->start( );
        service->join( );
        executor->halt( );
        return 0;
      }
    }; // namespace app
  }    // namespace app
} // namespace token

#endif // __TOKENIZATION_HH_
