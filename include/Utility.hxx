#pragma once

#ifndef UTILITY_HXX
#define UTILITY_HXX

#include "Utility.hxx"
#include <random>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>

// Forward declare the proxy function inside the namespace
namespace SimCore
{
    void routeLogToGui(int level, const QString& msg);
}

#define SIM_LOG(level, msg) \
do { \
    ACE_DEBUG ((level, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), msg)); \
} while (0)

namespace Manager
{
    class Utility
    {
        public:
    };

} // namspace Utility

#endif // UTILITY_HXX
