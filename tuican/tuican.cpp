#include "socketcan.h"
#include "terminal.h"

#include <libdbc/libdbc.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <poll.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

static volatile sig_atomic_t g_quit = 0;
static volatile sig_atomic_t g_resized = 0;

static void OnQuitSignal(int)
{
    g_quit = 1;
}

static void OnResize(int)
{
    g_resized = 1;
}

static void InstallSignal(int sig, void (*handler)(int))
{
    // No SA_RESTART, so a signal wakes up poll()
    struct sigaction sa{};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

// A value is highlighted for this long after it changes
static constexpr auto ChangedHighlight = 300ms;
// A signal not updated for this long is drawn dim
static constexpr auto StaleAfter = 2s;
static constexpr auto RedrawInterval = 100ms;

struct Row
{
    const libdbc::Signal* Signal;
    // MessageName.SignalName
    std::string FullName;
    size_t DbcIndex;
    // Decimal places needed to show every value this signal can take
    int Decimals;

    // Widest VAL_ name this signal has, so the value column doesn't change width as values do
    size_t ValueNameWidth = 0;

    bool Seen = false;
    float Value = 0;
    uint64_t Raw = 0;
    Clock::time_point LastUpdate{};
    Clock::time_point LastChange{};
};

// Decimal places needed to show x exactly, up to a limit
static int DecimalsFor(float x)
{
    constexpr int maxDecimals = 6;

    double scaled = std::fabs(x);
    for (int d = 0; d < maxDecimals; d++)
    {
        if (std::fabs(scaled - std::round(scaled)) < 1e-3)
        {
            return d;
        }

        scaled *= 10;
    }

    return maxDecimals;
}

// Enough decimal places to show a signal's resolution, without the noise of factors like 0.0245166
static int DecimalsForFactor(float factor)
{
    if (factor == 0)
    {
        return 0;
    }

    // Two significant digits of the factor
    int resolution = std::max(0, static_cast<int>(std::ceil(-std::log10(std::fabs(factor)))) + 1);

    return std::min(DecimalsFor(factor), resolution);
}

static bool LessCaseInsensitive(std::string_view a, std::string_view b)
{
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y)
    {
        return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
    });
}

static bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
{
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), [](char x, char y)
    {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });

    return it != haystack.end();
}

// Display width of a string, assuming one column per UTF-8 code point
static size_t Width(std::string_view s)
{
    return std::count_if(s.begin(), s.end(), [](char c) { return (c & 0xC0) != 0x80; });
}

// Appends s padded or truncated to exactly width columns
static void AppendFit(std::string& out, std::string_view s, size_t width, bool alignRight = false)
{
    size_t cols = 0;
    size_t end = 0;

    for (; end < s.size(); end++)
    {
        if ((s[end] & 0xC0) != 0x80)
        {
            if (cols == width)
            {
                break;
            }

            cols++;
        }
    }

    if (alignRight)
    {
        out.append(width - cols, ' ');
    }

    out.append(s.substr(0, end));

    if (!alignRight)
    {
        out.append(width - cols, ' ');
    }
}

// Escape sequence for a subtle background shade, from the 256 colour palette. Many terminals set
// COLORFGBG to "fg;bg" with the background as an ANSI colour number: 7 or 15 means light, assume dark otherwise.
static std::string StripeBackground()
{
    bool light = false;

    if (const char* fgbg = getenv("COLORFGBG"))
    {
        std::string_view value = fgbg;
        std::string_view bg = value.substr(value.rfind(';') + 1);
        light = bg == "7" || bg == "15";
    }

    return light ? "\x1b[48;5;254m" : "\x1b[48;5;236m";
}

// Like AppendFit, but if s is too long, trims the start instead so the end (the signal name) stays visible
static void AppendFitKeepEnd(std::string& out, std::string_view s, size_t width)
{
    size_t cols = Width(s);

    if (cols <= width || width == 0)
    {
        AppendFit(out, s, width);
        return;
    }

    // Skip code points until what's left, plus the ellipsis, fits
    size_t skip = cols - width + 1;
    size_t start = 0;

    while (skip)
    {
        start++;

        if (start >= s.size() || (s[start] & 0xC0) != 0x80)
        {
            skip--;
        }
    }

    out += "…";
    out.append(s.substr(start));
}

static std::string FormatValue(const Row& row)
{
    char buffer[64];

    if (row.Signal->Type == libdbc::ValueType::Integer)
    {
        snprintf(buffer, sizeof(buffer), "%.*f", row.Decimals, row.Value);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%.6g", row.Value);
    }

    return buffer;
}

static std::string FormatAge(Clock::duration age)
{
    char buffer[16];
    double sec = std::chrono::duration<double>(age).count();

    if (sec < 10)
    {
        snprintf(buffer, sizeof(buffer), "%.1fs", sec);
    }
    else if (sec < 60)
    {
        snprintf(buffer, sizeof(buffer), "%.0fs", sec);
    }
    else if (sec < 3600)
    {
        snprintf(buffer, sizeof(buffer), "%.0fm", sec / 60);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%.0fh", sec / 3600);
    }

    return buffer;
}

class App
{
public:
    App(std::string interface, std::vector<std::unique_ptr<libdbc::Dbc>> dbcs, SocketCan& can, Terminal& terminal)
        : m_interface(std::move(interface))
        , m_dbcs(std::move(dbcs))
        , m_can(can)
        , m_terminal(terminal)
        , m_stripe(StripeBackground())
    {
        for (size_t i = 0; i < m_dbcs.size(); i++)
        {
            for (const auto& [key, message] : m_dbcs[i]->Messages())
            {
                for (const auto& signal : message.Signals)
                {
                    int decimals = std::max(DecimalsForFactor(signal.Factor), DecimalsFor(signal.Offset));
                    m_rows.push_back({ &signal, message.Name + "." + signal.Name, i, decimals });

                    for (const auto& [bits, name] : signal.ValueNames)
                    {
                        m_rows.back().ValueNameWidth = std::max(m_rows.back().ValueNameWidth, Width(name));
                    }
                }
            }
        }

        std::sort(m_rows.begin(), m_rows.end(), [](const Row& a, const Row& b)
        {
            return LessCaseInsensitive(a.FullName, b.FullName);
        });

        // Map each DBC's signal IDs to rows, so decoding can find them quickly
        m_rowIndex.resize(m_dbcs.size());
        for (size_t i = 0; i < m_dbcs.size(); i++)
        {
            m_rowIndex[i].resize(m_dbcs[i]->SignalCount());
        }

        for (size_t r = 0; r < m_rows.size(); r++)
        {
            m_rowIndex[m_rows[r].DbcIndex][m_rows[r].Signal->Id] = r;
        }
    }

    void Run()
    {
        pollfd fds[2] =
        {
            { m_can.Fd(), POLLIN, 0 },
            { STDIN_FILENO, POLLIN, 0 },
        };

        auto nextDraw = Clock::now();
        auto canRetry = Clock::time_point{};
        m_fpsWindowStart = Clock::now();

        while (!g_quit)
        {
            auto now = Clock::now();

            // After a socket error, back off instead of spinning on it
            fds[0].fd = (now >= canRetry) ? m_can.Fd() : -1;

            auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(nextDraw - now).count();
            poll(fds, 2, std::max<int>(0, static_cast<int>(timeout)));

            if (fds[0].fd >= 0 && fds[0].revents)
            {
                if (!ReadFrames())
                {
                    canRetry = Clock::now() + 1s;
                }
            }

            bool redraw = false;

            if (fds[1].revents & POLLIN)
            {
                for (const auto& key : m_terminal.ReadKeys())
                {
                    HandleKey(key);
                }

                redraw = true;
            }

            if (g_resized)
            {
                g_resized = 0;
                redraw = true;
            }

            now = Clock::now();
            if (redraw || now >= nextDraw)
            {
                UpdateFps(now);
                Draw(now);
                nextDraw = now + RedrawInterval;
            }
        }
    }

private:
    // Returns false if the socket reported an error
    bool ReadFrames()
    {
        libdbc::CanFrame frame;
        std::string error;

        while (m_can.Read(frame, error))
        {
            m_error.clear();
            m_frameCount++;

            auto now = Clock::now();
            bool known = false;

            for (size_t i = 0; i < m_dbcs.size(); i++)
            {
                if (!m_dbcs[i]->FindMessage(frame.Id, frame.Extended))
                {
                    continue;
                }

                known = true;

                m_dbcs[i]->Decode(frame, [&](const libdbc::Signal& signal, uint64_t bits, float value)
                {
                    Row& row = m_rows[m_rowIndex[i][signal.Id]];

                    if (!row.Seen || row.Value != value)
                    {
                        row.LastChange = now;
                    }

                    row.Seen = true;
                    row.Value = value;
                    row.Raw = bits;
                    row.LastUpdate = now;
                });
            }

            if (!known)
            {
                m_unknownIds.insert(frame.Extended ? (frame.Id | libdbc::Dbc::ExtendedIdFlag) : frame.Id);
            }
        }

        if (!error.empty())
        {
            m_error = error;
            return false;
        }

        return true;
    }

    void UpdateFps(Clock::time_point now)
    {
        auto elapsed = now - m_fpsWindowStart;

        if (elapsed >= 1s)
        {
            m_fps = (m_frameCount - m_fpsWindowFrames) / std::chrono::duration<double>(elapsed).count();
            m_fpsWindowStart = now;
            m_fpsWindowFrames = m_frameCount;
        }
    }

    int PageSize() const
    {
        // Less the title, column header and footer
        return std::max(1, m_terminal.Rows() - 3);
    }

    void HandleKey(const KeyEvent& key)
    {
        if (m_editingFilter)
        {
            switch (key.Type)
            {
            case Key::Enter:
                m_editingFilter = false;
                break;
            case Key::Escape:
                m_editingFilter = false;
                m_filter.clear();
                break;
            case Key::Backspace:
                if (!m_filter.empty())
                {
                    m_filter.pop_back();
                }
                break;
            case Key::Char:
                if (std::isprint(static_cast<unsigned char>(key.Char)))
                {
                    m_filter.push_back(key.Char);
                }
                break;
            default:
                break;
            }

            m_scroll = 0;
            return;
        }

        switch (key.Type)
        {
        case Key::Up:       m_scroll -= 1; break;
        case Key::Down:     m_scroll += 1; break;
        case Key::PageUp:   m_scroll -= PageSize(); break;
        case Key::PageDown: m_scroll += PageSize(); break;
        case Key::Home:     m_scroll = 0; break;
        case Key::End:      m_scroll = static_cast<int>(m_rows.size()); break;
        case Key::Escape:   m_filter.clear(); break;
        case Key::Char:
            switch (key.Char)
            {
            case 'q':
            case 'Q':
                g_quit = 1;
                break;
            case 'k': m_scroll -= 1; break;
            case 'j': m_scroll += 1; break;
            case '/':
                m_editingFilter = true;
                break;
            case 'h':
                m_hideUnseen = !m_hideUnseen;
                m_scroll = 0;
                break;
            }
            break;
        default:
            break;
        }
    }

    void Draw(Clock::time_point now)
    {
        const size_t cols = m_terminal.Cols();

        std::vector<const Row*> visible;
        size_t nameW = 6, unitW = 4, valueW = 14;

        for (const auto& row : m_rows)
        {
            if (m_hideUnseen && !row.Seen)
            {
                continue;
            }

            if (!m_filter.empty() && !ContainsCaseInsensitive(row.FullName, m_filter))
            {
                continue;
            }

            visible.push_back(&row);
            nameW = std::max(nameW, Width(row.FullName));
            unitW = std::max(unitW, Width(row.Signal->Unit));
            valueW = std::max(valueW, row.ValueNameWidth);
        }

        // Column layout: shrink the name column to fit the terminal
        constexpr size_t ageW = 6, gap = 2;
        nameW = std::min<size_t>(nameW, 64);
        valueW = std::min<size_t>(valueW, 24);
        unitW = std::min<size_t>(unitW, 10);

        auto total = [&] { return 1 + nameW + gap + valueW + 1 + unitW + gap + ageW; };

        while (total() > cols && nameW > 12)
        {
            nameW--;
        }

        const int page = PageSize();
        m_scroll = std::clamp(m_scroll, 0, std::max(0, static_cast<int>(visible.size()) - page));

        std::string out = "\x1b[H";

        // Lines are padded out to the full width rather than relying on erase-to-end-of-line, so
        // backgrounds span the whole line. Erasing would also blank the last column of a full line.
        auto padTo = [&](size_t used)
        {
            if (used < cols)
            {
                out.append(cols - used, ' ');
            }
        };

        auto endLine = [&](bool erase, bool last = false)
        {
            out += erase ? "\x1b[0m\x1b[K" : "\x1b[0m";
            if (!last)
            {
                out += "\r\n";
            }
        };

        // Title bar
        {
            char stats[160];
            snprintf(stats, sizeof(stats), " tuican | %s | %zu DBC%s | %zu/%zu signals | %.0f frames/s | %zu unknown IDs ",
                m_interface.c_str(), m_dbcs.size(), m_dbcs.size() == 1 ? "" : "s",
                visible.size(), m_rows.size(), m_fps, m_unknownIds.size());

            std::string title = stats;
            if (!m_error.empty())
            {
                title += "| " + m_interface + ": " + m_error + " ";
            }

            out += m_error.empty() ? "\x1b[7m" : "\x1b[7;31m";
            AppendFit(out, title, cols);
            endLine(false);
        }

        // Column headers
        out += "\x1b[1m ";
        AppendFit(out, "Signal", nameW);
        out.append(gap, ' ');
        AppendFit(out, "Value", valueW, true);
        out += ' ';
        AppendFit(out, "Unit", unitW);
        out.append(gap, ' ');
        AppendFit(out, "Age", ageW, true);
        padTo(total());
        endLine(false);

        for (int i = 0; i < page; i++)
        {
            size_t index = m_scroll + i;

            if (index < visible.size())
            {
                const Row& row = *visible[index];

                // Shade every other row, counting from the top of the list so stripes scroll with it
                if (index % 2)
                {
                    out += m_stripe;
                }

                bool stale = !row.Seen || now - row.LastUpdate > StaleAfter;
                bool changed = row.Seen && now - row.LastChange < ChangedHighlight;

                if (stale)
                {
                    out += "\x1b[2m";
                }

                out += ' ';
                AppendFitKeepEnd(out, row.FullName, nameW);
                out.append(gap, ' ');

                // Values with a VAL_ name show the name instead, in cyan, without a unit
                const std::string* valueName = row.Seen ? row.Signal->ValueName(row.Raw) : nullptr;

                if (changed)
                {
                    out += "\x1b[1;32m";
                }
                else if (valueName)
                {
                    out += "\x1b[36m";
                }

                AppendFit(out, valueName ? *valueName : row.Seen ? FormatValue(row) : "-", valueW, true);

                if (changed)
                {
                    // Changed values are never stale, so this can't cancel the row's dimming
                    out += "\x1b[22;39m";
                }
                else if (valueName)
                {
                    out += "\x1b[39m";
                }

                out += ' ';
                AppendFit(out, valueName ? "" : row.Signal->Unit, unitW);
                out.append(gap, ' ');
                AppendFit(out, row.Seen ? FormatAge(now - row.LastUpdate) : "", ageW, true);
                padTo(total());
                endLine(false);
            }
            else
            {
                endLine(true);
            }
        }

        // Footer
        if (m_editingFilter)
        {
            out += " Filter: " + m_filter + "\x1b[7m \x1b[27m  (Enter to keep, Esc to clear)";
        }
        else
        {
            out += "\x1b[2m q quit  ↑↓ PgUp PgDn scroll  / filter  h ";
            out += m_hideUnseen ? "show" : "hide";
            out += " unseen";

            if (!m_filter.empty())
            {
                out += "\x1b[22m  filter: \x1b[1m" + m_filter + "\x1b[22m\x1b[2m (Esc clears)";
            }
        }

        endLine(true, true);

        // Clear anything below, in case the terminal grew
        out += "\x1b[J";

        m_terminal.Write(out);
    }

    const std::string m_interface;
    const std::vector<std::unique_ptr<libdbc::Dbc>> m_dbcs;
    SocketCan& m_can;
    Terminal& m_terminal;

    // Background escape for shaded rows
    const std::string m_stripe;

    std::vector<Row> m_rows;
    // [dbc index][signal id] -> index in m_rows
    std::vector<std::vector<size_t>> m_rowIndex;

    std::unordered_set<uint32_t> m_unknownIds;
    std::string m_error;

    uint64_t m_frameCount = 0;
    uint64_t m_fpsWindowFrames = 0;
    Clock::time_point m_fpsWindowStart;
    double m_fps = 0;

    int m_scroll = 0;
    std::string m_filter;
    bool m_editingFilter = false;
    bool m_hideUnseen = false;
};

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: tuican <interface> <file.dbc> [more.dbc ...]" << std::endl;
        return 1;
    }

    std::string interface = argv[1];
    std::vector<std::unique_ptr<libdbc::Dbc>> dbcs;

    for (int i = 2; i < argc; i++)
    {
        std::ifstream file(argv[i]);

        if (!file)
        {
            std::cerr << "DBC file \"" << argv[i] << "\" failed to open" << std::endl;
            return 1;
        }

        auto dbc = libdbc::Dbc::Parse(file);

        if (!dbc)
        {
            std::cerr << "DBC file \"" << argv[i] << "\" failed to parse" << std::endl;
            return 1;
        }

        dbcs.push_back(std::move(dbc));
    }

    std::string error;
    auto can = SocketCan::Open(interface, error);

    if (!can)
    {
        std::cerr << "Failed to open " << interface << ": " << error << std::endl;
        return 1;
    }

    InstallSignal(SIGINT, OnQuitSignal);
    InstallSignal(SIGTERM, OnQuitSignal);
    InstallSignal(SIGHUP, OnQuitSignal);
    InstallSignal(SIGWINCH, OnResize);

    Terminal terminal;

    if (!terminal.Ok())
    {
        std::cerr << "tuican needs to run in a terminal" << std::endl;
        return 1;
    }

    App app(interface, std::move(dbcs), *can, terminal);
    app.Run();

    return 0;
}
