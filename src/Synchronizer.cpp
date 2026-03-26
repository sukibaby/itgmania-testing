#include "Synchronizer.h"

#include <algorithm>
#include <queue>
#include <utility>

#include "RageLog.h"
#include "RageTimer.h"
#include "global.h"

Synchronizer* SYNCHRONIZER =
    nullptr;  // global and accessible from anywhere in our program

namespace {
int GetDefaultWorkerCount() {
  const unsigned int hw = std::thread::hardware_concurrency();
  if (hw <= 1U) {
    return 1;
  }

  // Keep one core available for the main/game thread.
  return static_cast<int>(hw - 1U);
}
}  // namespace

Synchronizer::Synchronizer() : m_started(false), m_shutdown(false) {
  Start(GetDefaultWorkerCount());
}

Synchronizer::~Synchronizer() {
  Stop();
}

void Synchronizer::Start(int workerCount) {
  std::lock_guard<std::mutex> lock(m_workerMutex);
  if (m_started) {
    return;
  }

  if (workerCount <= 0) {
    workerCount = 1;
  }

  m_shutdown = false;
  m_workers.reserve(workerCount);
  for (int i = 0; i < workerCount; ++i) {
    m_workers.emplace_back([this] { this->WorkerMain(); });
  }

  m_started = true;
  LOG->Trace("Synchronizer started with %d worker threads", workerCount);
}

void Synchronizer::Stop() {
  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    if (!m_started) {
      return;
    }
    m_shutdown = true;
  }

  m_workerCv.notify_all();

  for (std::thread& worker : m_workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  m_workers.clear();

  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    std::queue<std::function<void()>> empty;
    std::swap(m_workerQueue, empty);
  }

  {
    std::lock_guard<std::mutex> lock(m_mainThreadMutex);
    std::queue<std::function<void()>> empty;
    std::swap(m_mainThreadQueue, empty);
  }

  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    m_started = false;
  }

  LOG->Trace("Synchronizer stopped");
}

void Synchronizer::EnqueueWork(std::function<void()> job) {
  if (!job) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    if (!m_started || m_shutdown) {
      return;
    }
    m_workerQueue.push(std::move(job));
  }

  m_workerCv.notify_one();
}

void Synchronizer::EnqueueMainThreadTask(std::function<void()> task) {
  if (!task) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_mainThreadMutex);
  m_mainThreadQueue.push(std::move(task));
}

int Synchronizer::RunMainThreadTasks(int maxTasks, float maxSeconds) {
  if (maxTasks <= 0) {
    return 0;
  }

  const bool useTimeLimit = maxSeconds > 0.0f;
  RageTimer timer;
  int tasksProcessed = 0;

  while (tasksProcessed < maxTasks) {
    std::function<void()> task;
    {
      std::lock_guard<std::mutex> lock(m_mainThreadMutex);
      if (m_mainThreadQueue.empty()) {
        break;
      }

      task = std::move(m_mainThreadQueue.front());
      m_mainThreadQueue.pop();
    }

    ASSERT(task != nullptr);
    task();

    ++tasksProcessed;

    if (useTimeLimit && timer.Ago() >= maxSeconds) {
      break;
    }
  }

  return tasksProcessed;
}

int Synchronizer::GetWorkerCount() const {
  std::lock_guard<std::mutex> lock(m_workerMutex);
  return static_cast<int>(m_workers.size());
}

std::size_t Synchronizer::GetPendingWorkCount() const {
  std::lock_guard<std::mutex> lock(m_workerMutex);
  return m_workerQueue.size();
}

std::size_t Synchronizer::GetPendingMainThreadTaskCount() const {
  std::lock_guard<std::mutex> lock(m_mainThreadMutex);
  return m_mainThreadQueue.size();
}

void Synchronizer::WorkerMain() {
  for (;;) {
    std::function<void()> job;

    {
      std::unique_lock<std::mutex> lock(m_workerMutex);
      m_workerCv.wait(lock, [this] {
        return m_shutdown || !m_workerQueue.empty();
      });

      if (m_shutdown && m_workerQueue.empty()) {
        return;
      }

      job = std::move(m_workerQueue.front());
      m_workerQueue.pop();
    }

    ASSERT(job != nullptr);
    job();
  }
}

/*
 * (c) 2026 ITGmania Team
 * All rights reserved.
 */