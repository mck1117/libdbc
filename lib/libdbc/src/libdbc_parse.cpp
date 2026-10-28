#include <libdbc/libdbc.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iterator>
#include <string>

using namespace libdbc;

namespace
{

// Pseudo-message that holds signals not assigned to any real message
constexpr uint32_t IndependentSignalsMessageId = 0xC0000000;

enum class TokenType
{
    Word,       // Keywords, identifiers and numbers
    String,     // Contents of a quoted string, without the quotes (escapes are left in place)
    Punct,      // One of : | @ ( ) [ ] , ;
    Newline,
    End,
};

struct Token
{
    TokenType Type;
    std::string_view Text;

    bool Is(TokenType type, std::string_view text) const
    {
        return Type == type && Text == text;
    }
};

static bool IsPunct(char c)
{
    return std::string_view(":|@()[],;").find(c) != std::string_view::npos;
}

static bool IsSpace(char c)
{
    return std::isspace(static_cast<unsigned char>(c));
}

// Splits DBC text into tokens. Newlines are significant, since several statements (BO_, SG_, ...)
// aren't terminated by a semicolon, but strings may span multiple lines.
class Tokenizer
{
public:
    Tokenizer(std::string_view text)
        : m_text(text)
    {
        Advance();
    }

    const Token& Peek() const
    {
        return m_current;
    }

    Token Next()
    {
        Token result = m_current;
        Advance();
        return result;
    }

    // Set if the text ended inside a string
    bool Failed() const
    {
        return m_failed;
    }

private:
    void Advance()
    {
        // Skip whitespace, except newlines
        while (m_pos < m_text.size() && m_text[m_pos] != '\n' && IsSpace(m_text[m_pos]))
        {
            m_pos++;
        }

        if (m_pos >= m_text.size())
        {
            m_current = { TokenType::End, {} };
            return;
        }

        size_t start = m_pos;
        char c = m_text[m_pos];

        if (c == '\n')
        {
            m_pos++;
            m_current = { TokenType::Newline, m_text.substr(start, 1) };
        }
        else if (c == '"')
        {
            // Find the closing quote, skipping over escaped characters
            size_t end = start + 1;
            while (end < m_text.size() && m_text[end] != '"')
            {
                end += (m_text[end] == '\\') ? 2 : 1;
            }

            if (end >= m_text.size())
            {
                m_failed = true;
                m_pos = m_text.size();
                m_current = { TokenType::End, {} };
                return;
            }

            m_pos = end + 1;
            m_current = { TokenType::String, m_text.substr(start + 1, end - start - 1) };
        }
        else if (IsPunct(c))
        {
            m_pos++;
            m_current = { TokenType::Punct, m_text.substr(start, 1) };
        }
        else
        {
            while (m_pos < m_text.size() && !IsSpace(m_text[m_pos]) && !IsPunct(m_text[m_pos]) && m_text[m_pos] != '"')
            {
                m_pos++;
            }

            m_current = { TokenType::Word, m_text.substr(start, m_pos - start) };
        }
    }

    std::string_view m_text;
    size_t m_pos = 0;
    Token m_current;
    bool m_failed = false;
};

template <typename T>
static bool ParseNumber(std::string_view str, T& result)
{
    // from_chars doesn't accept a leading '+'
    if (str.starts_with('+'))
    {
        str.remove_prefix(1);

        if (str.starts_with('-'))
        {
            return false;
        }
    }

    const char* end = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(str.data(), end, result);

    return ec == std::errc{} && ptr == end;
}

static std::string Unescape(std::string_view str)
{
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '\\' && i + 1 < str.size())
        {
            i++;
        }

        result.push_back(str[i]);
    }

    return result;
}

class Parser
{
public:
    Parser(std::string_view text)
        : m_tokens(text)
    {
    }

    std::unique_ptr<Dbc> Run(const std::function<void(const Signal&)>& onSignal)
    {
        while (!m_error)
        {
            const Token& token = m_tokens.Peek();

            if (token.Type == TokenType::End)
            {
                break;
            }

            if (token.Type == TokenType::Newline)
            {
                m_tokens.Next();
                continue;
            }

            // Signals must immediately follow their message
            if (!token.Is(TokenType::Word, "SG_"))
            {
                m_currentMessage = nullptr;
                m_skippingMessage = false;
            }

            if (token.Is(TokenType::Word, "NS_"))
            {
                ParseNewSymbols();
            }
            else if (token.Is(TokenType::Word, "BO_"))
            {
                ParseMessage();
            }
            else if (token.Is(TokenType::Word, "SG_"))
            {
                ParseSignal();
            }
            else if (token.Is(TokenType::Word, "SIG_VALTYPE_"))
            {
                ParseValueType();
            }
            else
            {
                // Something we don't care about
                SkipStatement();
            }
        }

        if (m_error || m_tokens.Failed() || !Validate())
        {
            return nullptr;
        }

        auto dbc = std::make_unique<Dbc>(m_signalCount, std::move(m_messages));

        if (onSignal)
        {
            std::vector<const Signal*> byId(dbc->SignalCount());

            for (const auto& [key, message] : dbc->Messages())
            {
                for (const auto& signal : message.Signals)
                {
                    byId[signal.Id] = &signal;
                }
            }

            for (const Signal* signal : byId)
            {
                onSignal(*signal);
            }
        }

        return dbc;
    }

private:
    // On a mismatch, these flag an error and return an empty value, so a statement can be parsed
    // to the end and the error checked once afterwards.
    void Fail()
    {
        m_error = true;
    }

    Token Expect(TokenType type)
    {
        if (m_tokens.Peek().Type != type)
        {
            Fail();
            return { type, {} };
        }

        return m_tokens.Next();
    }

    void ExpectPunct(std::string_view punct)
    {
        if (!m_tokens.Peek().Is(TokenType::Punct, punct))
        {
            Fail();
            return;
        }

        m_tokens.Next();
    }

    template <typename T>
    T ExpectNumber()
    {
        T result{};

        if (!ParseNumber(Expect(TokenType::Word).Text, result))
        {
            Fail();
        }

        return result;
    }

    void ExpectEndOfLine()
    {
        if (m_tokens.Peek().Type == TokenType::Newline)
        {
            m_tokens.Next();
        }
        else if (m_tokens.Peek().Type != TokenType::End)
        {
            Fail();
        }
    }

    void SkipStatement()
    {
        while (true)
        {
            Token token = m_tokens.Next();

            if (token.Type == TokenType::Newline || token.Type == TokenType::End || token.Is(TokenType::Punct, ";"))
            {
                return;
            }
        }
    }

    // NS_ : followed by a list of keywords used in the file, one per line
    void ParseNewSymbols()
    {
        m_tokens.Next();
        ExpectPunct(":");

        while (m_tokens.Peek().Type == TokenType::Word)
        {
            m_tokens.Next();
        }

        ExpectEndOfLine();

        // The list continues on every following line that's a lone word
        while (!m_error)
        {
            while (m_tokens.Peek().Type == TokenType::Newline)
            {
                m_tokens.Next();
            }

            if (m_tokens.Peek().Type != TokenType::Word)
            {
                return;
            }

            Tokenizer ahead = m_tokens;
            ahead.Next();

            if (ahead.Peek().Type != TokenType::Newline && ahead.Peek().Type != TokenType::End)
            {
                return;
            }

            m_tokens = ahead;
        }
    }

    // BO_ <id> <name> : <dlc> <transmitter>
    void ParseMessage()
    {
        m_tokens.Next();

        uint32_t rawId = ExpectNumber<uint32_t>();
        std::string_view name = Expect(TokenType::Word).Text;
        ExpectPunct(":");
        uint32_t dlc = ExpectNumber<uint32_t>();

        if (m_tokens.Peek().Type == TokenType::Word)
        {
            // Transmitter, ignored
            m_tokens.Next();
        }

        ExpectEndOfLine();

        if (m_error)
        {
            return;
        }

        if (rawId == IndependentSignalsMessageId)
        {
            // Parse the signals that follow, but don't keep them
            m_skippingMessage = true;
            return;
        }

        bool extended = rawId & Dbc::ExtendedIdFlag;
        uint32_t id = rawId & ~Dbc::ExtendedIdFlag;

        if (id > (extended ? 0x1FFFFFFFu : 0x7FFu) || dlc > 64)
        {
            Fail();
            return;
        }

        auto result = m_messages.try_emplace(rawId, id, extended, static_cast<uint8_t>(dlc), name);

        if (!result.second)
        {
            // Uh oh, duplicate message!
            Fail();
            return;
        }

        m_currentMessage = &result.first->second;
    }

    // SG_ <name> [M|mN] : <bitpos>|<length>@<1 = intel, 0 = motorola><+|-> (<factor>,<offset>) [<min>|<max>] "<unit>" <receivers>
    void ParseSignal()
    {
        m_tokens.Next();

        std::string_view name = Expect(TokenType::Word).Text;

        MuxRole mux = MuxRole::None;
        uint32_t muxValue = 0;

        if (m_tokens.Peek().Type == TokenType::Word)
        {
            std::string_view muxText = m_tokens.Next().Text;

            if (muxText == "M")
            {
                mux = MuxRole::Multiplexor;
            }
            else if (muxText.starts_with('m') && !muxText.ends_with('M') && ParseNumber(muxText.substr(1), muxValue))
            {
                mux = MuxRole::Multiplexed;
            }
            else
            {
                // Includes "mNM", extended multiplexing, which needs SG_MUL_VAL_ and isn't supported
                Fail();
            }
        }

        ExpectPunct(":");

        uint32_t bitpos = ExpectNumber<uint32_t>();
        ExpectPunct("|");
        uint32_t length = ExpectNumber<uint32_t>();
        ExpectPunct("@");

        std::string_view format = Expect(TokenType::Word).Text;
        if (format.size() != 2 || (format[0] != '0' && format[0] != '1') || (format[1] != '+' && format[1] != '-'))
        {
            Fail();
        }

        ExpectPunct("(");
        float factor = ExpectNumber<float>();
        ExpectPunct(",");
        float offset = ExpectNumber<float>();
        ExpectPunct(")");

        ExpectPunct("[");
        float min = ExpectNumber<float>();
        ExpectPunct("|");
        float max = ExpectNumber<float>();
        ExpectPunct("]");

        std::string_view unit = Expect(TokenType::String).Text;

        // Receivers are comma separated, but some tools separate them with spaces
        while (m_tokens.Peek().Type == TokenType::Word || m_tokens.Peek().Is(TokenType::Punct, ","))
        {
            m_tokens.Next();
        }

        ExpectEndOfLine();

        if (m_error || m_skippingMessage)
        {
            return;
        }

        if (!m_currentMessage)
        {
            // Signal with no message?
            Fail();
            return;
        }

        auto& signals = m_currentMessage->Signals;
        Endian endian = (format[0] == '1') ? Endian::Little_Intel : Endian::Big_Motorola;

        // Signal must fit in the message
        if (length < 1 || length > 64 || bitpos >= 64 * 8 ||
            Dbc::BytesRequired(endian, static_cast<uint16_t>(bitpos), static_cast<uint8_t>(length)) > m_currentMessage->Dlc)
        {
            Fail();
            return;
        }

        bool duplicate = std::any_of(signals.begin(), signals.end(), [&](const Signal& s)
        {
            return s.Name == name || (mux == MuxRole::Multiplexor && s.Mux == MuxRole::Multiplexor);
        });

        if (duplicate)
        {
            // Duplicate name, or a second multiplexor
            Fail();
            return;
        }

        signals.emplace_back(
            m_signalCount++,
            name,
            endian,
            format[1] == '-',
            static_cast<uint16_t>(bitpos),
            static_cast<uint8_t>(length),
            factor,
            offset,
            min,
            max,
            Unescape(unit),
            mux,
            muxValue
        );
    }

    // SIG_VALTYPE_ <message id> <signal name> : <0 = integer, 1 = float, 2 = double> ;
    void ParseValueType()
    {
        m_tokens.Next();

        uint32_t rawId = ExpectNumber<uint32_t>();
        std::string_view name = Expect(TokenType::Word).Text;
        ExpectPunct(":");
        uint32_t type = ExpectNumber<uint32_t>();
        ExpectPunct(";");

        if (m_error || rawId == IndependentSignalsMessageId)
        {
            return;
        }

        auto message = m_messages.find(rawId);
        if (message == m_messages.end())
        {
            Fail();
            return;
        }

        auto& signals = message->second.Signals;
        auto signal = std::find_if(signals.begin(), signals.end(), [&](const Signal& s) { return s.Name == name; });

        if (signal == signals.end())
        {
            Fail();
            return;
        }

        if (type == 0)
        {
            signal->Type = ValueType::Integer;
        }
        else if (type == 1 && signal->Length == 32)
        {
            signal->Type = ValueType::Float32;
        }
        else if (type == 2 && signal->Length == 64)
        {
            signal->Type = ValueType::Float64;
        }
        else
        {
            Fail();
        }
    }

    // Checks that can only be made once the whole file is parsed
    bool Validate() const
    {
        for (const auto& [key, message] : m_messages)
        {
            auto has = [&](MuxRole role)
            {
                return std::any_of(message.Signals.begin(), message.Signals.end(), [&](const Signal& s) { return s.Mux == role; });
            };

            if (has(MuxRole::Multiplexed) && !has(MuxRole::Multiplexor))
            {
                return false;
            }
        }

        return true;
    }

    Tokenizer m_tokens;
    bool m_error = false;

    Dbc::message_container m_messages;
    Message* m_currentMessage = nullptr;
    // Set while parsing the signals of VECTOR__INDEPENDENT_SIG_MSG
    bool m_skippingMessage = false;
    size_t m_signalCount = 0;
};

} // namespace

/*static*/ std::unique_ptr<Dbc> Dbc::Parse(std::istream& file, std::function<void(const Signal&)> onSignal)
{
    if (!file)
    {
        // Probably failed to open
        return nullptr;
    }

    std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };

    if (file.bad())
    {
        return nullptr;
    }

    return Parser(text).Run(onSignal);
}
