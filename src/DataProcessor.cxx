#include "DataProcessor.hxx"
#include "TaskOrchestrator.hxx"
#include "Config.hxx"
#include <ace/Log_Msg.h>
#include "TestTasks.hxx"

namespace Manager
{
    DataProcessor::DataProcessor() : m_sharedDataPtr (nullptr)
    {
    }

    void DataProcessor::initialize (size_t poolCapacity)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::initialize\n")));

        // Producer (long-running)
        TaskConfig prodCfg;
        prodCfg.numThreads = 1; // ::Config::getInstance().getMaxThreads() / 2;
        prodCfg.basePriority = 30;
        prodCfg.workloadType = WorkloadType::PRIMARY_WORKER;
        prodCfg.useRealTime = true;
        prodCfg.intervalMs = 0;
        prodCfg.name = "DataProducer";
        prodCfg.role = ROLE_PRODUCER;
        prodCfg.producerIntervalMs = 500;  // 120000 is 2 minutes
        prodCfg.tier = SchedulingTier::RealTimeAndAffinity;

        // Consumer (fast)
        TaskConfig consCfg;
        consCfg.numThreads = 1;
        consCfg.basePriority = 50;
        consCfg.workloadType = WorkloadType::SIBLING_WORKER;
        consCfg.useRealTime = true;
        consCfg.intervalMs = 0;
        consCfg.name = "DataConsumer";
        consCfg.role = ROLE_CONSUMER;
        consCfg.producerIntervalMs = 0;
        consCfg.tier = SchedulingTier::RealTimeAndAffinity;

        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::initialize tasks with %i objects in buffers\n"), poolCapacity));

        TaskOrchestrator<ObjectData, TestData>::instance().initialize (::Config::getInstance().getMaxThreads());

        // 3. Create Producer (low priority, periodic ~5s for testing)
        // Producer: Low priority, runs every 5 seconds for testing (change to 120000 later)
        TaskOrchestrator<ObjectData, TestData>::instance().createProducer (prodCfg);

        // 4. Create Consumer (high priority, on-demand)
        // Consumer: High priority, on-demand style with light sleep
//        m_consumer = TaskOrchestrator::instance().createAndRegisterTask<TestConsumerTask> (consCfg, m_pool, m_sharedDataPtr, this);

//        m_producer = new TestProducerTask (prodCfg, m_pool, m_sharedDataPtr, this);
//        m_consumer = new TestConsumerTask (consCfg, m_pool, m_sharedDataPtr, this);

//        m_producer->open (0);
//        m_consumer->open (0);

//        if (!m_producer || !m_consumer)
        {
//            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::initialize failed to initialize tasks\n")));
        }
    }

    void DataProcessor::acceptData (std::vector<TestData>* dataPtr)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::acceptData\n")));

        m_sharedDataPtr = dataPtr;

        if (!TaskOrchestrator<ObjectData, TestData>::instance().isProducerStarted())
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::acceptData Starting producer task\n")));
            
            TaskOrchestrator<ObjectData, TestData>::instance().runProducer ("DataProducer", 0);
        }

        //ACE_Guard<ACE_Thread_Mutex> guard (m_acceptLock);
        //m_sharedDataPtr = dataPtr;

        if (TaskOrchestrator<ObjectData, TestData>::instance().isProducerStarted())
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Producer acceptData.\n")));
            
            TaskOrchestrator<ObjectData, TestData>::instance().updateSharedDataPtr (dataPtr);
        }

        //if (m_consumer)
        //{
        //    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Consumer acceptData.\n")));

        //    TaskOrchestrator<ObjectData, TestData>::instance().updateSharedDataPtr (dataPtr);

            //if (m_sharedDataPtr)
            //{
            //    m_consumerFinished = false;
            //    m_consumer->triggerConsumer();

            //    ACE_Time_Value timeout = ACE_OS::gettimeofday() + ACE_Time_Value (5, 0); // 60 second timeout

            //    while (!m_consumerFinished)
            //    {
            //        ACE_DEBUG ((LM_ERROR, ACE_TEXT ("Consumer acceptData() processing loop\n")));

            //        if (m_consumerDoneCond.wait (&timeout) == -1)
            //        {
            //            ACE_DEBUG ((LM_WARNING, ACE_TEXT ("---------------->Consumer wait timeout<---------------\n")));
            //            break;
            //        }
            //    }
            //}
            //else
            //{
            //    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::acceptData Consumer m_sharedDataPtr is NULL.\n")));
            //}
        //}
    }

} // namespace Manager

template class Manager::TaskOrchestrator<Manager::ObjectData, Manager::TestData>;
