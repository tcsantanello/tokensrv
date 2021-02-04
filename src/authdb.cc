#include "authdb.hh"
#include <iostream>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include "ratelimit-1.h"
#include "ratelimit-2.h"

namespace token {
  namespace app {
    void AuthTokenDB::init( ) {
      auto connection = dbPool.getConnection( );
      bool commit     = false;

      if ( !( connection << "SELECT 1 FROM pg_tables WHERE tablename = ?"
                         << "vaults" )
              .executeQuery( )
              .next( ) ) {
        ( connection << ( "CREATE TABLE vaults ( "
                          "  id         SERIAL PRIMARY KEY, "
                          "  format     INTEGER, "
                          "  alias      VARCHAR(255), "
                          "  tablename  VARCHAR(255), "
                          "  enckey     VARCHAR(255), "
                          "  mackey     VARCHAR(255), "
                          "  durable    BOOLEAN, "
                          "  CONSTRAINT vaults_alias_key UNIQUE ( alias ), "
                          "  CONSTRAINT vaults_name_key UNIQUE ( tablename ) "
                          ")" ) )
          .execute( );
        commit = true;
      }

      if ( !( connection << "SELECT 1 FROM pg_tables WHERE tablename = ?"
                         << "users" )
              .executeQuery( )
              .next( ) ) {
        ( connection
          << ( "CREATE TABLE users ( "
               "  id         SERIAL PRIMARY KEY,"
               "  username   VARCHAR(50) NOT NULL,"
               "  password   CHAR(64),"
               "  token      VARCHAR(25),"
               "  CONSTRAINT users_username_key UNIQUE( username ),"
               "  CONSTRAINT users_token_key    UNIQUE( token ),"
               "  CONSTRAINT users_auth_key CHECK ( password is not null or token is not null )"
               ")" ) )
          .execute( );
        commit = true;
      }

      if ( !( connection << "SELECT 1 FROM pg_tables WHERE tablename = ?"
                         << "user_vaults" )
              .executeQuery( )
              .next( ) ) {
        ( connection << ( "CREATE TABLE user_vaults ( "
                          "  id         SERIAL PRIMARY KEY,"
                          "  userid     INTEGER NOT NULL,"
                          "  vault      INTEGER NOT NULL,"
                          "  CONSTRAINT user_vaults_vault_fk  FOREIGN KEY ( vault ) REFERENCES "
                          "             vaults( id ) ON DELETE CASCADE,"
                          "  CONSTRAINT user_vaults_userid_fk FOREIGN KEY ( userid ) REFERENCES "
                          "             users( id ) ON DELETE CASCADE,"
                          "  CONSTRAINT user_vaults_uid_vault UNIQUE ( userid, vault )"
                          ")" ) )
          .execute( );
        commit = true;
      }

      if ( !( connection << "SELECT 1 FROM pg_tables WHERE tablename = ?"
                         << "user_limits_config" )
              .executeQuery( )
              .next( ) ) {
        ( connection << ( "CREATE TABLE user_limits_config ("
                          "  id         SERIAL PRIMARY KEY,"
                          "  uvault_id  INTEGER NOT NULL,"
                          "  period     INTERVAL NOT NULL,"
                          "  value      INTEGER NOT NULL,"
                          "  CONSTRAINT user_limit_vaults_fk FOREIGN KEY ( uvault_id ) REFERENCES "
                          "             user_vaults( id ) ON DELETE CASCADE"
                          ")" ) )
          .execute( );
        commit = true;
      }

      if ( !( connection << "SELECT 1 FROM pg_tables WHERE tablename = ?"
                         << "user_limits" )
              .executeQuery( )
              .next( ) ) {
        ( connection << ( "CREATE TABLE user_limits ("
                          "  id         SERIAL PRIMARY KEY,"
                          "  config_id  INTEGER NOT NULL,"
                          "  expire     TIMESTAMP NOT NULL,"
                          "  value      INTEGER NOT NULL,"
                          "  CONSTRAINT user_limits_config_fk FOREIGN KEY ( config_id ) REFERENCES "
                          "             user_limits_config( id ) ON DELETE CASCADE"
                          ")" ) )
          .execute( );
        commit = true;
      }

      if ( !( connection << "SELECT 1 FROM pg_proc WHERE proname = ?"
                         << "user_limit" )
              .executeQuery( )
              .next( ) ) {
        ( connection << ( std::string{ ( char * ) ratelimit_1_sql } ) ).execute( );
        ( connection << ( std::string{ ( char * ) ratelimit_2_sql } ) ).execute( );
        commit = true;
      }

      if ( commit ) {
        connection.commit( );
      }
    }

    bool AuthTokenDB::create_user( std::string user, std::string pass, std::string token ) {
      auto connection = dbPool.getConnection( );
      auto statement  = connection << "INSERT INTO users ( username, password, token )"
                                     " VALUES ( ?, encode( digest( ?, 'sha256' ), 'hex' ), ? )"
                                  << user << pass << token;
      auto rc = statement.executeUpdate( ) > 0;
      connection.commit( );
      return rc;
    }

    bool AuthTokenDB::grant_user( std::string user, std::string vault ) {
      auto connection = dbPool.getConnection( );
      auto statement  = connection << "INSERT INTO user_vaults( userid, vault )"
                                     " SELECT u.id, v.id"
                                     "   FROM users u, vaults v"
                                     "  WHERE u.username = ? "
                                     "    AND ? in ( v.alias, v.tablename )"
                                  << user << vault;
      auto rc = statement.executeUpdate( ) > 0;
      connection.commit( );
      return rc;
    }

    bool AuthTokenDB::limit_user( std::string user,
                                  std::string vault,
                                  int         count,
                                  std::string period ) {
      auto connection = dbPool.getConnection( );
      auto statement  = connection << "INSERT INTO user_limits_config( uvault_id, period, value )"
                                     " SELECT uv.id, ?::interval, ?"
                                     "   FROM user_vaults uv, "
                                     "        users u, "
                                     "        vaults v "
                                     "  WHERE u.username = ? "
                                     "    AND uv.userid = u.id"
                                     "    AND uv.vault = v.id"
                                     "    AND ? in ( v.alias, v.tablename )"
                                  << period << count << user << vault;
      auto rc = statement.executeUpdate( ) > 0;
      connection.commit( );
      return rc;
    }

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

        if ( rs.next( ) ) {
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
