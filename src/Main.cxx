#include "Config.hxx"
#include "TaskOrchestrator.hxx"
#include "DataProcessor.hxx"
#include "ObjectPool.hxx"
#include "ObjectData.hxx"
#include <ace/Log_Msg.h>
#include <iostream>
#include <iomanip>
#include <cstdlib>

#define DEBUG true

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

//int main (int argc, char *argv[])
int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
    // Set ACE to show: Time | Severity | Thread ID | Message
    ACE_Log_Msg::instance()->open (argv[0], ACE_Log_Msg::STDERR); // | ACE_Log_Msg::LOGGER);
    ACE_Log_Msg::instance()->priority_mask (LM_DEBUG |  LM_INFO | LM_ERROR | LM_CRITICAL | LM_WARNING, ACE_Log_Msg::PROCESS);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Starting up"));

    // 1. Config + Pool
    ::Config::getInstance().init ("configuration.json");

#if DEBUG
    std::cout << "THREAD_SLEEP_TIME " << std::fixed << std::setprecision (5) << ::Config::getInstance().THREAD_SLEEP_TIME << std::endl;
    std::cout << "MAX_THREADS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_THREADS << std::endl;
    std::cout << "MAX_FPU_THREADS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_FPU_THREADS << std::endl;
    std::cout << "MAX_OBJECTS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_OBJECTS << std::endl;
#endif

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Initializing DataProcessor"));

    DataProcessor::instance()->initialize (::Config::getInstance().getMaxObjects());

    // Test cycle
    std::vector<TestData> testData;
    int numObjects =  generateInt (100, 1000);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("Creating testData with %i objects.\n"), numObjects));

    for (int i = 0; i< numObjects; i++)
    {
        testData.push_back (TestData (i,
                                      generateFloat (0, 100),
                                      generateFloat (0, 1000),
                                      generateFloat (0, 1000),
                                      generateFloat (0, 1000)
                                     )
                           );
    }

    DataProcessor::instance()->acceptData (&testData);

    int total = 4;
    int count = 0;

    while (count < total)
    {
        // Sleep for 2 seconds and 500 milliseconds
        ACE_Time_Value sleep_duration (2, 500000); 

        // ACE_OS::sleep takes an ACE_Time_Value reference
        if (ACE_OS::sleep (sleep_duration) == -1)
        {
            // Handle interruption (returns -1 if interrupted by a signal)
            ACE_DEBUG ((LM_ERROR, ACE_TEXT ("Sleep was interrupted.\n")));
        }
        else
        {
            numObjects = generateInt (100, 1000);
            testData.clear();

            ACE_DEBUG ((LM_INFO, ACE_TEXT ("Creating testData with %i objects.\n"), numObjects));

            for (int i = 0; i< numObjects; i++)
            {
                testData.push_back (TestData (i,
                                              generateFloat (0, 100),
                                              generateFloat (0, 1000),
                                              generateFloat (0, 1000),
                                              generateFloat (0, 1000)
                                             )
                                   );
            }

            DataProcessor::instance()->acceptData (&testData);
        }

        count++;
    }

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Test cycle complete"));

    // Later: shutdown on exit
//    DataProcessor::instance()->shutdown();

    return 0;
}
