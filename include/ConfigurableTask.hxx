#pragma once
#ifndef CONFIGURABLETASK_HXX
#define CONFIGURABLETASK_HXX

#include "ObjectPool.hxx"
#include "ObjectData.hxx"
#include <ace/Task.h>
#include <ace/Barrier.h>
#include "ace/Thread.h"
#include "SystemCapabilities.hxx"
#include <vector>
#include <string>

namespace Manager
{
    struct TaskConfig
    {
        std::string name;
        int numChildThreads;      // Number of worker threads to spawn
        int basePriority;         // 1-99 (SCHED_FIFO) or niceness delta
        WorkloadType workloadType;
        bool useRealTime;         // Attempt SCHED_FIFO if permitted
        unsigned long intervalMs; // 0 = on-demand, else periodic (e.g. 120000)
        bool isProducer;          // true = writes + swaps, false = read-only
    };

    template<typename DataT>
    class ConfigurableTask : public ACE_Task<ACE_MT_SYNCH>
    {
        public:
            ConfigurableTask (const TaskConfig& cfg, ObjectPool<std::vector<DataT> >& pool);
            virtual ~ConfigurableTask();

            virtual int open (void* args = 0);
            virtual int svc();
            virtual int close (u_long flags = 0);

            // Consumer API (high-priority, blocking)
            std::vector<DataT>* getReadBuffer();

            TaskConfig getConfig()
            {
                return m_config;
            }

            // Producer API
            void triggerUpdate();     // For manual trigger if needed
            void swapBuffers();       // Called after producer work

            virtual void stop();

        protected:
            virtual void processWorkload (int threadId, std::vector<DataT>& buffer) = 0;

        private:
            TaskConfig m_config;
            ObjectPool<std::vector<DataT>> &m_pool;
            bool m_done;
            AtomicInt m_threadIndexer;
            const std::vector<HardwareCore> &m_hardwareCorePool;
            ::SchedulingTier m_selectedTier;

            // Double buffer
            std::vector<DataT>* m_bufferA;
            std::vector<DataT>* m_bufferB;
            std::vector<DataT>* m_activeWriteBuffer;
            ACE_Thread_Mutex m_bufferLock;

            void resetCoreAffinityOnShutdown();   // Called 

        public:
            static ACE_Thread_Mutex m_sharedVectorLock;
            ACE_Barrier* m_barrier;
    };
} // namespace Manager

#endif // CONFIGURABLETASK_HXX
