#ifdef __STDC_ALLOC_LIB__
#  define __STDC_WANT_LIB_EXT2__ 1
#endif

#include <vector>
#include <memory>
#include <string_view>
#include <charconv>
#include <chrono>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{

enum class Kind : uint8_t
{
    Seconds,
    Milliseconds,
    Microseconds,
    Clock,
    String,
    CharFormat,
    Input,
};

struct PartFormat
{
    Kind kind;
    unsigned len;
    char const* str;
};

struct ParseFormatResult
{
    std::vector<PartFormat> parts;
    std::unique_ptr<char[]> pad;
    char const* format_error = nullptr;
};

ParseFormatResult parse_format(char const* format, char const* endformat)
{
    assert(format);
    assert(*endformat == '\0');

    ParseFormatResult result;
    auto& parts = result.parts;
    unsigned max_len_zero = 0;
    unsigned max_len_space = 0;
    char const* s;

    parts.reserve(4);

    while ((s = std::strchr(format, '%')))
    {
        if (s != format)
        {
            parts.emplace_back(PartFormat{Kind::String, unsigned(s - format), format});
        }

        PartFormat part {Kind(), 0, ""};

        auto parse = [&]() -> std::ptrdiff_t {
            switch (s[1])
            {
                case 'i': part.kind = Kind::Input; return 2;
                case 't': part.kind = Kind::Clock; return 2;
                case 's': part.kind = Kind::Seconds; return 2;
                case 'm': part.kind = Kind::Milliseconds; return 2;
                case 'u': part.kind = Kind::Microseconds; return 2;
                case '%':
                    if (s != format)
                    {
                        ++parts.back().len;
                        return -1;
                    }
                    else
                    {
                        part.kind = Kind::CharFormat;
                        return 2;
                    }
            }

            unsigned min_len = 0;
            char pad = ' ';

            char const* s2 = s;

            ++s2;

            if (*s2 == '0')
            {
                pad = '0';
                ++s2;
            }

            auto r = std::from_chars(s2, endformat, min_len);
            if (r.ec != std::errc())
            {
                return 0;
            }

            if (pad == ' ')
            {
                max_len_space = std::max(max_len_space, min_len);
            }
            else
            {
                max_len_zero = std::max(max_len_zero, min_len);
            }

            char const* string = (pad == ' ') ? " " : "0";

            switch (*r.ptr)
            {
                case 's':
                    part = {Kind::Seconds, min_len, string};
                    return r.ptr - s + 1;

                case 'm':
                    part = {Kind::Milliseconds, min_len, string};
                    return r.ptr - s + 1;

                case 'u':
                    part = {Kind::Microseconds, min_len, string};
                    return r.ptr - s + 1;
            }

            return 0;
        };

        auto r = parse();
        if (r > 0)
        {
            parts.emplace_back(part);
            format = s + r;
        }
        // is %%
        else if (r == -1)
        {
            format = s + 2;
        }
        else
        {
            result.format_error = s;
            return result;
        }
    }

    if (format != endformat)
    {
        parts.emplace_back(PartFormat{Kind::String, unsigned(endformat - format), format});
    }

    auto pad_len = max_len_space + max_len_zero;
    if (pad_len)
    {
        auto* p = new char[pad_len];
        result.pad.reset(p);
        memset(p, ' ', max_len_space);
        memset(p + max_len_space, '0', max_len_zero);

        for (auto& part : result.parts)
        {
            if (unsigned(part.kind) <= unsigned(Kind::Microseconds))
            {
                part.str = p + (*part.str == '0' ? max_len_space : 0);
            }
        }
    }

    return result;
}

using Clock = std::conditional_t<
    std::chrono::high_resolution_clock::is_steady,
    std::chrono::high_resolution_clock,
    std::chrono::steady_clock>;
static_assert(Clock::is_steady);

static constexpr char digit10_pairs[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6',
    '0', '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3',
    '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0',
    '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7',
    '2', '8', '2', '9', '3', '0', '3', '1', '3', '2', '3', '3', '3', '4',
    '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', '4', '0', '4', '1',
    '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8',
    '4', '9', '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5',
    '5', '6', '5', '7', '5', '8', '5', '9', '6', '0', '6', '1', '6', '2',
    '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
    '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6',
    '7', '7', '7', '8', '7', '9', '8', '0', '8', '1', '8', '2', '8', '3',
    '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9', '9', '0',
    '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7',
    '9', '8', '9', '9',
};

char* push_2digits(char* s, Clock::duration::rep n)
{
    assert(n < 100);
    *s++ = digit10_pairs[n*2];
    *s++ = digit10_pairs[n*2+1];
    return s;
}

char* push_3digits(char* s, Clock::duration::rep n)
{
    assert(n < 1000);
    *s++ = char(n / 100 + '0');
    return push_2digits(s, n % 100);
}

bool write_time_impl(Clock::duration::rep d1, Clock::duration::rep d2, PartFormat duration_format)
{
    char output[64];
    char* s = output;
    s = std::to_chars(s, std::end(output), uint64_t(d1)).ptr;
    auto num_len = std::size_t(s - output);

    *s++ = '.';
    s = push_3digits(s, d2);

    auto len = std::size_t(s - output);
    if (duration_format.len > num_len)
    {
        auto len2 = duration_format.len - num_len;
        return fwrite(duration_format.str, 1, len2, stdout) == len2
            && fwrite(output, 1, len, stdout) == len;
    }
    return fwrite(output, 1, len, stdout) == len;
}

template<class OutputDuration>
bool write_time(Clock::duration duration, PartFormat duration_format)
{
    using Ratio = typename OutputDuration::period;
    using Ratio2 = std::ratio<Ratio::num, Ratio::den * 1000>;
    using OutputDuration2 = std::chrono::duration<typename OutputDuration::rep, Ratio2>;

    auto d1 = std::chrono::duration_cast<OutputDuration>(duration);
    auto d2 = std::chrono::duration_cast<OutputDuration2>(duration - d1);

    return write_time_impl(d1.count(), d2.count(), duration_format);
}

bool write_format(Clock::duration duration, PartFormat part, char const* input, std::size_t len)
{
    switch (part.kind)
    {
        case Kind::Seconds:
            return write_time<std::chrono::seconds>(duration, part);
        case Kind::Milliseconds:
            return write_time<std::chrono::milliseconds>(duration, part);
        case Kind::Microseconds:
            return write_time<std::chrono::microseconds>(duration, part);

        case Kind::Clock: {
            auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
            duration -= hours;
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
            duration -= minutes;
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
            duration -= seconds;
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

            char output[128];
            char* s = output;
            if (hours.count() <= 99)
            {
                s = push_2digits(s, hours.count());
            }
            else
            {
                s = std::to_chars(s, std::end(output), hours.count()).ptr;
            }
            *s++ = ':';
            s = push_2digits(s, minutes.count());
            *s++ = ':';
            s = push_2digits(s, seconds.count());
            *s++ = '.';
            s = push_3digits(s, milliseconds.count());

            auto len2 = std::size_t(s-output);
            return fwrite(output, 1, len2, stdout) == len2;
        }

        case Kind::String:
            return fwrite(part.str, 1, part.len, stdout) == part.len;

        case Kind::CharFormat:
            return fputc('%', stdout) != EOF;

        case Kind::Input:
            return fwrite(input, 1, len, stdout) == len;
    }

#if defined(__GNUC__)
    __builtin_unreachable();
#else
    return false;
#endif
}

void timedline(std::vector<PartFormat> const& parts)
{
    const bool last_is_not_input = (parts.back().kind != Kind::Input);

    char *input = nullptr;
    size_t len;
    ssize_t slen;

    auto t = Clock::now();

    while ((slen = getline(&input, &len, stdin)) > 0)
    {
        auto t2 = Clock::now();

        std::size_t trailing = 0;
        if (last_is_not_input && input[slen-1] == '\n')
        {
            trailing = 1;
        }

        for (auto const& part : parts)
        {
            if (!write_format(t2 - t, part, input, std::size_t(slen) - trailing))
            {
                goto end_main;
            }
        }

        if (trailing && fputc('\n', stdout) == EOF)
        {
            goto end_main;
        }

        t = t2;
    }

    end_main:
    std::free(input);
}

void usage(char const* progname, FILE* file)
{
    std::fprintf(file, "Usage: %s format\n\n%s", progname,
        " %%: an % character\n"
        " %i: input text\n"
        " %t: time in hh:mm:ss.ms format\n"
        " %s: time in seconds\n"
        " %m: time in milliseconds\n"
        " %u: time in microseconds\n"
        " %nF: with\n"
        "   n: An integer corresponding to the minimum size of the displayed number."
        " Spaces (or zeros if the number begins with 0) are placed to the left.\n"
        "   F: s, m or u formats\n"
    );
}

} // anonymous namespace

int main(int ac, char ** av)
{
    char const* progname = av[0];

    if (ac != 2)
    {
        usage(progname, stderr);
        return 1;
    }

    std::string_view format = av[1];

    if (format == "-h" || format == "-?" || format == "--help")
    {
        usage(progname, stdout);
        return 0;
    }

    auto result = parse_format(format.begin(), format.end());

    if (result.format_error)
    {
        std::fprintf(stderr, "%s: Invalid format at position %u: %s\n",
            progname, unsigned(result.format_error - format.begin()), result.format_error);
        return 2;
    }

    if (result.parts.empty())
    {
        std::fprintf(stderr, "%s: empty format\n", progname);
        return 2;
    }

    timedline(result.parts);

    return 0;
}
