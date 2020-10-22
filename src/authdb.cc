#include "authdb.hh"
#include <iostream>
#include <openssl/bio.h>
#include <openssl/evp.h>

namespace token {
  namespace app {
    uint32_t AuthTokenDB::authorizedCreds( std::string user, std::string pass ) {
      auto connection = dbPool.getConnection( );
      auto statement  = connection << "SELECT id FROM users WHERE username = ? AND password = "
                                     "encode( digest( ?, 'sha256' ), 'hex' )"
                                  << user << pass;
      auto rs = statement.executeQuery( );
      return rs.next( ) ? rs.get< int32_t >( 0 ) : 0;
    }

    uint32_t AuthTokenDB::authorizedToken( std::string token ) {
      auto connection = dbPool.getConnection( );
      auto statement  = connection << "SELECT id FROM users WHERE token = ?" << token;
      auto rs         = statement.executeQuery( );
      return rs.next( ) ? rs.get< int32_t >( 0 ) : 0;
    }

    uint32_t AuthTokenDB::authorizedBasic( std::string encoded ) {
      std::vector< char >    decoded;
      std::shared_ptr< BIO > b64( BIO_new( BIO_f_base64( ) ),
                                  []( BIO *bio ) { BIO_free_all( bio ); } );

      decoded.resize( encoded.size( ) * 3 / 4 );

      BIO_push(
        b64.get( ),
        BIO_new_mem_buf( reinterpret_cast< void * >( const_cast< char * >( encoded.c_str( ) ) ),
                         encoded.size( ) ) );
      BIO_set_flags( b64.get( ), BIO_FLAGS_BASE64_NO_NL );

      auto len = BIO_read( b64.get( ), &decoded[ 0 ], decoded.size( ) );
      decoded.resize( len );

      auto colon = std::find( decoded.begin( ), decoded.end( ), ':' );

      if ( colon == decoded.end( ) ) {
        return 0;
      }

      return authorizedCreds( std::string( &decoded[ 0 ], colon - decoded.begin( ) ),
                              std::string( &*( colon + 1 ), decoded.end( ) - colon - 1 ) );
    }

    bool AuthTokenDB::accessible( uint32_t uid, std::string vault ) {
      auto connection = dbPool.getConnection( );
      auto statement  = connection << "SELECT 1 FROM user_vaults WHERE userid = ? AND vault = ?"
                                  << uid << vault;
      auto rs = statement.executeQuery( );
      return rs.next( ) ? rs.get< int32_t >( 0 ) : 0;
    }

    uint32_t AuthTokenDB::rate_limit( uint32_t user, std::string vault, uint32_t count ) {
      uint32_t rc         = -1;
      auto     connection = dbPool.getConnection( );

      try {
        auto statement = connection << "SELECT user_limit( ?::integer, ?, ?::integer )" << user
                                    << vault << count;
        auto rs = statement.executeQuery( );

        if ( rs.next( ) ) {
          rc = rs.get< uint32_t >( 0 );
        }

        connection.commit( );
      } catch ( ... ) {
        connection.rollback( );
      }

      return rc;
    }

    bool AuthTokenDB::createVault( const token::api::core::VaultInfo &vault ) {
      auto        connection = dbPool.getConnection( );
      std::string constraints;

      try {
        auto rs =
          ( connection
            << "SELECT 1 FROM vaults WHERE ? IN ( tablename, alias ) OR ? IN ( tablename, alias )"
            << vault.table << vault.alias )
            .executeQuery( );

        if ( !rs.next( ) ) {
          return false;
        }

        if ( rs.get< int32_t >( 0 ) ) {
          std::cerr << "Unable to create vault; name or table already exists\n";
          return false;
        }

        std::cout << "  Creating token vault " << vault.alias << ": ";

        if ( vault.durable ) {
          constraints = fmt::format(
            "CONSTRAINT {}_pkey PRIMARY KEY ( token ),"
            "CONSTRAINT {}_hmac_key UNIQUE ( hmac )",
            vault.table,
            vault.table );
        } else {
          constraints = fmt::format( "CONSTRAINT {}_tran_tok_key UNIQUE ( token )", vault.table );
        }

        std::cout << " table";
        ( connection << fmt::format( "CREATE TABLE {} ("
                                     "  token         VARCHAR( {} ) NOT NULL,"
                                     "  hmac          BYTEA, "
                                     "  crypt         BYTEA, "
                                     "  mask          VARCHAR( {} ), "
                                     "  expiration    DATE, "
                                     "  properties    BYTEA, "
                                     "  enckey        VARCHAR( 255 ), "
                                     "  creation_date DATE DEFAULT NOW(), "
                                     "  last_used     DATE DEFAULT NOW(), "
                                     "  last_updated  DATE DEFAULT NOW(), "
                                     "  {} )",
                                     vault.table,
                                     vault.length,
                                     vault.length,
                                     constraints ) )
          .execute( );

        std::cout << " entry\n";

        auto stmt = connection << "INSERT INTO vaults ( format, alias, tablename, enckey, mackey, "
                                  "durable ) VALUES ( ?, ?, ?, ?, ?, ? )";

        stmt << vault.format;
        stmt << vault.alias;
        stmt << vault.table;
        stmt << vault.encKeyName;
        stmt << vault.macKeyName;
        stmt << vault.durable;

        std::cout << "Creation complete\n";

        if ( stmt.executeUpdate( ) > 0 ) {
          connection.commit( );
          return true;
        }
      } catch ( std::exception &ex ) {
        std::cerr << fmt::format( "Error encountered while creating new vault: {}\n", ex.what( ) );
      }
      return false;
    }
  } // namespace app
} // namespace token
