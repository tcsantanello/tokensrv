
#ifndef __TOKENIZATION_SERVICE_HH__
#define __TOKENIZATION_SERVICE_HH__

#include "base.hh"
#include "listener.hh"
#include "route_config.hh"

namespace token {
  namespace api {
    namespace http {
#define TOKEN_API_HTTP_SERVICE_LOG_ID "token::api::http::service"

      template < typename Traits = DefaultTypeTraits >
      class Service : public std::enable_shared_from_this< Service< Traits > >,
                      public RouteConfig< Traits >,
                      public Traits {
       public:
        using response_type  = typename Traits::response_type;
        using request_type   = typename Traits::request_type;
        using param_map_type = typename Traits::param_map_type;

       private:
        using listener_type = Listener< Traits >;
        using config_type   = RouteConfig< Traits >;

        std::shared_ptr< executor_type > executor;
        io_service_type                  ioc;
        thread_group                     threads;
        bool                             running     = false;
        int                              threadCount = 0;

        void thread_main( ) {
          while ( running ) {
            ioc.run( );
          }
        }

        void init( std::shared_ptr< executor_type > executor, int threadCount ) {
          this->logger = token::api::create_logger( TOKEN_API_HTTP_SERVICE_LOG_ID, { } );

          if ( !executor ) {
            executor = std::make_shared< executor_type >( 0 );
          }

          if ( threadCount < 1 ) {
            threadCount = std::thread::hardware_concurrency( );
            LOG(
              this->logger, err, "Invalid thread count supplied, defaulting to {}", threadCount );
          }

          this->executor    = executor;
          this->threadCount = threadCount;
          this->running     = false;

          LOG( this->logger, trace, "HTTP Service created: {} threads", threadCount );
        }

       public:
        Service( ) { init( nullptr, std::thread::hardware_concurrency( ) ); }

        explicit Service( int count ) { init( nullptr, count ); }
        explicit Service( std::shared_ptr< executor_type > executor ) {
          init( executor, std::thread::hardware_concurrency( ) );
        }
        Service( std::shared_ptr< executor_type > executor, int threadCount ) {
          init( executor, threadCount );
        }

        void start( ) {
          if ( !running ) {
            running = true;

            for ( auto num = 0; num < threadCount; ++num ) {
              threads.emplace_back(
                std::thread( std::bind( &Service::thread_main, this->shared_from_this( ) ) ) );
            }

            LOG( this->logger, trace, "Started {} threads", threadCount );
          }
        }

        void run( ) {
          start( );
          join( );
        }

        void stop( ) {
          running = false;
          join( );
          threads.clear( );
        }

        void join( ) {
          for ( auto &thread : threads ) {
            thread.join( );
          }
        }

        void addListener( const std::string &       address,
                          const std::string &       service,
                          networking::ssl::context *ctx = nullptr ) {
          auto resolved = boost::asio::ip::tcp::resolver( ioc ).resolve(
            address,
            service,
            boost::asio::ip::tcp::resolver::passive | boost::asio::ip::tcp::resolver::v4_mapped |
              boost::asio::ip::tcp::resolver::all_matching );

          std::make_shared< listener_type >(
            std::shared_ptr< io_service_type >( this->shared_from_this( ), &ioc ),
            std::dynamic_pointer_cast< config_type >( this->shared_from_this( ) ),
            ctx,
            resolved->endpoint( ),
            executor )
            ->start( );
        }

        void addListener( const std::string &       address,
                          int                       port,
                          networking::ssl::context *ctx = nullptr ) {
          addListener( address, std::to_string( port ), ctx );
        }
      };

      using HttpService = Service<>;
    } // namespace http
  }   // namespace api
} // namespace token

#endif //__TOKENIZATION_SERVICE_HH__
