#ifndef __AUTHDB_H_
#define __AUTHDB_H_

#include <cstdint>
#include <token/api/core/database.hh>

namespace token {
  namespace app {
    class AuthTokenDB : public token::api::core::TokenDB {
     public:
      AuthTokenDB( Uri *uri, size_t cxnCount )
        : TokenDB( uri, cxnCount ) {
        init( );
      }
      AuthTokenDB( std::string uri, size_t cxnCount )
        : TokenDB( uri, cxnCount ) {
        init( );
      }

      void         init( );
      uint32_t     authorizedCreds( std::string user, std::string pass );
      uint32_t     authorizedToken( std::string token );
      uint32_t     authorizedBasic( std::string encoded );
      bool         accessible( uint32_t uid, std::string vault );
      uint32_t     rate_limit( uint32_t user, std::string vault, uint32_t count );
      virtual bool createVault( const token::api::core::VaultInfo &vault ) override;
      bool         create_user( std::string user, std::string password, std::string token );
      bool         grant_user( std::string user, std::string vault );
      bool         limit_user( std::string user, std::string vault, int count, std::string period );
    };
  } // namespace app
} // namespace token

#endif // __AUTHDB_H_
