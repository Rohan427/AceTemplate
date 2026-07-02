#include "Config.hxx"
#include "TaskOrchestrator.hxx"
#include "DataProcessor.hxx"
#include "ObjectPool.hxx"
#include "ObjectData.hxx"
#include <ace/Log_Msg.h>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ace/Signal.h>

#define DEBUG true

static void signalHandler (int sig)
{
    const char* sigName = "unknown";
    switch (sig)
    {
        case SIGINT:  sigName = "SIGINT";  break;
        case SIGTERM: sigName = "SIGTERM"; break;
        case SIGHUP:  sigName = "SIGHUP";  break;
        case SIGSEGV: sigName = "SIGSEGV"; break;
            // Add more as needed
    }

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("Received signal %s (%d) - shutting down...\n"), sigName, sig));

    if (sig == SIGSEGV)
    {
        ACE_DEBUG ((LM_CRITICAL, ACE_TEXT ("Segmentation fault - dumping core if enabled\n")));
    }

    Manager::DataProcessor::instance()->shutdown();
    ACE_OS::exit (0);   // Or return from main if possible
}

using namespace Manager;

float generateFloat (float min, float max)
{
    double scaled = static_cast<double> (std::rand()) / RAND_MAX;
    return min + static_cast<float> (scaled * (max - min + 1));
}

float generateInt (int min, int max)
{
    double scaled = static_cast<double> (std::rand()) / RAND_MAX;
    return min + static_cast<int> (scaled * (max - min + 1));
}

static std::vector<ObjectData> createFakeObjectData (size_t size)
{
    std::vector<ObjectData> data;
    data.reserve (size);

    for (size_t i = 0; i < size; ++i)
    {
        data.push_back (ObjectData (i, time (0), true, ::Config::getInstance().VALID_LEVEL));
    }

    return data;
}

static std::vector<TestData> createFakeTestData (size_t size, float min, float max)
{
    std::vector<TestData> data;
    data.reserve (size);

    for (size_t i = 0; i < size; ++i)
    {
        float level = generateFloat (min, max);
        
        if (i == 0 || i == (size - 1))
        {
            level = ::Config::getInstance().VALID_LEVEL + 1.0f;
        }

        data.push_back (TestData (i, level, false, generateFloat (0, 1000), generateFloat (0, 1000), generateFloat (0, 1000)));
    }

    return data;
}

//int main (int argc, char *argv[])
int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
    // Register signal handler
    ACE_Sig_Action sa (signalHandler);
    sa.register_action (SIGINT);
    sa.register_action (SIGTERM);
    sa.register_action (SIGHUP);
    sa.register_action (SIGTSTP);
    sa.register_action (SIGBUS);
    sa.register_action (SIGILL);
    sa.register_action (SIGFPE);
    sa.register_action (SIGABRT);
    sa.register_action (SIGSEGV);

    // Set ACE to show: Time | Severity | Thread ID | Message
    ACE_Log_Msg::instance()->open (argv[0], ACE_Log_Msg::STDERR); // | ACE_Log_Msg::LOGGER);
    ACE_Log_Msg::instance()->priority_mask (LM_DEBUG |  LM_INFO | LM_ERROR | LM_CRITICAL | LM_WARNING, ACE_Log_Msg::PROCESS);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n\n"), "Starting up. Press Ctrl+C to interrupt."));

    // 1. Config + Pool
    ::Config::getInstance().init ("configuration.json");

#if DEBUG
    std::cout << "THREAD_SLEEP_TIME " << std::fixed << std::setprecision (5) << ::Config::getInstance().THREAD_SLEEP_TIME << std::endl;
    std::cout << "MAX_THREADS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_THREADS << std::endl;
    std::cout << "MAX_FPU_THREADS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_FPU_THREADS << std::endl;
    std::cout << "MAX_OBJECTS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_OBJECTS << std::endl;
#endif

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Initializing DataProcessor"));
    DataProcessor* processor = DataProcessor::instance();
    TaskOrchestrator<ObjectData, TestData>* manager = processor->getOrchestrator();

    processor->initialize (::Config::getInstance().getMaxObjects());

    size_t dataSize = 5;

    // Test cycle
    
//    int numObjects =  generateInt (100, 1000);

//    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: Creating TestData and ObjectData with %i objects.\n"), dataSize));

//    std::vector<TestData> testData = ::createFakeTestData (dataSize, 1, 10);
//    std::vector<ObjectData> objectData = ::createFakeObjectData (dataSize);

    manager->m_bufferA->resize (dataSize);
    manager->m_bufferB->resize (dataSize);

    //manager->m_bufferA = &objectData;

    //manager->m_activeReadBuffer = manager->m_bufferA;
    //manager->m_activeReadState->valid = true;

    for (int cycle = 0; cycle < 20; ++cycle)
    {  // Run several cycles
        std::vector<TestData> testData = ::createFakeTestData (dataSize, 1, 10);

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: Testing TestData and ObjectData sizes %i, %i objects.\n"), testData.size(), manager->m_bufferA->size()));

        DataProcessor::instance()->acceptData (&testData);

        ACE_OS::sleep (ACE_Time_Value (0, 500000));  // 500ms delay
    }
    

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: %s\n"), "Test cycle complete"));

    // Later: shutdown on exit
    processor->shutdown();

    return 0;


    //for (int i = 0; i< numObjects; i++)
    //{
    //    testData.push_back (TestData (i,
    //                                  generateFloat (0, 100),
    //                                  false,
    //                                  generateFloat (0, 1000),
    //                                  generateFloat (0, 1000),
    //                                  generateFloat (0, 1000)
    //                                 )
    //                       );
    //}
}
