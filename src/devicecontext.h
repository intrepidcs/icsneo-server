#ifndef __DEVICE_CONTEXT_H_
#define __DEVICE_CONTEXT_H_

#include <memory>
#include <unordered_map>
#include <set>
#include <mutex>
#include <chrono>
#include <iterator>
#include "icsneo/device/device.h"
#include "icsneo/communication/interprocessmailbox.h"
#include "clientthread.h"
#include "writerthread.h"
#include "unlockthread.h"

namespace icsneo
{

class DeviceContext {
public:
	DeviceContext(const std::shared_ptr<Device>& dev) : device(dev) {}
	std::shared_ptr<Device> getDevice() { return device; }
	bool addReader(const std::shared_ptr<InterprocessMailbox>& mb);
	bool addWriter(const std::shared_ptr<InterprocessMailbox>& mb);
	bool removeWriter(const std::shared_ptr<InterprocessMailbox>& mb);
	bool removeReader(const std::shared_ptr<InterprocessMailbox>& mb);
	bool open(const std::shared_ptr<ClientThread>& client);
	bool close(const std::shared_ptr<ClientThread>& client);
	bool goOnline(const std::shared_ptr<ClientThread>& client);
	bool goOffline(const std::shared_ptr<ClientThread>& client);
	void lock(const std::shared_ptr<ClientThread>& client, const std::chrono::milliseconds& maxDuration); // once lock is gained, hold it for up to maxDuration
	void unlock(const std::shared_ptr<ClientThread>& client);
private:
	std::shared_ptr<Device> device;
	std::mutex thisMutex; // guards all members
	std::mutex readersMutex;
	std::unordered_map<std::shared_ptr<InterprocessMailbox>, std::list<Communication::RawCallback>::iterator> readers;
	std::unordered_map<std::shared_ptr<InterprocessMailbox>, std::shared_ptr<WriterThread>> writers;
	std::set<std::shared_ptr<ClientThread>> openClients;
	std::set<std::shared_ptr<ClientThread>> onlineClients;
	std::mutex lockingLock; // used in lock/unlock() to control atomic operations
	std::chrono::time_point<std::chrono::steady_clock> lockHeldTill; // only allow the io lock to be held until this point
	std::shared_ptr<ClientThread> holdingLock;
	UnlockThread mUnlockThread;
};

} //namespace icsneo
#endif
