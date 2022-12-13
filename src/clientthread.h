#ifndef __CLIENT_THREAD_H_
#define __CLIENT_THREAD_H_

#include <unordered_map>
#include <memory>
#include <thread>
#include <string>
#include "icsneo/device/device.h"
#include "icsneo/communication/socket.h"
#include "icsneo/communication/interprocessmailbox.h"

namespace icsneo
{

class ClientThread : public std::enable_shared_from_this<ClientThread> {
public:
	ClientThread(uint8_t id, std::shared_ptr<ActiveSocket> comm);
	~ClientThread();
	void run();
	void stop();
private:
	bool mStop = false;
	const uint8_t mID;
	std::thread thread;
	std::shared_ptr<ActiveSocket> clientComm;
	std::unordered_map<std::string, std::shared_ptr<InterprocessMailbox>> sdioWriters;
	std::unordered_map<std::string, std::shared_ptr<InterprocessMailbox>> sdioReaders;
};

} // namespace icsneo
#endif
