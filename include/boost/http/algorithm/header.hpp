/* Copyright (c) 2014 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#ifndef BOOST_HTTP_ALGORITHM_HEADER_HPP
#define BOOST_HTTP_ALGORITHM_HEADER_HPP

#include <cctype>

#include <algorithm>
#include <regex>
#include <type_traits>

#include <boost/utility/string_ref.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace boost {
namespace http {

namespace detail {

/* This function was created, because most int <-> string conversion
 * techniques/abstractions can only handle char* or basic_string. They don't
 * support generic strings.
 *
 * Despite this being the critical point to motivate this development, there are
 * other disappointing behaviours with the found abstractions. For instance, I
 * invested a big effort to allocate as little memory on the heap as I could,
 * but most of the abstractions will just use something like standard library
 * string-based stream or other expensive abstractions.
 *
 * However, I'm not advocating the interface developed here is better. This
 * interface is limited to non-negative base-10 conversions and don't properly
 * handle Unicode characters. Even worse, it doesn't check for invalid
 * input.
 *
 * Don't freak out about the lack of valid inputs. This is checked by the
 * regex layer.
 *
 * It's just a workaround to not throw away all the efforts spent in the other
 * layers.*/
template<class Target, class BidirIt>
Target from_decimal_string(BidirIt begin, BidirIt end)
{
    static_assert(std::is_same<
                    typename std::iterator_traits<BidirIt>::value_type,
                    char>::value,
                  "from_decimal_string only supports char type");
    Target ret = 0, digit = 1;
    std::reverse_iterator<BidirIt> it(end), rend(begin);
    for (;it != rend;++it) {
        ret += (*it - '0') * digit;
        digit *= 10;
    }
    return ret;
}

template<class Target, class BidirIt>
Target from_decimal_submatch(const std::sub_match<BidirIt> &match)
{
    return from_decimal_string<Target>(match.first, match.second);
}

template<class Target, class BidirIt>
Target from_submatch_to_month(std::sub_match<BidirIt> m)
{
    switch (*m.first) {
    case 'A': // "Apr" or "Aug"
        ++m.first;
        return (*m.first == 'p') ? 4 : 8;
    case 'D': // "Dec"
        return 12;
    case 'F': // "Feb"
        return 2;
    case 'J': // "Jan" or "Jun" or "Jul"
        ++m.first;
        if (*m.first == 'a')
            return 1;
        ++m.first;
        return (*m.first == 'n') ? 6 : 7;
    case 'M': // "Mar" or "May"
        ++m.first; ++m.first;
        return (*m.first == 'r') ? 3 : 5;
    case 'N': // "Nov"
        return 11;
    case 'O': // "Oct"
        return 10;
    case 'S': // "Sep"
        return 9;
    default:
        assert(false);
    }
}

template<class String>
bool rfc1123(const String &value, posix_time::ptime &datetime)
{
    using namespace gregorian;
    using namespace posix_time;

    typedef date::year_type::value_type year_type;
    typedef date::month_type::value_type month_type;
    typedef date::day_type::value_type day_type;
    typedef time_duration::hour_type hour_type;
    typedef time_duration::min_type min_type;
    typedef time_duration::sec_type sec_type;

    static const std::basic_regex<typename String::value_type>
        regex("(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun), " // day
              "(\\d{2}) " // day-1
              "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) " // month-2
              "(\\d{4}) " // year-3
              "(\\d{2}):" // hour-4
              "(\\d{2}):" // minutes-5
              "(\\d{2}) " // seconds-6
              "GMT");

    std::match_results<typename String::const_iterator> matches;
    if (!std::regex_match(value.begin(), value.end(), matches, regex))
        return false;

    hour_type hour = from_decimal_submatch<hour_type>(matches[4]);
    min_type min = from_decimal_submatch<min_type>(matches[5]);
    sec_type sec = from_decimal_submatch<sec_type>(matches[6]);

    if (hour > 23 || min > 59 || sec > 60)
        return false;

    try {
        datetime = ptime(date(from_decimal_submatch<year_type>(matches[3]),
                              from_submatch_to_month<month_type>(matches[2]),
                              from_decimal_submatch<day_type>(matches[1])),
                         time_duration(hour, min, sec));
    } catch(std::out_of_range&) {
        return false;
    }

    return true;
}

template<class String>
bool rfc1036(const String &value, posix_time::ptime &datetime)
{
    using namespace gregorian;
    using namespace posix_time;

    typedef date::year_type::value_type year_type;
    typedef date::month_type::value_type month_type;
    typedef date::day_type::value_type day_type;
    typedef time_duration::hour_type hour_type;
    typedef time_duration::min_type min_type;
    typedef time_duration::sec_type sec_type;

    static const std::basic_regex<typename String::value_type>
        regex("(?:Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday), " // day
              "(\\d{2})-" // day-1
              "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)-" // month-2
              "(\\d{2}) " // year-3
              "(\\d{2}):" // hour-4
              "(\\d{2}):" // minutes-5
              "(\\d{2}) " // seconds-6
              "GMT");

    std::match_results<typename String::const_iterator> matches;
    if (!std::regex_match(value.begin(), value.end(), matches, regex))
        return false;

    hour_type hour = from_decimal_submatch<hour_type>(matches[4]);
    min_type min = from_decimal_submatch<min_type>(matches[5]);
    sec_type sec = from_decimal_submatch<sec_type>(matches[6]);

    if (hour > 23 || min > 59 || sec > 60)
        return false;

    try {
        datetime = ptime(date(from_decimal_submatch<year_type>(matches[3])
                              + 1900,
                              from_submatch_to_month<month_type>(matches[2]),
                              from_decimal_submatch<day_type>(matches[1])),
                         time_duration(hour, min, sec));
    } catch(std::out_of_range&) {
        return false;
    }

    return true;
}

template<class String>
bool asctime(const String &value, posix_time::ptime &datetime)
{
    using namespace gregorian;
    using namespace posix_time;

    typedef date::year_type::value_type year_type;
    typedef date::month_type::value_type month_type;
    typedef date::day_type::value_type day_type;
    typedef time_duration::hour_type hour_type;
    typedef time_duration::min_type min_type;
    typedef time_duration::sec_type sec_type;

    static const std::basic_regex<typename String::value_type>
        regex("(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun) " // day
              "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) " // month-1
              "((?:\\d| )\\d) " // day-2
              "(\\d{2}):" // hour-3
              "(\\d{2}):" // minutes-4
              "(\\d{2}) " // seconds-5
              "(\\d{4})" // year-6
              );

    std::match_results<typename String::const_iterator> matches;
    if (!std::regex_match(value.begin(), value.end(), matches, regex))
        return false;

    hour_type hour = from_decimal_submatch<hour_type>(matches[3]);
    min_type min = from_decimal_submatch<min_type>(matches[4]);
    sec_type sec = from_decimal_submatch<sec_type>(matches[5]);

    if (hour > 23 || min > 59 || sec > 60)
        return false;

    try {
        datetime = ptime(date(from_decimal_submatch<year_type>(matches[6]),
                              from_submatch_to_month<month_type>(matches[1]),
                              [&matches]() {
                                  auto m = matches[2];
                                  if (*m.first == ' ')
                                      ++m.first;
                                  return from_decimal_submatch<day_type>(m);
                              }()),
                         time_duration(hour, min, sec));
    } catch(std::out_of_range&) {
        return false;
    }

    return true;
}

} // namespace detail

/**
 * Use is_not_a_date_time() to check if the conversion failed.
 */
template<class String>
posix_time::ptime header_to_ptime(const String &value)
{
    using namespace gregorian;
    using namespace posix_time;
    ptime ret(date_time::not_a_date_time);
    if (!detail::rfc1123(value, ret)) {
        if (!detail::rfc1036(value, ret))
            detail::asctime(value, ret);
    }
    return ret;
}

/**
 * \p's signature MUST be:
 * bool(boost::string_ref value);
 */
template<class String, class Predicate>
bool header_value_any_of(const String &header_value, const Predicate &p)
{
    typedef typename String::value_type char_type;
    typedef typename String::const_reverse_iterator reverse_iterator;

    auto comma = header_value.begin();
    decltype(comma) next_comma;
    do {
        next_comma = std::find(comma, header_value.end(), ',');

        auto value_begin = std::find_if_not(comma, next_comma,
                                            [](const char_type &c) {
                                                return std::isspace(c);
                                            });

        if (value_begin != next_comma) {
            auto value_end = std::find_if_not(reverse_iterator(next_comma),
                                              reverse_iterator(value_begin),
                                              [](const char_type &c){
                                                  return std::isspace(c);
                                              }).base();
            if (value_begin != value_end
                && p(string_ref(&*value_begin, value_end - value_begin)))
                return true;
        }

        comma = next_comma;

        /* skip comma, so won't process an empty string in the next iteration
           and enter within an infinite loop afterwards. */
        if (next_comma != header_value.end())
            ++comma;
    } while (comma != header_value.end());
    return false;
}

} // namespace http
} // namespace boost

#endif // BOOST_HTTP_ALGORITHM_HEADER_HPP
