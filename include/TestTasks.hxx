#pragma once
#ifndef TESTTASKS_HXX
#define TESTTASKS_HXX

#include "ConfigurableTask.hxx"
#include <ace/Task.h>
#include <ace/Thread.h>

#include "ObjectData.hxx"

namespace Manager
{
    std::vector<ObjectData> createFakeObjectData (size_t size);

    std::vector<TestData> createFakeTestData (size_t size, float min, float max);

    struct TaskConfig;

    template<typename T>
    class ObjectPool;

    template<typename DataT, typename InputT>
    class ConfigurableTask;

    template<typename DT, typename IT>
    class TaskOrchestrator;

    template<typename DataT, typename InputT>
    class TestProducerTask : public ConfigurableTask<DataT, InputT>
    {
        public:
            TestProducerTask (const TaskConfig& cfg, TaskOrchestrator<DataT, InputT>* processor)
                : ConfigurableTask<DataT, InputT> (cfg, processor)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] TestProducerTask initialized\n")));
            }

        protected:
            virtual void processWorkload (int threadId, void* arg) override
            {
                std::vector<InputT>* input; // = static_cast<std::vector<InputT>*>(arg);

                TaskOrchestrator<DataT, InputT>* manager = this->m_orchestrator;

                input = &manager->m_currentInputData;

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("\n\n\nProducer %d processing %zu items\n"), threadId, input ? input->size() : 0));

                if (!input) return;

                std::vector<DataT>* writeBuffer = manager->getActiveWriteBuffer();

                if (!writeBuffer) return;

                // Simple copy with level
                for (size_t i = threadId; i < input->size(); i += this->m_config.numThreads)
                {
                    if (i < writeBuffer->size())
                    {
                        const InputT& td = (*input)[i];
                        DataT& od = (*writeBuffer)[i];
                        od.level = td.level;   // Copy level
                        od.isValid = true;
                    }
                }

                if (--manager->m_activeProducerThreads == 0)
                {
                    manager->markAndSwap (true);
                    manager->m_nextRun = ACE_OS::gettimeofday() + ACE_Time_Value (0, this->m_config.producerIntervalMs * 1000);
                    manager->setProducerFinished (ProducerState::STOPPED);
                }

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("%s %i leaving processWorkload()\n\n"), this->m_config.name.c_str(), threadId));

                
            }
    };


    template<typename DataT, typename InputT>
    class TestConsumerTask : public ConfigurableTask<DataT, InputT>
    {
        public:
            TestConsumerTask (const TaskConfig& cfg, TaskOrchestrator<DataT, InputT>* processor)
                : ConfigurableTask<DataT, InputT> (cfg, processor)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] TestConsumerTask initialized\n")));
            }

        protected:
            virtual void processWorkload (int threadId, void* arg) override
            {
                std::vector<DataT>* readBuffer; // = static_cast<std::vector<DataT>*> (arg);
                std::vector<InputT>* currentPtr = this->m_orchestrator->getSharedDataPtr().value();

                TaskOrchestrator<DataT, InputT>* manager = this->m_orchestrator;
                readBuffer = manager->m_activeReadBuffer;

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Consumer %d processing %zu objects\n"), threadId, readBuffer ? readBuffer->size() : 0));

                if (!readBuffer) return;

                // Stride division
                size_t numThreads = this->m_config.numThreads;

                for (size_t i = threadId; i < readBuffer->size(); i += numThreads)
                {
                    const DataT& obj = (*readBuffer)[i];
                    // Find corresponding TestData and update flagged

                    if (i < currentPtr->size())
                    {
                        InputT& td = (*currentPtr)[i];

                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("input  %i: level: %f, flagged %i\n"), td.objectId, td.level, td.flagged));
                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("object %i: level: %f, isValid %i\n"), obj.objectId, obj.level, obj.isValid));

                        td.flagged = (td.level >= obj.level);


                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("input updated %i: level: %f, flagged %i\n\n"), td.objectId, td.level, td.flagged));
                    }
                }

                if (--manager->m_activeConsumerThreads == 0)
                {
                    this->notifyConsumerDone();
                }

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("%s %i leaving processWorkload()\n\n"), this->m_config.name.c_str(), threadId));
            }
        };
} // namespace Manager

#endif
