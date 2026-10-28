#include "socketcan.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

/*static*/ std::unique_ptr<SocketCan> SocketCan::Open(const std::string& interface, std::string& error)
{
    unsigned int ifindex = if_nametoindex(interface.c_str());

    if (!ifindex)
    {
        error = "no such interface " + interface;
        return nullptr;
    }

    int fd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);

    if (fd < 0)
    {
        error = std::string("socket: ") + strerror(errno);
        return nullptr;
    }

    // Ask for FD frames too. Fails on kernels without FD support, which is fine: we just get classic frames.
    int enable = 1;
    setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable, sizeof(enable));

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = static_cast<int>(ifindex);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        error = "bind " + interface + ": " + strerror(errno);
        close(fd);
        return nullptr;
    }

    return std::unique_ptr<SocketCan>(new SocketCan(fd));
}

SocketCan::~SocketCan()
{
    close(m_fd);
}

bool SocketCan::Read(libdbc::CanFrame& frame, std::string& error)
{
    while (true)
    {
        canfd_frame raw;
        ssize_t n = read(m_fd, &raw, sizeof(raw));

        if (n < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            {
                error = strerror(errno);
            }

            return false;
        }

        if (n != CAN_MTU && n != CANFD_MTU)
        {
            // Not a frame we understand
            continue;
        }

        if (raw.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG))
        {
            continue;
        }

        // The classic can_frame's length field is in the same place as the FD one
        frame = {};
        frame.Extended = raw.can_id & CAN_EFF_FLAG;
        frame.Id = raw.can_id & (frame.Extended ? CAN_EFF_MASK : CAN_SFF_MASK);
        frame.Dlc = std::min<uint8_t>(raw.len, sizeof(frame.Data));
        memcpy(frame.Data, raw.data, frame.Dlc);

        return true;
    }
}
