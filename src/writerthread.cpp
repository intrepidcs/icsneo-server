#include "spdlog/spdlog.h"
#include "writerthread.h"

namespace icsneo
{

WriterThread::WriterThread(const std::shared_ptr<Device>& device, const std::shared_ptr<InterprocessMailbox>& mailbox) :
	mDevice(device), mMailbox(mailbox) {
}

WriterThread::~WriterThread() {
	stop();
}

void WriterThread::start() {
	mRunning = true;
	mThread = std::thread(&WriterThread::main, this);
}

void WriterThread::stop() {
	mRunning = false;
	if(mThread.joinable())
		mThread.join();
}

void WriterThread::main() {
	std::vector<uint8_t> data;
	LengthFieldType actualLength;
	spdlog::info("WriterThread starting");
	while(mRunning) {
		data.resize(MAX_DATA_SIZE);
		if(!mMailbox->read(data.data(), actualLength, std::chrono::milliseconds(100))) {
			if(!mMailbox)
				break;
			continue;
		}

		if(actualLength > 0) {
			if(actualLength > MAX_DATA_SIZE) { // split message
				std::vector<uint8_t> reassembled(actualLength);
				std::copy(data.begin(), data.begin() + MAX_DATA_SIZE, reassembled.begin());
				auto offset = reassembled.data() + MAX_DATA_SIZE;
				for(auto remaining = actualLength - MAX_DATA_SIZE; remaining > 0; remaining -= actualLength) {
					if(!mMailbox->read(offset, actualLength, std::chrono::milliseconds(10))) {
						spdlog::error("Failed to read remaining data in an extended mailbox message");
						break;
					}
					offset += actualLength;
				}
				mDevice->com->rawWrite(reassembled);
			} else {
				data.resize(actualLength);
				mDevice->com->rawWrite(data);
			}
		}
	}
	spdlog::info("WriterThread stopping");
}

}
