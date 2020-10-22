#ifndef __TYPE_STATUS_H_
#define __TYPE_STATUS_H_

#include "api/marshal/types/base.hh"
#include "token/api/manager.hh"

namespace token {
  namespace api {
    namespace marshal {
      struct status {
        struct getter : public base::getter {
          const token::api::Status &status_;

          explicit getter( const token::api::Status &s )
            : status_( s ) {}

          std::string status( ) { return status_.description( ); }
        };
      };
    } // namespace marshal
  }   // namespace api
} // namespace token

#endif // __TYPE_STATUS_H_
