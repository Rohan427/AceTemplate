#pragma once

#ifndef COMPATIBILITY_HXX
#define COMPATIBILITY_HXX

#include <ace/Version.h>
#include <string>

/*
 * CPP_STD_VERSION will be set to:
 *   0         → Unknown / pre-standard
 *   199711L   → C++98/03
 *   201103L   → C++11
 *   201402L   → C++14
 *   201703L   → C++17
 *   202002L   → C++20
 *   202302L   → C++23
 *
 * Works around GCC <= 4.6 bug where __cplusplus == 1.
 */

#if defined(__GNUC__) && (__GNUC__ < 5)
    // GCC 4.x and early 5.x quirks
    #if (__GNUC__ == 4 && __GNUC_MINOR__ <= 6)
        // GCC 4.4.x, 4.5.x, 4.6.x — __cplusplus is always 1
        #define CPP_STD_VERSION 199711L
    #else
        // GCC 4.7+ reports correct __cplusplus
        #define CPP_STD_VERSION __cplusplus
    #endif

#else
    // All other compilers — trust __cplusplus
    #define CPP_STD_VERSION __cplusplus
#endif

// Compatibility layer
#if defined(__clang__) && defined(__HIP__)
    #pragma clang diagnostic ignored "-Wc++11-extensions"
    #pragma clang diagnostic ignored "-Wc++17-extensions"
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// ACE comptibility checks
#if ACE_MAJOR_VERSION >= 6
    #define ACE_VERSION_6_OR_HIGHER

    

    #include <ace/Barrier.h>
    #include <ace/Condition_T.h>
    #include <ace/Thread_Mutex.h>
    #include <ace/OS_NS_stdio.h>
    #include <ace/OS_NS_unistd.h>

    #include <atomic>
    typedef std::atomic<int> AtomicInt;
#else
    #define ACE_VERSION_5

    #include <ace/OS.h>

    // If compiling on legacy ACE 5.2a where ACE_TMAIN doesn't exist
    #ifndef ACE_TMAIN
        #if defined(ACE_MAIN)
            #define ACE_TMAIN ACE_MAIN
        #else
            #define ACE_TMAIN main
        #endif
    #endif

    // ACE 5.2a also lacked the modern ACE_TCHAR definition on Linux
    #ifndef ACE_TCHAR
        #define ACE_TCHAR char
    #endif

    // Use ACE_Atomic_Op instead of std::atomic
    // ACE 5.2a_p12 has very different headers than all newer versions of ACE
    // The include below are for Barrier.h, Condition_T.h, Thread_Mutex.h,
    // OS_NS_stdio.h, and Atomic_Op.h, all found in newer releases.
    #include <ace/Synch_T.h>
    #include <ace/Synch.h>
    #include <ace/OS.h>
    #include <sstream>
    #include <stdexcept>

    typedef ACE_Atomic_Op<ACE_Thread_Mutex, int> AtomicInt;

    // Disable some C++11 features that ACE may still reference
    #ifndef ACE_LACKS_CXX11
        #define ACE_LACKS_CXX11
    #endif
#endif

#if (CPP_STD_VERSION < 201103L)
    #define nullptr 0
    #define override
    #define final

    // For missing functions in C++98
    namespace std
    {
        template <typename T>
        inline std::string to_string (T value)
        {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }

        inline int stoi (const std::string& str)
        {
            std::stringstream ss (str);
            int result = 0;
            ss >> result;

            if (ss.fail() || !ss.eof())
            {
                throw std::invalid_argument ("stoi: invalid argumment '" + str + "'");
            }

            return result;
        }
    }

    // For missing enum class scoping
    struct SchedulingTier
    {
        enum Type
        {
            RealTimeAndAffinity, // SCHED_FIFO + Physical P-Core / Low-Latency CCX Pinning
            AffinityOnly,        // Standard Timesharing + Physical Core Pinning
            StandardFallback     // Default OS management (Single-core fallback or blocked API)
        };
    };

    struct WorkloadType
    {
         enum Type
        {
            PRIMARY_WORKER,      // Dense, uniform FPU arithmetic
            SIBLING_WORKER,      // Intercept path calculations (Takes mathematical precedence)
            MAIN_TASK            // Low-math, high-string background processing
        };
    };

    namespace Manager
    {
        struct ProducerState
        { 
            enum Type
            {
                STOPPED,
                STARTED,
                RUNNING,
                FINISHED
            };
        };

	struct TaskRole
        {
            enum Type
            {
                ROLE_PRODUCER,
                ROLE_CONSUMER
            };
        };
    }
#else
    // For modern enum class scoping
    enum class SchedulingTier
    {
        RealTimeAndAffinity, // SCHED_FIFO + Physical P-Core / Low-Latency CCX Pinning
        AffinityOnly,        // Standard Timesharing + Physical Core Pinning
        StandardFallback     // Default OS management (Single-core fallback or blocked API)
    };

    enum class WorkloadType
    {
        PRIMARY_WORKER,      // Dense, uniform FPU arithmetic
        SIBLING_WORKER,      // Intercept path calculations (Takes mathematical precedence)
        MAIN_TASK            // Low-math, high-string background processing
    };

    namespace Manager
    {
        enum ProducerState { STOPPED, STARTED, RUNNING, FINISHED };
	    enum TaskRole { ROLE_PRODUCER, ROLE_CONSUMER };
    }
#endif

// Manual Singleton for ACE compatibility across versions
#define DECLARE_SINGLETON(ClassName) \
public: \
    static ClassName* instance() { \
        static ClassName* theInstance = 0; \
        if (theInstance == 0) { \
            theInstance = new ClassName(); \
        } \
        return theInstance; \
    } \
private: \
    ClassName(); \
    ~ClassName() \
     { \
          shutdown(); \
     }

#endif // COMPATIBILITY_HXX
