#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

// The new threadpool.

// To help smart scheduling.
enum class TaskType {
	CPU_COMPUTE,
	IO_BLOCKING,
	DEDICATED_THREAD,  // These can never get stuck in queue behind others, but are more expensive to launch. Cannot use I/O.
};

enum class TaskPriority {
	HIGH = 0,
	NORMAL = 1,
	LOW = 2,

	COUNT,
};

// Implement this to make something that you can run on the thread manager.
class Task {
public:
	virtual ~Task() {}
	virtual TaskType Type() const = 0;
	virtual TaskPriority Priority() const = 0;
	virtual void Run() = 0;
	virtual bool Cancellable() { return false; }
	virtual void Cancel() {}
	virtual uint64_t id() { return 0; }
	virtual void Release() { delete this; }
	virtual const char *Kind() const { return nullptr; }  // Useful for selecting task by some qualifier, like, waiting for them all.
};

class Waitable {
public:
	virtual ~Waitable() {}

	virtual void Wait() = 0;

	void WaitAndRelease() {
		Wait();
		delete this;
	}
};

struct TaskThreadContext;
static constexpr size_t TASK_PRIORITY_COUNT = (size_t)TaskPriority::COUNT;

struct GlobalThreadContext {
	std::mutex mutex;
	std::deque<Task *> compute_queue[TASK_PRIORITY_COUNT];
	std::atomic<int> compute_queue_size;
	std::deque<Task *> io_queue[TASK_PRIORITY_COUNT];
	std::atomic<int> io_queue_size;
	std::vector<TaskThreadContext *> threads_;

	std::atomic<int> roundRobin;
};

class ThreadManager {
public:
	ThreadManager();
	~ThreadManager() = default;

	// The distinction here is to be able to take hyper-threading into account.
	// It gets even trickier when you think about mobile chips with BIG/LITTLE, but we'll
	// just ignore it and let the OS handle it.
	void Init(int numCores, int numLogicalCoresPerCpu);
	void EnqueueTask(Task *task);
	// Use enforceSequence if this must run after all previously queued tasks.
	void EnqueueTaskOnThread(int threadNum, Task *task);
	void Teardown();

	bool IsInitialized() const;

	// Currently does nothing. It will always be best-effort - maybe it cancels,
	// maybe it doesn't. Note that the id is the id() returned by the task. You need to make that
	// something meaningful yourself.
	void TryCancelTask(uint64_t id);

	// Parallel loops (assumed compute-limited) get one thread per logical core. We have a few extra threads too
	// for I/O bounds tasks, that can be run concurrently with those.
	int GetNumLooperThreads() const;

private:
	bool TeardownTask(Task *task, bool enqueue);

	// This is always pointing to a context, initialized in the constructor.
	std::unique_ptr<GlobalThreadContext> global_;

	int numThreads_ = 0;
	int numComputeThreads_ = 0;

	friend struct TaskThreadContext;
};

extern ThreadManager g_threadManager;
