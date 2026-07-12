#pragma once
#ifndef CONFIGURABLETASK_HXX
#define CONFIGURABLETASK_HXX

#include "TaskOrchestrator.hxx"
#include "SystemCapabilities.hxx"
#include "ObjectData.hxx"
#include "ObjectPool.hxx"
#include <ace/Task.h>
#include <vector>

namespace Manager
{
    template<typename DataT, typename InputT>
    class TaskOrchestrator;

    class DataProcessor;

    struct TaskConfig
    {
        int numThreads;
        int basePriority;
        int workloadType;
        bool useRealTime;
        unsigned long intervalMs;
        std::string name;
        int role;
        unsigned long producerIntervalMs;
        int tier;
    };

    template<typename DataT, typename InputT>
    class ConfigurableTask : public ACE_Task<ACE_MT_SYNCH>
    {
        public:
            virtual int open (void* args = 0);
            virtual int svc();
            virtual int close (u_long flags = 0);

            void stop();
            void triggerProducer();
            void triggerConsumer();
            void notifyConsumerDone();

            void markAndSwap (bool isUseful);
            std::vector<DataT>* getReadBuffer();
            BufferState getBufferState() const;

            TaskConfig getConfig()
            {
                return m_config;
            }

        protected:
            TaskConfig m_config;
            virtual void processWorkload (int threadId, void* arg) = 0;

            ACE_Barrier* getBarrier()
            {
                return m_barrier;
            }

            TaskOrchestrator<DataT, InputT>* m_orchestrator;

        private:
 //           ObjectPool<std::vector<DataT> >& m_pool;

            ACE_Barrier* m_barrier;
            bool m_done;
            AtomicInt m_threadIndexer;
            std::vector<HardwareCore> m_hardwareCorePool;
            int m_tier;

            // Necessary templte function definitions
        public:
            ConfigurableTask (const TaskConfig& cfg, TaskOrchestrator<DataT, InputT>* orchestrator)
                : m_config (cfg)
                , m_orchestrator (orchestrator)
                , m_barrier (0)
                , m_done (false)
                , m_threadIndexer (0)
            {
            }

            ~ConfigurableTask()
            {
                stop();

                if (m_barrier)
                {
                    delete m_barrier;
                    m_barrier = 0;
                }
            }
    };

} // namespace Manager

#endif // CONFIGURABLETASK_HXX
