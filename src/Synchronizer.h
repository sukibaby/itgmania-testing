#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H

#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class Synchronizer {
 public:
  Synchronizer();
  ~Synchronizer();

  Synchronizer(const Synchronizer&) = delete;
  Synchronizer& operator=(const Synchronizer&) = delete;

  void Start(int workerCount);
  void Stop();

  void EnqueueWork(std::function<void()> job);
  void EnqueueMainThreadTask(std::function<void()> task);
  void EnqueueWorkThenMainThreadTask(
      std::function<void()> work, std::function<void()> mainThreadTask);

  int RunMainThreadTasks(int maxTasks, float maxSeconds);

  int GetWorkerCount() const;
  std::size_t GetPendingWorkCount() const;
  std::size_t GetPendingMainThreadTaskCount() const;
  uint64_t GetSubmittedWorkCount() const;
  uint64_t GetCompletedWorkCount() const;
  uint64_t GetSubmittedMainThreadTaskCount() const;
  uint64_t GetCompletedMainThreadTaskCount() const;
  std::size_t GetPeakPendingWorkCount() const;
  std::size_t GetPeakPendingMainThreadTaskCount() const;

 private:
  void WorkerMain();

  mutable std::mutex m_workerMutex;
  std::condition_variable m_workerCv;
  std::queue<std::function<void()>> m_workerQueue;
  std::vector<std::thread> m_workers;

  mutable std::mutex m_mainThreadMutex;
  std::queue<std::function<void()>> m_mainThreadQueue;

  uint64_t m_submittedWorkCount;
  uint64_t m_completedWorkCount;
  uint64_t m_submittedMainThreadTaskCount;
  uint64_t m_completedMainThreadTaskCount;
  std::size_t m_peakPendingWorkCount;
  std::size_t m_peakPendingMainThreadTaskCount;

  bool m_started;
  bool m_shutdown;
};

extern Synchronizer* SYNCHRONIZER;

#endif

/*
 * (c) 2026 ITGmania Team
 * All rights reserved.
 */