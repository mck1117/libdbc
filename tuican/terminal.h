#pragma once

#include <string_view>
#include <vector>

#include <termios.h>

enum class Key
{
    Char,
    Up,
    Down,
    PageUp,
    PageDown,
    Home,
    End,
    Enter,
    Backspace,
    Escape,
};

struct KeyEvent
{
    Key Type;
    char Char = 0;
};

// Raw-mode terminal on the alternate screen. The destructor puts everything back.
class Terminal
{
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    // False if stdin/stdout isn't a terminal
    bool Ok() const
    {
        return m_ok;
    }

    int Rows() const;
    int Cols() const;

    void Write(std::string_view str);

    // Non-blocking: returns whatever keys are waiting on stdin
    std::vector<KeyEvent> ReadKeys();

private:
    bool m_ok = false;
    termios m_original;
};
