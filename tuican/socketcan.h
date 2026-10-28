#pragma once

#include <libdbc/libdbc.h>

#include <memory>
#include <string>

// Non-blocking raw SocketCAN socket that receives both classic and FD frames
class SocketCan
{
public:
    // Returns nullptr and fills error on failure
    static std::unique_ptr<SocketCan> Open(const std::string& interface, std::string& error);

    ~SocketCan();

    int Fd() const
    {
        return m_fd;
    }

    // Returns false once no more frames are waiting. Remote and error frames are skipped.
    // If reading failed for any other reason than an empty queue, error is set.
    bool Read(libdbc::CanFrame& frame, std::string& error);

private:
    explicit SocketCan(int fd)
        : m_fd(fd)
    {
    }

    int m_fd;
};
