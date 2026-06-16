#include "Config.hxx"
#include "TaskOrchestrator.hxx"
#include "ObjectPool.hxx"
#include "ObjectData.hxx"
#include <ace/Log_Msg.h>
#include <iostream>
#include <iomanip>

#define DEBUG false

using namespace Manager;

class TestProducerTask : public ConfigurableTask<ObjectData>
{
    public:
        TestProducerTask (const TaskConfig& cfg, ObjectPool<std::vector<ObjectData> >& pool)
            : ConfigurableTask<ObjectData>(cfg, pool)
        { 
//            ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Producer"));
        }

    protected:
        virtual void processWorkload (int threadId, std::vector<ObjectData>& buffer)
        {
//            ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] Hello from Producer Worker %d - Updating buffer (size=%d)\n"),
//                       threadId, (int)buffer.size()));

            // Simulate work
            buffer.resize (10);  // Minimal test data

            for (size_t i = 0; i < buffer.size(); ++i)
            {
                buffer[i].x = (float)threadId;
                buffer[i].y = (float)i;
                buffer[i].z = 42.0f;
            }

            swapBuffers();   // Signal swap
        }
};

class TestConsumerTask : public ConfigurableTask<ObjectData>
{
    public:
        TestConsumerTask (const TaskConfig& cfg, ObjectPool<std::vector<ObjectData> >& pool)
            : ConfigurableTask<ObjectData>(cfg, pool)
        {
            //ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Consumer"));
        }

    protected:
        virtual void processWorkload (int threadId, std::vector<ObjectData>& buffer)
        {
//            ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Hello from Consumer Worker %d - Reading buffer (size=%d)\n"),
//                       threadId, (int)buffer.size()));
        }
};

//int main (int argc, char *argv[])
int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
    // Set ACE to show: Time | Severity | Thread ID | Message
    ACE_Log_Msg::instance()->open (argv[0], ACE_Log_Msg::STDERR); // | ACE_Log_Msg::LOGGER);
    ACE_Log_Msg::instance()->priority_mask (LM_DEBUG |  LM_INFO | LM_ERROR | LM_CRITICAL, ACE_Log_Msg::PROCESS);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Starting up"));

    // 1. Config + Pool
    ::Config::getInstance().init("configuration.json");
    ObjectPool<std::vector<ObjectData> > objectPool(1000);   // Small test size

#if DEBUG
    std::cout << "THREAD_SLEEP_TIME " << std::fixed << std::setprecision (5) << ::Config::getInstance().THREAD_SLEEP_TIME << std::endl;
    std::cout << "MAX_THREADS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_THREADS << std::endl;
    std::cout << "MAX_FPU_THREADS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_FPU_THREADS << std::endl;
    std::cout << "MAX_OBJECTS " << std::fixed << std::setprecision (5) << ::Config::getInstance().MAX_OBJECTS << std::endl;
#endif

    // 2. Orchestrator
    TaskOrchestrator::instance().initialize (8, objectPool);

    // 3. Create Producer (low priority, periodic ~5s for testing)
    // Producer: Low priority, runs every 5 seconds for testing (change to 120000 later)
    TaskConfig prodCfg = {"DataUpdater", 16, 10 /*low pri*/, WorkloadType::PRIMARY_WORKER, true, 5000, true};
    TestProducerTask* producer = TaskOrchestrator::instance().createAndRegisterTask<TestProducerTask> (prodCfg);

    // 4. Create Consumer (high priority, on-demand)
    // Consumer: High priority, on-demand style with light sleep
    TaskConfig consCfg = {"TestConsumer", 4, 60, WorkloadType::SIBLING_WORKER, true, 0, false};
    TestConsumerTask* consumer = TaskOrchestrator::instance().createAndRegisterTask<TestConsumerTask> (consCfg);

    // 5. Start everything
    TaskOrchestrator::instance().startAll();

    // 6. Simulate high-frequency consumer calls from main thread
    for (int i = 0; i < 5; ++i)
    {
        ACE_OS::sleep (ACE_Time_Value (2));  // 2 second intervals

        std::vector<ObjectData>* stable = consumer->getReadBuffer();
        ACE_DEBUG((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] Main thread: Consumer read buffer OK - size=%d\n"),
                   stable ? (int)stable->size() : 0));
    }

    ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Test complete. Shutting down...\n")));

    TaskOrchestrator::instance().stopAll();

    ACE_DEBUG((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Shutdown successful. Buffers safely managed.\n")));
    return 0;
}
