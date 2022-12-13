#ifndef __UNLOCK_THREAD_H_
#define __UNLOCK_THREAD_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace icsneo
{

class UnlockThread {
public:
	UnlockThread();
	~UnlockThread();
	void lock(const std::chrono::milliseconds& timeout);
	void unlock();
private:
	void thread();
	std::chrono::milliseconds mTimeout;
	bool mLockable = false;
	bool mLocked = false;
	bool mTimeoutSet = false;
	std::condition_variable mCV;
	std::mutex mMutex;
	std::thread mThread;
	bool mStop = false;
};

} // namespace icsneo

#endif
