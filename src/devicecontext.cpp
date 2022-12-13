#include "spdlog/spdlog.h"
#include "devicecontext.h"

namespace icsneo
{

bool DeviceContext::addReader(const std::shared_ptr<InterprocessMailbox>& mb) {
	std::scoped_lock lk(readersMutex);

	device->com->modifyRawCallbacks([&](auto& callbacks) {
		readers[mb] = callbacks.emplace(callbacks.end(), [=](std::vector<uint8_t>& data) {
			const auto dataSize = static_cast<LengthFieldType>(data.size());
			const auto tryWrite = [&](const void* input, LengthFieldType length) -> bool {
				for(int i = 0; i < 50; ++i) { // try to write for 5s, making sure we can close if need be
					if(mb->write(input, length, std::chrono::milliseconds(100)))
						return true;
					if(!mb)
						return false;
				}
				spdlog::error("Unable to write message");
				return false;
			};

			if(!tryWrite(data.data(), dataSize))
				return;

			if(data.size() > MAX_DATA_SIZE) {
				auto offset = data.data() + MAX_DATA_SIZE;
				for(LengthFieldType remaining = dataSize - MAX_DATA_SIZE; remaining > 0; ) {
					const LengthFieldType toWrite = std::min(MAX_DATA_SIZE, remaining);
					if(!tryWrite(offset, toWrite))
						return;
					remaining -= toWrite;
					offset += toWrite;
				}
			}
		});
	});

	return true;
}

bool DeviceContext::removeReader(const std::shared_ptr<InterprocessMailbox>& mb) {
	std::scoped_lock lk(thisMutex);
	auto it = readers.find(mb);
	if(it == readers.end())
		return false;
	device->com->modifyRawCallbacks([&](auto& callbacks) {
		callbacks.erase(it->second);
	});
	return true;
}

bool DeviceContext::addWriter(const std::shared_ptr<InterprocessMailbox>& mb) {
	std::scoped_lock lk(thisMutex);
	auto [thread, inserted] = writers.emplace(std::make_pair(mb, std::make_shared<WriterThread>(device, mb)));
	if(!inserted) {
		spdlog::error("Unable to add writer");
		return false;
	}
	thread->second->start();
	return true;
}

bool DeviceContext::removeWriter(const std::shared_ptr<InterprocessMailbox>& mb) {
	std::scoped_lock lk(thisMutex);
	if(const auto it = writers.find(mb); it != writers.end()) {
		it->second->stop();
		writers.erase(it);
		return true;
	}
	spdlog::error("Unable to remove writer");
	return false;
}

bool DeviceContext::open(const std::shared_ptr<ClientThread>& client) {
	std::scoped_lock lk(thisMutex);
	if(!openClients.emplace(client).second) {
		spdlog::error("Multiple calls to open");
		return false;
	}

	if(device->isOpen())
		return true;

	device->settings->disabled = true;
	return device->open();
}

bool DeviceContext::close(const std::shared_ptr<ClientThread>& client) {
	std::scoped_lock lk(thisMutex);
	if(openClients.erase(client) == 0) {
		spdlog::error("Close issued on unopen device");
		return false;
	}

	if(openClients.empty())
		return device->close();

	return true;

}

bool DeviceContext::goOnline(const std::shared_ptr<ClientThread>& client) {
	std::scoped_lock lk(thisMutex);
	if(!onlineClients.emplace(client).second) {
		spdlog::error("Multiple calls to goOnline");
		return false;
	}

	if(device->isOnline())
		return true;

	return device->goOnline();
}

bool DeviceContext::goOffline(const std::shared_ptr<ClientThread>& client) {
	std::scoped_lock lk(thisMutex);
	if(onlineClients.erase(client) == 0) {
		spdlog::error("Close issued on device that is not online");
		return false;
	}

	if(onlineClients.empty())
		return device->goOffline();

	return true;
}


void DeviceContext::lock(const std::shared_ptr<ClientThread>& client, const std::chrono::milliseconds& maxDuration) {
	mUnlockThread.lock(maxDuration);
}

void DeviceContext::unlock(const std::shared_ptr<ClientThread>& client) {
	mUnlockThread.unlock();
}

} //namespace icsneo