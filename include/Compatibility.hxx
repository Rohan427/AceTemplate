#pragma once

#ifndef COMPATIBILITY_HXX
#define COMPATIBILITY_HXX

// Compatibility layer
#if defined(__clang__) && defined(__HIP__)
#pragma clang diagnostic ignored "-Wc++11-extensions"
#pragma clang diagnostic ignored "-Wc++17-extensions"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#if defined(LEGACY_GCC_44) || (__cplusplus < 201103L)
    #define nullptr 0
    #define override
    #define final

    // Use ACE_Atomic_Op instead of std::atomic
    #include <ace/Atomic_Op.h>
    typedef ACE_Atomic_Op<ACE_Thread_Mutex, int> AtomicInt;

    // Disable some C++11 features that ACE may still reference
    #define ACE_LACKS_CXX11
#else
    #include <atomic>
    typedef std::atomic<int> AtomicInt;
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
    ~ClassName();

#endif // COMPATIBILITY_HXX
