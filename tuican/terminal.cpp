#include "terminal.h"

#include <algorithm>
#include <cerrno>
#include <utility>

#include <sys/ioctl.h>
#include <unistd.h>

Terminal::Terminal()
{
    m_ok = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && tcgetattr(STDIN_FILENO, &m_original) == 0;

    if (!m_ok)
    {
        return;
    }

    // No line buffering or echo, but keep ISIG so Ctrl-C still raises SIGINT
    termios raw = m_original;
    raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Alternate screen, hide cursor, no autowrap (so overlong lines are clipped instead of wrapping)
    Write("\x1b[?1049h\x1b[?25l\x1b[?7l");
}

Terminal::~Terminal()
{
    if (!m_ok)
    {
        return;
    }

    Write("\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_original);
}

int Terminal::Rows() const
{
    winsize ws;
    return (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row) ? ws.ws_row : 24;
}

int Terminal::Cols() const
{
    winsize ws;
    return (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col) ? ws.ws_col : 80;
}

void Terminal::Write(std::string_view str)
{
    while (!str.empty())
    {
        ssize_t n = write(STDOUT_FILENO, str.data(), str.size());

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return;
        }

        str.remove_prefix(n);
    }
}

std::vector<KeyEvent> Terminal::ReadKeys()
{
    static const std::pair<std::string_view, Key> sequences[] =
    {
        { "\x1b[A", Key::Up },
        { "\x1b[B", Key::Down },
        { "\x1bOA", Key::Up },
        { "\x1bOB", Key::Down },
        { "\x1b[5~", Key::PageUp },
        { "\x1b[6~", Key::PageDown },
        { "\x1b[H", Key::Home },
        { "\x1b[F", Key::End },
        { "\x1bOH", Key::Home },
        { "\x1bOF", Key::End },
        { "\x1b[1~", Key::Home },
        { "\x1b[4~", Key::End },
    };

    char buffer[128];
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

    std::vector<KeyEvent> result;

    if (n <= 0)
    {
        return result;
    }

    std::string_view in(buffer, n);

    while (!in.empty())
    {
        if (in[0] == '\x1b')
        {
            bool matched = false;

            for (const auto& [sequence, key] : sequences)
            {
                if (in.starts_with(sequence))
                {
                    result.push_back({ key });
                    in.remove_prefix(sequence.size());
                    matched = true;
                    break;
                }
            }

            if (matched)
            {
                continue;
            }

            if (in.size() > 1 && in[1] == '[')
            {
                // Some other CSI sequence: skip up to and including its final byte
                size_t end = 2;
                while (end < in.size() && !(in[end] >= 0x40 && in[end] <= 0x7E))
                {
                    end++;
                }

                in.remove_prefix(std::min(end + 1, in.size()));
                continue;
            }

            result.push_back({ Key::Escape });
            in.remove_prefix(1);
        }
        else
        {
            char c = in[0];
            in.remove_prefix(1);

            if (c == '\r' || c == '\n')
            {
                result.push_back({ Key::Enter });
            }
            else if (c == 0x7F || c == 0x08)
            {
                result.push_back({ Key::Backspace });
            }
            else
            {
                result.push_back({ Key::Char, c });
            }
        }
    }

    return result;
}
