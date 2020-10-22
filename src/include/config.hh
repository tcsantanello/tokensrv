#ifndef __CONFIG_H_
#define __CONFIG_H_

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <spdlog/spdlog.h>

namespace token {
  namespace app {

    /** Default database pool size */
#define DATABASE_POOL_SIZE_DEFAULT 1
    /** Default worker thread count */
#define WORKER_POOL_SIZE_DEFAULT std::thread::hardware_concurrency( )
    /** Default HTTP/REST IO thread count */
#define HTTP_POOL_SIZE_DEFAULT 1
    /** Default HTTP/REST address */
#define HTTP_REST_ADDRESS_DEFAULT "::"
    /** Default HTTP/REST port */
#define HTTP_REST_PORT_DEFAULT 8080

    /**
     * @brief Application configuration class wrapper
     */
    class Config {
      /** Loaded configuration */
      boost::property_tree::ptree config;

     public:
      /**
       * @brief 'Main' constructor; loads the configuration file based on the application name
       */
      Config( int argc, const char *argv[] ) {
        std::string ini = std::string{ argv[ 0 ] } + std::string{ ".ini" };
        boost::property_tree::read_ini( ini, config );
        spdlog::set_level( getLogLevel( ) );
      }

      /**
       * @brief Get the database connection string
       * @return configured database url
       */
      std::string databaseUrl( ) const { return config.get< std::string >( "database.url" ); }

      /**
       * @brief Get the database connection pool size
       * @return configured database pool size, or 1 if unconfigured
       */
      int databasePoolSize( ) const { return config.get( "database.pool_size", DATABASE_POOL_SIZE_DEFAULT ); }

      /**
       * @brief Get the HTTP/REST listener address
       * @return configured listener address or [::] if unconfigured
       */
      std::string listenerAddress( ) const {
        return config.get( "http.rest.address", std::string{ HTTP_REST_ADDRESS_DEFAULT } );
      }

      /**
       * @brief Get the HTTP/REST listener port
       * @return configured listener port or 8080 if unconfigured
       */
      int listenerPort( ) const { return config.get( "http.rest.port", HTTP_REST_PORT_DEFAULT ); }

      /**
       * @brief Get the HTTP/REST IO pool size
       * @return configured pool size, or 1 if unconfigured
       */
      int restPoolSize( ) const { return config.get( "http.rest.pool_size", HTTP_POOL_SIZE_DEFAULT ); }

      /**
       * @brief Get the number of worker threads
       * @return configured pool size or CPU core count if unconfigured
       */
      int workerPoolSize( ) const { return config.get( "worker.pool_size", WORKER_POOL_SIZE_DEFAULT ); }

      /**
       * @brief Get the configured log level
       * @return configured log level, defaults to 'info' level
       */
      spdlog::level::level_enum getLogLevel( ) const {
        return spdlog::level::from_str( config.get< std::string >( "log.level", "info" ) );
      }
    };

  } // namespace app
} // namespace token

#endif // __CONFIG_H_
