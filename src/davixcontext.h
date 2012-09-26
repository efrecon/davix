#ifndef DAVIXCONTEXT_H
#define DAVIXCONTEXT_H

#include <tr1/memory>
#include <contextconfig.h>


namespace Davix{

class PosixGate;
class HttpGate;
class ContextInternal;

/// @brief Main Entry point for Davix
/// a Davix context is a independant instance of Davix
/// Each instance of Davix has its own session-reuse pool and parameters
class Context : public ContextConfig
{
public:
    /// create a new context for Davix
    Context();
    Context(const Context & c);

    virtual ~Context();

    /// clone this instance to a new context dynamically allocated,
    /// the new context inherit of a copy of all the parent context parameters
    /// this context need to be destroyed after usage
    Context* clone();

    /// POSIX-like gate
    /// provide the File POSIX-oriented operations
    /// this gate need to be before the destruction of its context
    PosixGate & posixGate();


    /// standard plain Http request Gate
    /// provide the Http
    HttpGate& httpGate();

protected:
    // internal context
    std::tr1::shared_ptr<ContextInternal> _intern;
    std::auto_ptr<PosixGate> p_gate;
    std::auto_ptr<HttpGate> h_gate;
    Glib::Mutex mux_gate;
    friend class PosixGate;
    friend class httpGate;

};

}

#endif // DAVIXCONTEXT_H
