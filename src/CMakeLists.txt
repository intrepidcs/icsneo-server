add_executable(icsneo-server
    icsneo-server.cpp
    clientthread.cpp
    devicecontext.cpp
    devicecontextstore.cpp
    writerthread.cpp
    unlockthread.cpp
)

target_link_libraries(icsneo-server icsneocpp)
