#pragma once

#ifndef TASKORCHESTRATOR_HXX
#define TASKORCHESTRATOR_HXX

#include "Compatibility.hxx"
#include "Config.hxx"
#include "SystemCapabilities.hxx"
#include "ConfigurableTask.hxx"
#include "TestTasks.hxx"
#include <ace/Task.h>
#include <ace/Barrier.h>
#include <ace/Thread.h>
#include <ace/Condition_T.h>
#include <ace/Thread_Mutex.h>
#include <ace/OS_NS_stdio.h>
#include <vector>
#include "ObjectData.hxx"
#include "ObjectPool.hxx"

namespace Manager
{
    template<typename DataT, typename InputT>
    class ConfigurableTask;

    struct TaskConfig;

    template<typename DT, typename IT>
    class TaskOrchestrator
    {
        public:
            size_t m_bufferCapacity;
            std::vector<DT>* m_bufferA;
            std::vector<DT>* m_bufferB;
            std::vector<DT>* m_activeWriteBuffer;
            std::vector<DT>* m_activeReadBuffer;
            BufferState m_bufferStateA;
            BufferState m_bufferStateB;
            BufferState* m_activeWriteState;
            BufferState* m_activeReadState;

            ACE_Thread_Mutex m_bufferLock;
            ACE_Thread_Mutex m_notifyLock;
            ACE_Condition<ACE_Thread_Mutex> m_notifyCond;

            unsigned long long m_versionCounter;
            ACE_Time_Value m_nextRun;


            static TaskOrchestrator& instance();

            void initialize (int maxThreads)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::initialize\n")));

                if (m_initialized)
                {
                    return;
                }

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::initialize buffers\n")));


                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::initialize m_bufferA\n")));



                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::initialize scan hardware\n")));

                // Scan the hardware and verify the core topology and system permissions
                m_numThreads = maxThreads;

                m_selectedTier = SystemCapabilities::AnalyzeTopologyAndPermissions (m_hardwarePool);

                if (m_hardwarePool.size() < maxThreads)
                {
                    ACE_DEBUG ((LM_INFO, ACE_TEXT ("Found less than %i cores available, setting thread count to %i\n"), 
                                maxThreads,
                                m_hardwarePool.size())
                              );

                    m_numThreads = m_hardwarePool.size();
                }

                // VERBOSE LOGGING FOR ENTERPRISE ENVIRONMENT MANAGEMENT
                std::string tierName;

                switch (m_selectedTier)
                {
                    case SchedulingTier::RealTimeAndAffinity:
                        tierName = "Tier 1: [REAL-TIME SCHED_FIFO + INTUITIVE HARDWARE CORE PINNING]";
                        break;

                    case SchedulingTier::AffinityOnly:
                        tierName = "Tier 2: [STANDARD SCHEDULER TIMESHARING + INTUITIVE HARDWARE CORE PINNING]";
                        break;

                    case SchedulingTier::StandardFallback:
                        tierName = "Tier 3: [STANDARD FALLBACK - SINGLE CORE OR OS CONTEXT ACCESS BLOCKED]";
                        break;
                }

                ACE_DEBUG ((LM_INFO, ACE_TEXT ("System Initialization: Selected Operating Framework: %s\n"), tierName.c_str()));
                ACE_DEBUG ((LM_INFO, ACE_TEXT ("Detected Total Hardware Units: %i available processing paths.\n"), m_numThreads));

                ACE_DEBUG ((LM_INFO, ACE_TEXT ("HETEROGENEOUS POOL MAPPING SUMMARY:\n")));
                ACE_DEBUG ((LM_INFO, ACE_TEXT ("  -> Main Thread: Reserved exclusively for Core 0\n")));

                size_t physicalCount = 0;
                size_t siblingCount  = 0;

                for (std::vector<HardwareCore>::const_iterator it = m_hardwarePool.begin(); it != m_hardwarePool.end(); ++it)
                {
                    const HardwareCore& core = *it;

                    if (core.isHTSibling)
                    {
                        siblingCount++;
                    }
                    else
                    {
                        physicalCount++;
                    }
                }

                ACE_DEBUG ((LM_INFO, ACE_TEXT ("  -> Pool 1: %i Physical Cores allocated (Cores 1-15)\n"), physicalCount - 1));
                ACE_DEBUG ((LM_INFO, ACE_TEXT ("  -> Pool 2: %i Sibling Cores allocated (Cores 17-31)\n"), siblingCount - 1));

                // SYNCHRONIZE BARRIER LIFECYCLE HANDSHAKE if needed
                //m_barrier = new ACE_Barrier (m_numThreads + 1);
                
                //// Launch threads as dedicated kernel-level entities (THR_BOUND)
                //this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_numThreads);
                
                //m_barrier->wait();
                //ACE_DEBUG ((LM_INFO, ACE_TEXT ("All %1 Primary threads pinned, scaled, and synchronized.").arg (m_numThreads));

                ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] TaskOrchestrator initialized with maxThreads=%d\n"), m_numThreads));

                m_initialized = true;
            }

            void registerTask (ConfigurableTask<DT, IT> *task);

            void createProducer (const TaskConfig &cfg)
            {
                if (m_producerStarted)
                {
                    ACE_DEBUG ((LM_WARNING, ACE_TEXT ("Producer already started\n")));
                    return;
                }

                TestProducerTask<DT, IT>* producer = new TestProducerTask<DT, IT> (cfg, this);
                registerTask (producer);
//                m_producer = producer;   // Optional internal pointer

                ACE_DEBUG ((LM_INFO, ACE_TEXT ("Created and registered Producer task\n")));
            }

            void createConsumer (const TaskConfig &cfg)
            {
                if (m_consumerStarted) return;

                TestConsumerTask<DT, IT>* consumer = new TestConsumerTask<DT, IT> (cfg, this);
                registerTask (consumer);
//                m_consumer = consumer; // Optional internal pointer

                ACE_DEBUG ((LM_INFO, ACE_TEXT("Created and registered Consumer task\n")));
            }

            void startAll();
            void stopAll();
            void runProducer (std::string name, void* args);
            void runConsumer (std::string name, void* args);

            // Access to shared pool
            ObjectPool<std::vector<DT>> *getObjectPool();

            SchedulingTier getSchedulingTier() const
            {
                return m_selectedTier;
            }

            const std::vector<HardwareCore>& getHardwarePool() const
            { 
                return m_hardwarePool;
            }

            bool isProducerStarted()
            {
                return m_producerStarted;
            }

            bool isValid()
            {
                return m_isValid;
            }

            bool isInitialized()
            {
                return m_initialized;
            }

            ACE_Thread_Mutex& getAcceptLock()
            {
                return m_acceptLock;
            }

            void setConsumerFinished (bool state)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: DataProcessor::setConsumerFinished\n")));
                m_consumerFinished = state;
            }

            ACE_Condition<ACE_Thread_Mutex>& getConsumerCond()
            {
                return m_consumerDoneCond;
            }

            bool isDataValid ()
            {
                return m_dataValid;
            }

            std::vector<DT>* getActiveWriteBuffer()
            {
                return m_activeWriteBuffer;
            }

            std::vector<DT>* getActiveReadBuffer()
            {
                return m_activeReadBuffer;
            }

            // After all worker threads finish heavy computation
            void markAndSwap (bool isUseful)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: ConfigurableTask<DataT>::markAndSwap\n")));

                ACE_Guard<ACE_Thread_Mutex> guard (m_bufferLock);

                m_activeWriteState->valid = isUseful;
                m_activeWriteState->version = ++m_versionCounter;
                std::time (&m_activeWriteState->timestamp);

                // Swap
                if (m_activeWriteBuffer == m_bufferA)
                {
                    m_activeWriteBuffer = m_bufferB;
                    m_activeWriteState = &m_bufferStateB;
                    m_activeReadBuffer = m_bufferA;
                    m_activeReadState = &m_bufferStateA;
                }
                else
                {
                    m_activeWriteBuffer = m_bufferA;
                    m_activeWriteState = &m_bufferStateA;
                    m_activeReadBuffer = m_bufferB;
                    m_activeReadState = &m_bufferStateB;
                }

                m_dataValid = isUseful;
            }

        private:

            static TaskOrchestrator *s_instance;
            static ACE_Thread_Mutex s_creationLock;

            std::vector<ConfigurableTask<DT, IT>*> m_tasks;
            ObjectPool<std::vector<DT>> m_objectPool;
            bool m_initialized;
            bool m_producerStarted;
            bool m_consumerStarted;
            bool m_isValid;

            SchedulingTier m_selectedTier;
            std::vector<HardwareCore> m_hardwarePool;
            size_t m_numThreads;

            ACE_Thread_Mutex m_acceptLock;
            ACE_Condition<ACE_Thread_Mutex> m_consumerDoneCond;
            bool m_consumerFinished;
            bool m_dataValid;

            TaskOrchestrator() : m_objectPool (::Config::getInstance().getMaxObjects() * 2),
                                 m_bufferCapacity(::Config::getInstance().getMaxObjects()),
                                 m_initialized (false),
                                 m_producerStarted (false),
                                 m_consumerStarted (false),
                                 m_isValid (false),
                                 m_activeWriteBuffer (0),
                                 m_activeReadBuffer (0),
                                 m_activeWriteState (0),
                                 m_activeReadState (0),
                                 m_versionCounter (0),
                                 m_notifyCond (m_notifyLock),
                                 m_nextRun (ACE_OS::gettimeofday()),
                                 m_consumerDoneCond (m_acceptLock)
            {
                // Acquire the two buffers from the pool
                m_bufferA = m_objectPool.acquire();
                m_bufferB = m_objectPool.acquire();

                if (m_bufferA)
                {
                    m_bufferA->resize (m_bufferCapacity);
                }

                if (m_bufferB)
                {
                    m_bufferB->resize (m_bufferCapacity);
                }

                m_activeWriteBuffer = m_bufferA;
                m_activeReadBuffer = m_bufferB;

                m_activeWriteState = &m_bufferStateA;
                m_activeReadBuffer = m_bufferB;
                m_activeReadState = &m_bufferStateB;
            }

            ~TaskOrchestrator()
            {
                stopAll();
                // Note: Do not delete tasks here - ownership is with caller or explicit cleanup
            }

        protected:
            ACE_Atomic_Op<ACE_Thread_Mutex, std::vector<IT>*> m_sharedDataPtr;

        public:
            void updateSharedDataPtr (std::vector<IT>* newPtr)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::updateSharedDataPtr()\n")));

                if (newPtr)
                {
                    m_sharedDataPtr = newPtr;
                }
                else
                {
                    ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::updateSharedDataPtr() newPtr is null\n")));
                }
            }

            ACE_Atomic_Op<ACE_Thread_Mutex, std::vector<IT>*> getSharedDataPtr()
            {
                return m_sharedDataPtr;
            }
    };
} // namespace Manager

#endif // TASKORCHESTRATOR_HXX
