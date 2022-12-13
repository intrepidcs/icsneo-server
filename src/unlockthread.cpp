#include "unlockthread.h"

namespace icsneo {

UnlockThread::UnlockThread() {
	mThread = std::thread(&UnlockThread::thread, this);
}

UnlockThread::~UnlockThread() {
	{
		std::unique_lock lk(mMutex);
		mStop = true;
	}
	mCV.notify_all();
	if(mThread.joinable())
		mThread.join();
}

void UnlockThread::lock(const std::chrono::milliseconds& timeout) {
	{
		std::unique_lock lk(mMutex);
		mCV.wait(lk, [&]{ return mLockable; });
		mTimeout = timeout;
		mTimeoutSet = true;
		mLockable = false;
	}
    mCV.notify_all();
}

void UnlockThread::unlock() {
	{
		std::unique_lock lk(mMutex);
		mLocked = false;
	}
	mCV.notify_all();
}

void UnlockThread::thread() {
	while(!mStop) {
		{
			std::lock_guard lk(mMutex);
			mLockable = true;
		}
		mCV.notify_one();
		std::unique_lock lk(mMutex);
		mCV.wait(lk, [&]{ return mTimeoutSet || mStop; });
		mTimeoutSet = false;
		mLocked = true;
		mCV.wait_for(lk, mTimeout, [&]{ return !mLocked || mStop; });
		mLocked = false;
	}
}

} // namespace icsneo
