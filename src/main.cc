
#include "opensslprovider.hh"
#include "tokenization.hh"
#include <token/api.hh>

#include <spdlog/spdlog.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

const std::string ssl_key =
  "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
  "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\n"
  "RVyUYYKoMV5IAK6ktBSVnA==\n"
  "-----END ENCRYPTED PRIVATE KEY-----";

const std::string ssl_cert =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n";

void log_init( void ) {
  auto        logfile = std::make_shared< spdlog::sinks::basic_file_sink_mt >( "log.txt", true );
  auto        console = std::make_shared< spdlog::sinks::stdout_color_sink_mt >( );
  std::string names[] = { "dbcpp::psql",
                          "dbcpp::sqlite",
                          "dbcpp::Pool",
                          "dbcpp::Driver",
                          "token::api::manager",
                          "token::api::tokendb",
                          TOKEN_API_HTTP_SESSION_LOG_ID,
                          TOKEN_API_HTTP_SERVICE_LOG_ID,
                          TOKEN_API_HTTP_LISTENER_LOG_ID };

  for ( auto &&name : names ) {
    token::api::create_logger( name, { console, logfile } )->set_level( spdlog::level::trace );
  }
}

int main( int argc, const char *argv[] ) {
  log_init( );

  return token::app::Tokenization< token::crypto::OpenSSL::Provider >( argc, //
                                                                       argv,
                                                                       ssl_key,
                                                                       ssl_cert )
    .run( );
}
