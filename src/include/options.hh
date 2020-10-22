
#ifndef __TOKENIZATION_OPTIONS_HH__
#define __TOKENIZATION_OPTIONS_HH__

#include "token/crypto/provider.hh"
#include <boost/program_options.hpp>
#include <iostream>
#include <unistd.h>
#include <vector>

namespace po = boost::program_options;

class Options {
  std::vector< const char * >        args;
  po::variables_map                  result;
  po::options_description            allOptions;
  po::options_description            start;
  po::options_description            general;
  po::options_description            vault;
  po::options_description            crypto;
  po::options_description            hmac;
  po::positional_options_description pod;
  std::vector< std::string >         groups{ "start", "stop", "status", "vault" };

  Options( std::shared_ptr< token::crypto::Provider > provider )
    : allOptions( "Options" )
    , start( "start - Service start" )
    , general( "General Options" )
    , vault( "vault - Vault Options" )
    , crypto( "crypto - Encryption Key Options" )
    , hmac( "hmac - HMAC Key Options" ) {

    general.add_options( )                    //
      ( "help,h", "Print this help message" ) //
      ( "version,v", "Print the application version" );

    start.add_options( )                                           //
      ( "foreground,g", "Run the application in the foreground" ); //

    vault.add_options( )                                                              //
      ( "create,c", "Create a new vault" )                                            //
      ( "rekey,r", "Rekey a vault" )                                                  //
      ( "vault-name,n", po::value< std::string >( ), "Vault Name" )                   //
      ( "vault-key,k", po::value< std::string >( ), "New vault encryption key name" ) //
      ( "vault-hmac,a", po::value< std::string >( ), "New vault hmac key name" )      //
      ( "vault-format,f", po::value< int >( ), "New vault token format" )             //
      ( "single-use,s", "New vault generates single-use tokens" )                     //
      ( "data-length,l", po::value< int >( ), "New vault value and token length" );

    provider->cmdArgs( crypto, hmac );

    allOptions.add( general ).add( start ).add( vault );

    if ( !crypto.options( ).empty( ) ) {
      allOptions.add( crypto );
      crypto.add_options( )                           //
        ( "create,c", "Create a new encryption key" ) //
        ( "remove,r", "Remove an existing encryption key" );
      groups.push_back( "crypto" );
    }

    if ( !hmac.options( ).empty( ) ) {
      allOptions.add( hmac );
      hmac.add_options( ) //
        ( "create,c", "Create a new HMAC key" );
      groups.push_back( "hmac" );
    }

    auto moduleDesc = ( std::string( "Action or group ( " ) + [ & ]( std::vector< std::string > list ) -> std::string {
      if ( list.size( ) == 1 ) {
        return list[ 0 ];
      } else {
        std::stringstream ss;
        auto              iter = list.begin( );

        ss << *list.begin( );

        for ( ++iter; iter != list.end( ) - 1; ++iter ) {
          ss << ", " << *iter;
        }

        ss << " or " << *iter;
        return ss.str( );
      }
    }( groups ) + " )" );

    allOptions.add_options( )( "module,m", po::value< std::string >( )->required( ), moduleDesc.c_str( ) );

    pod.add( "module", 1 );
  }

 public:
  Options( std::shared_ptr< token::crypto::Provider > provider, int argc, const char *argv[] )
    : Options( std::move( provider ) ) {
    args.assign( argv, argv + argc );
  }

  Options &parse( ) {
    try {
      po::store( po::command_line_parser( args.size( ), &args[ 0 ] ).options( allOptions ).positional( pod ).run( ),
                 result );
    } catch ( po::required_option &e ) {
      std::cout << e.what( ) << "\n";
      exit( 1 );
    } catch ( po::error &e ) {
      std::cout << e.what( ) << "\n";
      exit( 2 );
    }
    return *this;
  }

  void printHelp( ) {
    if ( has( "module" ) ) {
      auto module = result[ "module" ].as< std::string >( );
      std::cout << general << "\n";
      if ( module == "vault" ) {
        std::cout << vault << "\n";
      } else if ( module == "crypto" ) {
        std::cout << crypto << "\n";
      } else if ( module == "hmac" ) {
        std::cout << hmac << "\n";
      } else if ( module == "start" ) {
        std::cout << start << "\n";
      }
    } else {
      std::cout << allOptions << "\n";
    }
  }

  const std::vector< const char * > parameters( ) const { return args; }

  bool wantHelp( ) { return has( "help" ) || ( args.size( ) == 1 ) || !has( "module" ); }
  bool has( std::string name ) { return result.count( name ) > 0; }
  template < typename T >
  T get( std::string name ) {
    return result[ name ].as< T >( );
  }
};

#endif //__TOKENIZATION_OPTIONS_HH__
