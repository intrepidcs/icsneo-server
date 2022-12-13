#include "spdlog/spdlog.h"

#include "clientthread.h"
#include "devicecontextstore.h"
#include "icsneo/icsneocpp.h"

namespace icsneo
{

ClientThread::ClientThread(uint8_t id, std::shared_ptr<ActiveSocket> comm)
	: mID(id), clientComm(comm) {
	thread = std::thread(&ClientThread::run, this);
}

ClientThread::~ClientThread() {
	#ifdef _WIN32
	// TODO
	#else
	pthread_kill(thread.native_handle(), SIGTERM);
	#endif
	stop();
	if(thread.joinable())
		thread.join();
}

void ClientThread::run() {
	RPC rpc;
	spdlog::info("Client {} connected", mID);
	while(!mStop) {
		if(!clientComm->read(&rpc, sizeof(rpc)))
			break;

		switch(rpc) {
			case RPC::DEVICE_FINDER_GET_SUPORTED_DEVICES:
				{
					const auto supported = GetSupportedDevices();
					const uint16_t count = static_cast<uint16_t>(supported.size());
					std::vector<devicetype_t> devices(count);
					for(unsigned i = 0; i < count; ++i)
						devices[i] = supported[i].getDeviceType();

					if(!clientComm->write(&count, sizeof(count)))
						break;

					if(!clientComm->write(devices.data(), devices.size() * sizeof(devicetype_t)))
						break;
				}
				break;
			case RPC::DEVICE_FINDER_FIND_ALL:
				{
					auto devs = DeviceContextStore::getDeviceContextStore()->findAll();
					std::vector<std::array<char, sizeof(FoundDevice::serial)>> serials(devs.size());
					for(std::size_t i = 0; i < devs.size(); ++i) {
						const auto serialStr = devs[i]->getDevice()->getSerial();
						std::copy(serialStr.begin(), serialStr.end(), serials[i].begin());
						*(std::end(serials[i]) - 1) = '\0';
					}

					if(!clientComm->writeTyped(static_cast<uint16_t>(serials.size())))
						break;

					if(!clientComm->write(serials.data(), serials.size() * sizeof(FoundDevice::serial)))
						break;
				}
				break;
			case RPC::SDIO_OPEN:
				{
					std::string serial;
					std::string inID;
					std::string outID;
					// lambda so we can return early
					const auto sdioOpen = [&]() -> bool {
						if(!clientComm->readString(serial))
							return false;
						inID = "icsneo-" + serial + "-i-" + std::to_string(mID);
						outID = "icsneo-" + serial + "-o-" + std::to_string(mID);
						auto in = std::make_shared<InterprocessMailbox>();
						if(!in->open(inID, true))
							return false;
						auto out = std::make_shared<InterprocessMailbox>();
						if(!out->open(outID, true))
							return false;
						auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);
						if(!deviceContext) {
							spdlog::error("No device context for serial '{}'", serial);
						}
						if(!deviceContext->addWriter(in))
							return false;
						if(!deviceContext->addReader(out))
							return false;
						sdioWriters[serial] = in;
						sdioReaders[serial] = out;
						return true;
					};
					// returning essentially a std::optional<std::pair<std::string, std::string>>
					const auto ret = sdioOpen();
					if(!ret)
						spdlog::error("Failed to open mailboxes");

					if(!clientComm->writeTyped(ret))
						break;

					if(ret) {
						if(!clientComm->writeString(outID))
							break;

						if(!clientComm->writeString(inID))
							break;
					}
				}
				break;
			case RPC::SDIO_CLOSE:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;
					const auto close = [&]() -> bool {
						auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);
						{
							auto it = sdioWriters.find(serial);
							if(it == sdioWriters.end())
								return false;
							if(!deviceContext->removeWriter(it->second))
								return false;
							sdioWriters.erase(it);
						}
						{
							auto it = sdioReaders.find(serial);
							if(it == sdioReaders.end())
								return false;
							if(!deviceContext->removeReader(it->second))
								return false;
							sdioReaders.erase(it);
						}
						return true;
					};
					clientComm->writeTyped(close());
				}
				break;
			case RPC::DEVICE_OPEN:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;

					auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);

					if(!clientComm->writeTyped(deviceContext->open(shared_from_this())))
						break;
				}
				break;
			case RPC::DEVICE_GO_ONLINE:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;

					auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);

					if(!clientComm->writeTyped(deviceContext->goOnline(shared_from_this())))
						break;
				}
				break;
			case RPC::DEVICE_GO_OFFLINE:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;

					auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);

					if(!clientComm->writeTyped(deviceContext->goOffline(shared_from_this())))
						break;
				}

				break;
			case RPC::DEVICE_CLOSE:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;

					auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);

					bool ret = deviceContext->close(shared_from_this());
					if(!clientComm->writeTyped(ret))
						break;
				}
				break;
			case RPC::DEVICE_LOCK:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;

					int64_t maxDuration;
					if(!clientComm->readTyped(maxDuration))
						break;

					auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);
					if(!deviceContext)
						break;

					deviceContext->lock(shared_from_this(), std::chrono::milliseconds(maxDuration));

					if(!clientComm->writeTyped(true))
						break;
				}
				break;
			case RPC::DEVICE_UNLOCK:
				{
					std::string serial;
					if(!clientComm->readString(serial))
						break;

					auto deviceContext = DeviceContextStore::getDeviceContextStore()->find(serial);
					if(!deviceContext)
						break;

					deviceContext->unlock(shared_from_this());

					if(!clientComm->writeTyped(true))
						break;
				}
				break;
			case RPC::GET_EVENTS:
				{
					std::vector<APIEvent> events = icsneo::GetEvents(EventFilter());
					std::vector<neosocketevent_t> eventStructs;
					for(APIEvent& event : events)
					{
						eventStructs.push_back(event.getNeoSocketEvent());
					}
					if(eventStructs.size() == events.size())
					{
						clientComm->writeTyped<size_t>(eventStructs.size());
						clientComm->write(eventStructs.data(), (eventStructs.size() * sizeof(neosocketevent_t)));
					}
					else
						clientComm->writeTyped<size_t>(0);

					break;
				}
				break;
			case RPC::GET_LAST_ERROR:
				{
					icsneo::APIEvent event = icsneo::GetLastError();
					clientComm->writeTyped<neosocketevent_t>(event.getNeoSocketEvent());
				}
				break;
			case RPC::GET_EVENT_COUNT:
				{
					neosocketeventfilter_t eventFilter;
					if(!clientComm->readTyped(eventFilter))
						break;

					EventFilter ef(eventFilter);
					clientComm->writeTyped<size_t>(icsneo::EventCount(ef));
				}
				break;
			case RPC::DISCARD_EVENTS:
				{
					neosocketeventfilter_t eventFilter;
					if(!clientComm->readTyped(eventFilter))
						break;

					EventFilter ef(eventFilter);
					icsneo::DiscardEvents(ef);
				}
				break;
			case RPC::SET_EVENT_LIMIT:
				{
					size_t limit;
					if(!clientComm->readTyped(limit))
						break;

					icsneo::SetEventLimit(limit);
				}
				break;
			case RPC::GET_EVENT_LIMIT:
				{
					clientComm->writeTyped<size_t>(icsneo::GetEventLimit());
				}
				break;
		}
	}
	spdlog::info("Client {} disconnected", mID);
}

void ClientThread::stop() {
	mStop = true;
	clientComm->close();
}

} //namespace icsneo
