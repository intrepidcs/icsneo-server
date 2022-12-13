#ifndef __DEVICE_CONTEXT_STORE_H_
#define __DEVICE_CONTEXT_STORE_H_

#include <mutex>
#include <unordered_map>
#include <memory>
#include "devicecontext.h"

namespace icsneo
{

class DeviceContextStore {
public:
	static std::shared_ptr<DeviceContextStore> getDeviceContextStore();
	std::vector<std::shared_ptr<DeviceContext>> findAll();
	std::shared_ptr<DeviceContext> find(const std::string& serial);
private:
	std::mutex mutex;
	std::unordered_map<std::string, std::shared_ptr<DeviceContext>> deviceContexts;
};

} //namespace icsneo

#endif
