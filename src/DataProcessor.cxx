#include "DataProcessor.hxx"
#include "TaskOrchestrator.hxx"
#include "Config.hxx"
#include <ace/Log_Msg.h>
#include "TestTasks.hxx"

namespace Manager
{
    DataProcessor::DataProcessor() : m_sharedDataPtr (nullptr)
    {
        m_orchestrator = &TaskOrchestrator<ObjectData, TestData>::instance();
    }

    void DataProcessor::initialize (size_t poolCapacity)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::initialize\n")));

        // Producer (long-running)
        TaskConfig prodCfg;
        prodCfg.numThreads = ::Config::getInstance().MAX_PROD_THREADS; // ::Config::getInstance().getMaxThreads() / 2;
        prodCfg.basePriority = 30;
        prodCfg.workloadType = WorkloadType::PRIMARY_WORKER;
        prodCfg.useRealTime = true;
        prodCfg.intervalMs = 0;
        prodCfg.name = "DataProducer";
        prodCfg.role = TaskRole::ROLE_PRODUCER;
        prodCfg.producerIntervalMs = ::Config::getInstance().UPDATE_INTERVAL;  // 120000 is 2 minutes
        prodCfg.tier = SchedulingTier::RealTimeAndAffinity;

        // Consumer (fast)
        TaskConfig consCfg;
        consCfg.numThreads = ::Config::getInstance().MAX_CON_THREADS;
        consCfg.basePriority = 50;
        consCfg.workloadType = WorkloadType::SIBLING_WORKER;
        consCfg.useRealTime = true;
        consCfg.intervalMs = 0;
        consCfg.name = "DataConsumer";
        consCfg.role = TaskRole::ROLE_CONSUMER;
        consCfg.producerIntervalMs = 0;
        consCfg.tier = SchedulingTier::RealTimeAndAffinity;

        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::initialize tasks with %i objects in buffers\n"), poolCapacity));

        m_orchestrator->initialize (::Config::getInstance().getMaxThreads());

        m_orchestrator->createProducer (prodCfg);
        m_orchestrator->createConsumer (consCfg);
    }

    void DataProcessor::acceptData (std::vector<TestData>* dataPtr)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::acceptData, with data size of %i\n"), dataPtr->size()));

        m_sharedDataPtr = dataPtr;

        m_orchestrator->runProducer ("DataProducer", dataPtr, 0);

        if (m_orchestrator->isDataValid())
        {
            m_orchestrator->updateSharedDataPtr (dataPtr);
            m_orchestrator->runConsumer ("DataConsumer", dataPtr, 0);
            m_orchestrator->waitForConsumerCompletion ("DataConsumer", TaskRole::ROLE_CONSUMER);
        }
    }

    void DataProcessor::shutdown()
    {
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::shutdown\n")));

        if (m_orchestrator)
        {
            m_orchestrator->stopAll();
        }
        else
        {
            ACE_DEBUG ((LM_WARNING, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator already closed\n")));
            TaskOrchestrator<ObjectData, TestData>::instance().stopAll();
        }
        // Any other cleanup
    }

} // namespace Manager

template class Manager::TaskOrchestrator<Manager::ObjectData, Manager::TestData>;
