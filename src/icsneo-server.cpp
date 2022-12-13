#include <memory>
#include <array>
#include <csignal>

#include "spdlog/spdlog.h"

#include "icsneo/icsneocpp.h"
#include "icsneo/device/device.h"
#include "icsneo/communication/socket.h"
#include "icsneo/api/eventmanager.h"
#include "clientthread.h"

using namespace icsneo;

std::atomic<bool> CLOSE = false;
Acceptor ACCEPTOR(SocketBase::Protocol::TCP, RPC_PORT);

void signalHandler(int signal) {
	if(signal == SIGINT) {
		spdlog::info("SIGINT received, shutting down");
		CLOSE = true;
		ACCEPTOR.close();
	}
}

int main() {
	signal(SIGINT, signalHandler);

	icsneo::EventManager::GetInstance().downgradeErrorsOnCurrentThread();
	icsneo::EventManager::GetInstance().addEventCallback(icsneo::EventCallback([](std::shared_ptr<icsneo::APIEvent> evt) {
		switch(evt->getSeverity()) {
			case APIEvent::Severity::Error:
				spdlog::error(evt->describe());
				break;
			case APIEvent::Severity::EventWarning:
				spdlog::warn(evt->describe());
				break;
			case APIEvent::Severity::EventInfo:
				spdlog::info(evt->describe());
				break;
			default:
				spdlog::error("Unknown error/event");
		}
	}));

	if(!ACCEPTOR.initialize()) {
		spdlog::critical("Unable to bind acceptor socket");
		return -1;
	}

	uint8_t clientID = 0;
	std::array<std::shared_ptr<ClientThread>, UINT8_MAX> clientsThreads;
	while(!CLOSE) {
		auto comm = ACCEPTOR.accept();
		if(CLOSE || !comm)
			break;

		clientsThreads[clientID] = std::make_shared<ClientThread>(clientID, comm);

		clientID = ++clientID % clientsThreads.size();
	}
	return 0;
}