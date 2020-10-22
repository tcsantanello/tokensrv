/* -*- Mode: c++ -*- */
#ifndef __EXECUTOR_HH__
#define __EXECUTOR_HH__

#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace token {
  namespace async {

    class Executor {
     public:
      /** Asynchronous action */
      using Runnable = std::function< void( void ) >;
      /** Get the return type of a function */
      template < typename Function, typename... Args >
      using result_type_t = typename std::result_of< Function( Args... ) >::type;

      /**
       * @brief Default constructor, creates a thread pool based on the core count
       */
      Executor( )
        : Executor( std::thread::hardware_concurrency( ) ) {}

      /**
       * @brief Explicit executor constructor, creates a thread pool of the specified size
       * @param count number of threads
       */
      explicit Executor( int count ) {
        if ( count < 0 ) {
          count = std::thread::hardware_concurrency( );
        }

        for ( int num = 0; num < count; ++num ) {
          pool.emplace_back( std::thread( [this] {
            do {
              Runnable runnable = nullptr;

              {
                std::unique_lock< std::mutex > guard( lock );
                cond.wait( guard, [this]( ) { return ( !queue.empty( ) ) || ( !running( ) ); } );

                if ( !running( ) ) {
                  return;
                }

                runnable = queue.front( );
                queue.pop( );
              }

              if ( runnable != nullptr ) {
                runnable( );
              }
            } while ( running( ) );
          } ) );
        }
      }

      /**
       * @brief Shutdown the executor
       *
       * Clears the processing state of the executor (clears queue) and terminates the threads
       */
      void halt( ) {
        if ( running( ) ) {
          std::lock_guard< std::mutex > guard( lock );
          while ( !queue.empty( ) ) {
            queue.pop( );
          }
          done = true;
          cond.notify_all( );
        }

        for ( auto &thread : pool ) {
          thread.join( );
        }
      }

      /**
       * @brief Identify if the executor is still active
       * @return true if active, false if not
       */
      bool running( ) { return !done; }

      /**
       * @brief Add a method to call at another point in time
       * @param callable executable function
       */
      template < typename Function,
                 typename Result                                                        = result_type_t< Function >,
                 typename std::enable_if< std::is_void< Result >::value, void >::type * = nullptr >
      void add( const Function &callable ) {
        if ( running( ) ) {
          if ( pool.empty( ) ) {
            callable( );
            return;
          }

          std::lock_guard< std::mutex > guard( lock );
          queue.push( callable );
          cond.notify_one( );
        }
      }

      /**
       * @brief Add a method to call at another point in time
       * @param callable executable function
       * @param args function arguments
       */
      template < typename Function,
                 typename... Args,
                 typename Result = result_type_t< Function, Args... >,
                 typename std::enable_if< std::is_void< Result >::value, void >::type * = nullptr,
                 typename std::enable_if< sizeof...( Args ) != 0, void >::type *        = nullptr >
      void add( const Function &callable, Args... args ) {
        add( [=]( ) { callable( args... ); } );
      }

      /**
       * @brief Add a method to call at another point in time
       * @param callable executable function
       * @param args function arguments
       */
      template < typename Function,
                 typename... Args,
                 typename Result = result_type_t< Function, Args... >,
                 typename std::enable_if< !std::is_void< Result >::value, void >::type * = nullptr >
      std::future< Result > add( const Function &callable, Args... args ) {
        auto pr = std::make_shared< std::promise< Result > >( );
        add( [=]( ) { pr->set_value( callable( args... ) ); } );
        return pr->get_future( );
      }

     protected:
      /** Thread pool */
      std::vector< std::thread > pool;
      /** Synchronization for the queue */
      std::mutex lock;
      /** Work queue */
      std::queue< Runnable > queue;
      /** Work item notification primitive */
      std::condition_variable cond;
      /** Completion flag */
      bool done = false;
    };
  } // namespace async
} // namespace token

#endif
