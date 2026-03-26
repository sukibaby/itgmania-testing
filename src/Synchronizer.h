#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H

#include <cstddef>
#include <condition_variable>
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

  int RunMainThreadTasks(int maxTasks, float maxSeconds);

  int GetWorkerCount() const;
  std::size_t GetPendingWorkCount() const;
  std::size_t GetPendingMainThreadTaskCount() const;

 private:
  void WorkerMain();

  mutable std::mutex m_workerMutex;
  std::condition_variable m_workerCv;
  std::queue<std::function<void()>> m_workerQueue;
  std::vector<std::thread> m_workers;

  mutable std::mutex m_mainThreadMutex;
  std::queue<std::function<void()>> m_mainThreadQueue;

  bool m_started;
  bool m_shutdown;
};

extern Synchronizer* SYNCHRONIZER;

#endif

/*
 * (c) 2026 ITGmania Team
 * All rights reserved.
 */