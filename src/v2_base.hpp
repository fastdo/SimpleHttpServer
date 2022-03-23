#pragma once

#include <winux.hpp>
#include <eiennet.hpp>
#include <eienwebx.hpp>

namespace v2
{
using namespace eiennet;
using namespace eienwebx;

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


// 是否输出提示
extern bool outputVerbose;

}
