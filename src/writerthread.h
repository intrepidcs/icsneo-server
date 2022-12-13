#ifndef __WRITER_THREAD_H_
#define __WRITER_THREAD_H_

#include <memory>
#include <string>
#include <thread>
#include "icsneo/communication/interprocessmailbox.h"
#include "icsneo/device/device.h"

namespace icsneo
{

class WriterThread : public std::enable_shared_from_this<WriterThread> {
public:
	WriterThread(const std::shared_ptr<Device>& device, const std::shared_ptr<InterprocessMailbox>& mailbox);
    ~WriterThread();
	void start();
	void stop();
private:
    void main();
    std::shared_ptr<Device> mDevice;
    std::shared_ptr<InterprocessMailbox> mMailbox;
    std::thread mThread;
    std::atomic<bool> mRunning = false;
};

} // namespace icsneo
#endif
