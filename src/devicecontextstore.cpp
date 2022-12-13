#include "devicecontextstore.h"
#include "icsneo/icsneocpp.h"

namespace icsneo
{

std::shared_ptr<DeviceContextStore> DeviceContextStore::getDeviceContextStore() {
	static std::shared_ptr<DeviceContextStore> store = std::make_shared<DeviceContextStore>();
	return store;
}

std::vector<std::shared_ptr<DeviceContext>> DeviceContextStore::findAll() {
	std::scoped_lock lk(mutex);
	auto devs = FindAllDevices();
	std::vector<std::shared_ptr<DeviceContext>> ret;
	ret.reserve(devs.size());
	for(auto& dev : devs) {
		auto ct = deviceContexts.emplace(std::make_pair(dev->getSerial(), std::make_shared<DeviceContext>(dev)));
		ret.emplace_back(ct.first->second);
	}
	return ret;
}

std::shared_ptr<DeviceContext> DeviceContextStore::find(const std::string& serial) {
	std::scoped_lock lk(mutex);
	if(auto it = deviceContexts.find(serial); it != deviceContexts.end())
		return it->second;

	return nullptr;
}

} //namespace icsneo
