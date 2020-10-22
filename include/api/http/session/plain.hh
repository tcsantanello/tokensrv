#ifndef __SESSION_PLAIN_H_
#define __SESSION_PLAIN_H_

#include "api/http/base.hh"
#include "api/http/session/basic.hh"

namespace token {
  namespace api {
    namespace http {
      template < typename Traits = DefaultTypeTraits >
      class PlainSession;

      template < typename Traits >
      class PlainSession : public Session< PlainSession< Traits >, Traits >,
                           public std::enable_shared_from_this< PlainSession< Traits > > {
        using self_type   = PlainSession< Traits >;
        using base_type   = Session< self_type, Traits >;
        using config_type = RouteConfig< Traits >;

       public:
        PlainSession( io_service_type *                io,
                      std::shared_ptr< config_type >   _rc,
                      std::shared_ptr< executor_type > _executor )
          : base_type( io, std::move( _rc ), std::move( _executor ) ) {}

        PlainSession( buffer_type &&                    _buffer,
                      socket_type &&                    _sock,
                      strand_type &&                    _strand,
                      std::shared_ptr< config_type >    _rc,
                      std::shared_ptr< executor_type >  _exec,
                      std::shared_ptr< spdlog::logger > _logger,
                      std::string &&                    _local,
                      std::string &&                    _remote )
          : base_type( std::move( _buffer ),
                       std::move( _sock ),
                       std::move( _strand ),
                       std::move( _rc ),
                       std::move( _exec ),
                       std::move( _logger ),
                       std::move( _local ),
                       std::move( _remote ) ) {}

        tcp_type::socket &transport( ) { return this->sock; }
      };
    } // namespace http
  }   // namespace api
} // namespace token

#endif // __SESSION_PLAIN_H_
