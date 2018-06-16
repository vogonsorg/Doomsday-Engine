/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/App"
#include "de/String"
#include "de/Block"
#include "de/RegExp"
#include "de/Path"
#include "de/charsymbols.h"

#include <c_plus/path.h>
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <cwchar>

namespace de {

const String::size_type String::npos = String::size_type(-1);

String::String()
{
    init_String(&_str);
}

String::String(const String &other)
{
    initCopy_String(&_str, &other._str);
}

String::String(String &&moved)
{
    _str = moved._str;
    iZap(moved._str);
}

String::String(const Block &bytes)
{
    initCopy_Block(&_str.chars, bytes);
}

String::String(const iBlock *bytes)
{
    initCopy_Block(&_str.chars, bytes);
}

String::String(const iString *other)
{
    initCopy_String(&_str, other);
}

String::String(const std::string &text)
{
    initCStrN_String(&_str, text.data(), text.size());
}

String::String(const std::wstring &text)
{
    init_String(&_str);
    for (auto ch : text)
    {
        iMultibyteChar mb;
        init_MultibyteChar(&mb, ch);
        appendCStr_String(&_str, mb.bytes);
    }
}

String::String(const char *nullTerminatedCStr)
{
    initCStr_String(&_str, nullTerminatedCStr);
}

String::String(const Char *nullTerminatedWideStr)
{
    initWide_String(&_str, nullTerminatedWideStr);
}

String::String(char const *cStr, int length)
{
    initCStrN_String(&_str, cStr, length);
}

String::String(char const *cStr, dsize length)
{
    initCStrN_String(&_str, cStr, length);
}

String::String(dsize length, char ch)
{
    init_Block(&_str.chars, length);
    fill_Block(&_str.chars, ch);
}

String::String(const char *start, const char *end)
{
    initCStrN_String(&_str, start, end - start);
}

String::String(const Range<const char *> &range)
{
    initCStrN_String(&_str, range.start, range.end - range.start);
}

String::String(const CString &cstr)
{
    initCStrN_String(&_str, cstr.begin(), cstr.size());
}

String::String(dsize length, iChar ch)
{
    init_String(&_str);

    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    if (strlen(mb.bytes) == 1)
    {
        resize_Block(&_str.chars, length);
        fill_Block(&_str.chars, mb.bytes[0]);
    }
    else
    {
        while (length-- > 0)
        {
            appendCStr_String(&_str, mb.bytes);
        }
    }
}

String::~String()
{
    // The string may have been deinitialized already by a move.
    if (_str.chars.i)
    {
        deinit_String(&_str);
    }
}

void String::resize(size_t newSize)
{
    resize_Block(&_str.chars, newSize);
}

std::wstring String::toWideString() const
{
    std::wstring ws;
    for (auto c : *this)
    {
        ws.push_back(c);
    }
    return ws;
}

CString String::toCString() const
{
    return {begin(), end()};
}

void String::clear()
{
    clear_String(&_str);
}

bool String::contains(char c) const
{
    for (const char *i = constBegin_String(&_str), *end = constEnd_String(&_str); i != end; ++i)
    {
        if (*i == c) return true;
    }
    return false;
}

bool String::contains(const char *cStr) const
{
    return indexOfCStr_String(&_str, cStr) != iInvalidPos;
}

int String::count(char ch) const
{
    int num = 0;
    for (const char *i = constBegin_String(&_str), *end = constEnd_String(&_str); i != end; ++i)
    {
        if (*i == ch) ++num;
    }
    return num;
}

bool String::beginsWith(Char ch, Sensitivity cs) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    return beginsWith(mb.bytes, cs);
}

String String::substr(CharPos pos, dsize count) const
{
    return String::take(mid_String(&_str, pos.index, count));
}

String String::substr(BytePos pos, dsize count) const
{
    iBlock *sub = mid_Block(&_str.chars, pos.index, count);
    String s(sub);
    delete_Block(sub);
    return s;
}

String String::substr(const Range<CharPos> &range) const
{
    return substr(range.start, range.size().index);
}

String String::substr(const Range<BytePos> &range) const
{
    return substr(range.start, range.size().index);
}

String String::right(CharPos count) const
{
    if (count.index == 0) return {};
    auto i = rbegin();
    for (auto end = rend(); --count.index > 0 && i != end; ++i) {}
    return String(static_cast<const char *>(i), data() + size());
}

void String::remove(BytePos start, dsize count)
{
    remove_Block(&_str.chars, start.index, count);
}

void String::truncate(String::BytePos pos)
{
    truncate_Block(&_str.chars, pos.index);
}

List<String> String::split(const char *separator) const
{
    List<String> parts;
    iRangecc seg{};
    iRangecc str{constBegin_String(&_str), constEnd_String(&_str)};
    while (nextSplit_Rangecc(&str, separator, &seg))
    {
        parts << String(seg.start, seg.end);
    }
    return parts;
}

List<CString> String::splitRef(const char *separator) const
{
    List<CString> parts;
    iRangecc seg{};
    iRangecc str{constBegin_String(&_str), constEnd_String(&_str)};
    while (nextSplit_Rangecc(&str, separator, &seg))
    {
        parts << CString(seg.start, seg.end);
    }
    return parts;
}

List<CString> String::splitRef(Char ch) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    return splitRef(mb.bytes);
}

List<String> String::split(Char ch) const
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, ch);
    return split(mb.bytes);
}

List<String> String::split(const RegExp &regExp) const
{
    List<String> parts;
    const char *pos = constBegin_String(&_str);
    for (RegExpMatch m; regExp.match(*this, m); pos = m.end())
    {
        // The part before the matched separator.
        parts << String(pos, m.begin());
    }
    // The final part.
    parts << String(pos, constEnd_String(&_str));
    return parts;
}

String String::operator+(const std::string &s) const
{
    return *this + CString(s);
}

String String::operator+(const CString &cStr) const
{
    String cat = *this;
    appendCStrN_String(&cat._str, cStr.begin(), cStr.size());
    return cat;
}

String String::operator+(const char *cStr) const
    {
    String cat(*this);
    appendCStr_String(&cat._str, cStr);
    return cat;
    }

String &String::operator+=(char ch)
{
    appendData_Block(&_str.chars, &ch, 1);
    return *this;
}

String &String::operator+=(iChar ch)
{
    appendChar_String(&_str, ch);
    return*this;
}

String &String::operator+=(const char *cStr)
    {
    appendCStr_String(&_str, cStr);
    return *this;
    }

String &String::operator+=(const CString &s)
{
    appendCStrN_String(&_str, s.begin(), s.size());
    return *this;
}

String &String::operator+=(const String &other)
{
    append_String(&_str, &other._str);
    return *this;
}

void String::insert(BytePos pos, const char *cStr)
{
    insertData_Block(&_str.chars, pos.index, cStr, cStr ? strlen(cStr) : 0);
}

void String::insert(BytePos pos, const String &str)
{
    insertData_Block(&_str.chars, pos.index, str.data(), str.size());
}

String &String::replace(Char before, Char after)
{
    iMultibyteChar mb1, mb2;
    init_MultibyteChar(&mb1, before);
    init_MultibyteChar(&mb2, after);
    return replace(mb1.bytes, mb2.bytes);
}

String &String::replace(const CString &before, const CString &after)
{
    const String oldTerm{before};
    const iRangecc newTerm = after;
    iRangecc remaining(*this);
    String result;
    dsize found;
    while ((found = CString(remaining).indexOf(oldTerm)) != npos)
    {
        const iRangecc prefix{remaining.start, remaining.start + found};
        appendRange_String(result, &prefix);
        appendRange_String(result, &newTerm);
        remaining.start += found + before.size();
    }
    if (size_Range(&remaining) == size())
    {
        // No changes were applied.
        return *this;
    }
    appendRange_String(result, &remaining);
    return *this = result;
}

String &String::replace(const RegExp &before, const CString &after)
{
    const iRangecc newTerm = after;
    iRangecc remaining(*this);
    String result;
    RegExpMatch found;
    while (before.match(*this, found))
    {
        const iRangecc prefix{remaining.start, found.begin()};
        appendRange_String(result, &prefix);
        appendRange_String(result, &newTerm);
        remaining.start = found.end();
    }
    appendRange_String(result, &remaining);
    return *this = result;
}

iChar String::first() const
{
    return empty() ? 0 : *begin();
}

iChar String::last() const
{
    return empty() ? 0 : *rbegin();
}

String String::operator/(const String &path) const
{
    return concatenatePath(path);
}

String String::operator/(const CString &path) const
{
    return concatenatePath(String(path));
}

String String::operator/(const char *path) const
{
    return concatenatePath(String(path));
}

String String::operator/(const Path &path) const
{
    return concatenatePath(path.toString());
}

String String::operator%(const PatternArgs &args) const
{
    String result;
//    QTextStream output(&result);

    PatternArgs::const_iterator arg = args.begin();

    for (auto i = begin(); i != end(); ++i)
    {
        if (*i == '%')
        {
            String::const_iterator next = i;
            advanceFormat(next, end());
            if (*next == '%')
            {
                // Escaped.
                result += *next;
                ++i;
                continue;
            }

            if (arg == args.end())
            {
                // Out of args.
                throw IllegalPatternError("String::operator%", "Ran out of arguments");
            }

            result += patternFormat(i, end(), **arg);
            ++arg;
        }
        else
        {
            result += *i;
        }
    }

    // Just append the rest of the arguments without special instructions.
    for (; arg != args.end(); ++arg)
    {
        result += (*arg)->asText();
    }

    return result;
}

String String::concatenatePath(const String &other, iChar dirChar) const
{
    if ((dirChar == '/' || dirChar == '\\') && isAbsolute_Path(&other._str))
    {
        // The other path is absolute - use as is.
        return other;
    }
    return concatenateRelativePath(other, dirChar);
}

String String::concatenateRelativePath(const String &other, Char dirChar) const
{
    if (other.isEmpty()) return *this;

    const auto startPos = CharPos(other.first() == dirChar? 1 : 0);

    // Do a path combination. Check for a slash.
    String result(*this);
    if (!empty() && last() != dirChar)
    {
        result += dirChar;
    }
    result += other.substr(startPos);
    return result;
}

String String::concatenateMember(const String &member) const
{
    if (member.isEmpty()) return *this;
    if (member.first() == '.')
    {
        throw InvalidMemberError("String::concatenateMember", "Invalid: '" + member + "'");
    }
    return concatenatePath(member, '.');
}

String String::strip() const
{
    String ts(*this);
    trim_String(&ts._str);
    return ts;
}

String String::leftStrip() const
{
    String ts(*this);
    trimStart_String(&ts._str);
    return ts;
    }

String String::rightStrip() const
{
    String ts(*this);
    trimEnd_String(&ts._str);
    return ts;
}

String String::normalizeWhitespace() const
{
    static const RegExp reg("\\s+");
    String s = *this;
    s.replace(reg, " ");
    return s.strip();
}

String String::removed(const RegExp &expr) const
{
    String s = *this;
    s.replace(expr, "");
    return s;
}

String String::lower() const
{
    return String::take(lower_String(&_str));
}

String String::upper() const
{
    return String::take(upper_String(&_str));
}

String String::upperFirstChar() const
{
    if (isEmpty()) return "";
    const_iterator i = begin();
    String capitalized(1, Char(towupper(*i++)));
    appendCStr_String(&capitalized._str, i);
    return capitalized;
}

CString String::fileName(Char dirChar) const
{
    if (auto bytePos = lastIndexOf(dirChar))
    {
        return {data() + bytePos.index + 1, data() + size()};
    }
    return *this;
}

CString String::fileNameWithoutExtension() const
{
    const CString name = fileName();
    if (auto dotPos = lastIndexOf('.'))
    {
        if (dotPos.index > dsize(name.begin() - data()))
        {
            return {name.begin(), data() + dotPos.index};
        }
    }
    return name;
}

CString String::fileNameExtension() const
{
    auto pos      = lastIndexOf('.');
    auto slashPos = lastIndexOf('/');
    if (pos > 0)
    {
        // If there is a directory included, make sure there it at least
        // one character's worth of file name before the period.
        if (!slashPos || pos > slashPos + 1)
        {
            return {data() + pos.index, data() + size()};
        }
    }
    return "";
}

CString String::fileNamePath(Char dirChar) const
{
    if (auto pos = lastIndexOf(dirChar))
    {
        //return left(pos);
        return {data(), data() + pos.index};
    }
    return "";
}

String String::fileNameAndPathWithoutExtension(Char dirChar) const
{
    return String(fileNamePath(dirChar)) / fileNameWithoutExtension();
}

bool String::containsWord(const String &word) const
{
    if (word.isEmpty())
    {
        return false;
    }
    return RegExp(stringf("\\b%s\\b", word.c_str())).hasMatch(*this);
}

int String::compare(const CString &str, Sensitivity cs) const
{
    const iRangecc s = *this;
    return cmpCStrNSc_Rangecc(&s, str.begin(), str.size(), cs);
}

dint String::compareWithCase(const String &other) const
{
    return cmpSc_String(&_str, other, &iCaseSensitive);
}

dint String::compareWithoutCase(const String &other) const
{
    return cmpSc_String(&_str, other, &iCaseInsensitive);
}

dint String::compareWithoutCase(const String &other, int n) const
{
    return cmpNSc_String(&_str, other, n, &iCaseInsensitive);
}

int String::commonPrefixLength(const String &str, Sensitivity sensitivity) const
{
    int count = 0;
    for (const_iterator a = begin(), b = str.begin(), aEnd = end(), bEnd = str.end();
         a != aEnd && b != bEnd; ++a, ++b, ++count)
    {
        if (sensitivity == CaseSensitive)
        {
            if (*a != *b) break;
        }
        else
        {
            if (towlower(*a) != towlower(*b)) break;
        }
    }
    return count;
}

String::const_iterator String::begin() const
{
    const_iterator i;
    init_StringConstIterator(&i.iter, &_str);
    return i;
}

String::const_iterator String::end() const
{
    const_iterator i;
    init_StringConstIterator(&i.iter, &_str);
    i.iter.pos = i.iter.next = constEnd_String(&_str);
    i.iter.remaining = 0;
    return i;
}

String::const_reverse_iterator String::rbegin() const
    {
    const_reverse_iterator i;
    init_StringReverseConstIterator(&i.iter, &_str);
    return i;
    }

String::const_reverse_iterator String::rend() const
{
    const_reverse_iterator i;
    init_StringReverseConstIterator(&i.iter, &_str);
    i.iter.pos = i.iter.next = constBegin_String(&_str);
    i.iter.remaining = 0;
    return i;
}

String String::take(iString *str) // static
{
    String s(str);
    delete_String(str);
    return s;
}

void String::skipSpace(const_iterator &i, const const_iterator &end)
{
    while (i != end && iswspace(*i)) ++i;
}

String String::format(const char *format, ...)
{
    va_list args;
    Block buffer;

    for (;;)
    {
        va_start(args, format);
        int count =
            vsnprintf(buffer.size() == 0 ? nullptr : reinterpret_cast<char *>(buffer.data()),
                      buffer.size(),
                      format,
                      args);
        va_end(args);

        if (count < 0)
        {
            warning("[String::format] Error: %s", errno, strerror(errno));
            return buffer;
        }
        if (dsize(count) < buffer.size())
        {
            buffer.resize(count);
            break;
        }
        buffer.resize(count + 1);
    }
    return buffer;
}

dint String::toInt(bool *ok, int base, duint flags) const
{
    char *endp;
    auto value = std::strtol(*this, &endp, base);
    if (ok) *ok = (errno != ERANGE);
    if (!(flags & AllowSuffix) && !(*endp == 0 || isspace(*endp)))
    {
        // Suffix not allowed; consider this a failure.
        if (ok) *ok = false;
        value = 0;
        }
    return dint(value);
}

duint32 String::toUInt32(bool *ok, int base) const
{
    const auto value = std::strtoul(*this, nullptr, base);
    if (ok) *ok = (errno != ERANGE);
    return duint32(value);
}

long String::toLong(bool *ok, int base) const
{
    long value = std::strtol(*this, nullptr, base);
    if (ok) *ok = (errno != ERANGE);
    return value;
}

dfloat String::toFloat() const
{
    return std::strtof(*this, nullptr);
}

ddouble String::toDouble() const
{
    return std::strtod(*this, nullptr);
}

String String::addLinePrefix(const String &prefix) const
{
    String result;
    iRangecc str{constBegin_String(&_str), constEnd_String(&_str)};
    iRangecc range{};
    while (nextSplit_Rangecc(&str, "\n", &range))
    {
        if (!result.empty()) result += '\n';
        result += prefix;
        result += String(range.start, range.end);
    }
    return result;
}

String String::escaped() const
{
    String esc = *this;
    esc.replace("\\", "\\\\")
       .replace("\"", "\\\"")
       .replace("\b", "\\b")
       .replace("\f", "\\f")
       .replace("\n", "\\n")
       .replace("\r", "\\r")
       .replace("\t", "\\t");
    return esc;
}

void String::get(Offset at, Byte *values, Size count) const
{
    if (at + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("String::get", "Out of range " +
                          String::format("(%zu[+%zu] > %zu)", at, count, size()));
    }
    std::memcpy(values, constBegin_String(&_str) + at, count);
}

void String::set(Offset at, const Byte *values, Size count)
{
    if (at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("String::set", "Out of range");
    }
    setSubData_Block(&_str.chars, at, values, count);
}

String String::truncateWithEllipsis(dsize maxLength) const
{
    if (size() <= maxLength)
    {
        return *this;
    }
    return left(CharPos(maxLength/2 - 1)) + "..." + right(CharPos(maxLength/2 - 1));
}

void String::advanceFormat(const_iterator &i, const const_iterator &end)
{
    ++i;
    if (i == end)
    {
        throw IllegalPatternError("String::advanceFormat",
                                  "Incomplete formatting instructions");
    }
}

String String::join(const StringList &stringList, const char *sep)
{
    if (stringList.isEmpty()) return {};

    String joined = stringList.at(0);
    for (dsize i = 1; i < stringList.size(); ++i)
    {
        joined += sep;
        joined += stringList.at(i);
    }
    return joined;
}

String String::patternFormat(const_iterator &      formatIter,
                             const const_iterator &formatEnd,
                             const IPatternArg &   arg)
{
    advanceFormat(formatIter, formatEnd);

    String result;

    // An argument comes here.
    bool  rightAlign = true;
    dsize maxWidth = 0;
    dsize minWidth = 0;

    DE_ASSERT(*formatIter != '%');

    if (*formatIter == '-')
    {
        // Left aligned.
        rightAlign = false;
        advanceFormat(formatIter, formatEnd);
    }
    String::const_iterator k = formatIter;
    while (iswdigit(*formatIter))
    {
        advanceFormat(formatIter, formatEnd);
    }
    if (k != formatIter)
    {
        // Got the minWidth.
        minWidth = String(k, formatIter).toInt();
    }
    if (*formatIter == '.')
    {
        advanceFormat(formatIter, formatEnd);
        k = formatIter;
        // There's also a maxWidth.
        while (iswdigit(*formatIter))
        {
            advanceFormat(formatIter, formatEnd);
        }
        maxWidth = String(k, formatIter).toInt();
    }

    // Finally, the type formatting.
    switch (*formatIter)
    {
    case 's':
        result += arg.asText();
        break;

    case 'b':
        result += (int(arg.asNumber())? "True" : "False");
        break;

    case 'c':
        result += Char(arg.asNumber());
        break;

    case 'i':
    case 'd':
        result += format("%lli", dint64(arg.asNumber()));
        break;

    case 'u':
        result += format("%llu", duint64(arg.asNumber()));
        break;

    case 'X':
        result += format("%llX", dint64(arg.asNumber()));
        break;

    case 'x':
        result += format("%llx", dint64(arg.asNumber()));
        break;

    case 'p':
        result += format("%p", dintptr(arg.asNumber()));
        break;

    case 'f':
        // Max width is interpreted as the number of decimal places.
        result += format(stringf("%%.%df", maxWidth? maxWidth : 3).c_str(), arg.asNumber());
        maxWidth = 0;
        break;

    default:
        throw IllegalPatternError("Log::Entry::str",
            "Unknown format character '" + String(1, *formatIter) + "'");
    }

    // Align and fit.
    if (maxWidth || minWidth)
    {
        // Must determine actual character count.
        CharPos len = result.sizec();

        if (maxWidth && len.index > maxWidth)
        {
            result = result.left(CharPos(maxWidth));
            len.index = maxWidth;
        }

        if (minWidth && len.index < minWidth)
        {
            // Pad it.
            String padding = String(minWidth - len.index, ' ');
            if (rightAlign)
            {
                result = padding + result;
            }
            else
            {
                result += padding;
            }
        }
    }
    return result;
}

Block String::toUtf8() const
{
    return Block(&_str.chars);
}

Block String::toLatin1() const
{
    // Non-8-bit characters are simply filtered out.
    Block latin;
    for (iChar ch : *this)
{
        if (ch < 256) latin.append(Block::Byte(ch));
}
    return latin;
}

String String::fromUtf8(const IByteArray &byteArray)
{
    String s;
    setBlock_String(&s._str, Block(byteArray));
    return s;
}

String String::fromUtf8(const Block &block)
{
    String s;
    setBlock_String(&s._str, block);
    return s;
}

String String::fromUtf8(char const *nullTerminatedCStr)
{
    return String(nullTerminatedCStr);
}

String String::fromLatin1(const IByteArray &byteArray)
{
    const Block bytes(byteArray);
    return String(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

String String::fromCP437(const IByteArray &byteArray)
{
    const Block chars(byteArray);
    String conv;
    for (auto ch : chars)
    {
        conv += Char(codePage437ToUnicode(ch));
    }
    return conv;
}

Block String::toPercentEncoding() const
{
    iString *enc = urlEncode_String(*this);
    Block b(&enc->chars);
    delete_String(enc);
    return b;
}

String String::fromPercentEncoding(const Block &percentEncoded) // static
{
    return String::take(urlDecode_String(String(percentEncoded)));
}

Char mb_iterator::operator*() const
{
    return decode();
}

Char mb_iterator::decode() const
{
    Char ch;
    const char *end = i;
    for (int j = 0; *end && j < MB_CUR_MAX; ++j, ++end) {}
    curCharLen = std::mbrtowc(&ch, i, end - i, &mb);
    return ch;
}

mb_iterator &mb_iterator::operator++()
{
    if (!curCharLen) decode();
    i += de::max(curCharLen, dsize(1));
    curCharLen = 0;
    return *this;
}

mb_iterator mb_iterator::operator++(int)
{
    mb_iterator i = *this;
    ++(*this);
    return i;
}

mb_iterator mb_iterator::operator+(int offset) const
{
    mb_iterator i = *this;
    return i += offset;
}

mb_iterator &mb_iterator::operator+=(int offset)
{
    while (offset-- > 0) ++(*this);
    return *this;
}

std::string stringf(const char *format, ...)
{
    // First determine the length of the result.
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    const int requiredLength = vsnprintf(nullptr, 0, format, args1);
    va_end(args1);
    // Format the output to a new string.
    std::string str(requiredLength, '\0');
    vsnprintf(&str[0], str.size() + 1, format, args2);
    va_end(args2);
    return str;
}

} // namespace de
