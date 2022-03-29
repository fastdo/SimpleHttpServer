#pragma once

#include <winux.hpp>
#include <eiennet.hpp>
#include <eienwebx.hpp>

namespace v2
{

#define _DEFINE_EVENT_RELATED(evtname, paramtypes, calledparams) \
public: \
    using evtname##HandlerFunction = std::function< void paramtypes >; \
    void on##evtname##Handler( evtname##HandlerFunction handler ) \
    { \
        this->_##evtname##Handler = handler; \
    } \
protected: \
    evtname##HandlerFunction _##evtname##Handler; \
    virtual void on##evtname##paramtypes \
    { \
        if ( this->_##evtname##Handler ) this->_##evtname##Handler##calledparams; \
    }

#define _DEFINE_EVENT_RELATED_EX(evtname, paramtypes) \
public: \
    using evtname##HandlerFunction = std::function< void paramtypes >; \
    void on##evtname##Handler( evtname##HandlerFunction handler ) \
    { \
        this->_##evtname##Handler = handler; \
    } \
protected: \
    evtname##HandlerFunction _##evtname##Handler; \
    virtual void on##evtname##paramtypes

#define _DEFINE_EVENT_RETURN_RELATED_EX(ret, evtname, paramtypes) \
public: \
    using evtname##HandlerFunction = std::function< ret paramtypes >; \
    void on##evtname##Handler( evtname##HandlerFunction handler ) \
    { \
        this->_##evtname##Handler = handler; \
    } \
protected: \
    evtname##HandlerFunction _##evtname##Handler; \
    virtual ret on##evtname##paramtypes


}
