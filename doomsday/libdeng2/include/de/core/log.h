/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_LOG_H
#define LIBDENG2_LOG_H

#include "../Time"
#include "../String"
#include "../Lockable"
#include "../Guard"
#include "../ISerializable"

#include <QList>
#include <vector>
#include <string>
#include <cstdlib>

/// Macro for accessing the local log of the current thread.
#define LOG()               de::Log::threadLog()

/// Macro for accessing the local log of the current thread and entering
/// a new log section.
#define LOG_AS(sectionName) \
    de::Log::Section __logSection = de::Log::Section(sectionName);

/**
 * Macro for accessing the local log of the current thread and entering
 * a new log section with a de::String variable based name.
 *
 * @param str  Anything out of which a de::String can be constructed.
 *
 * @note Log::Section doesn't own the strings passed in; we have to
 * ensure that the string exists in memory as long as the section (scope)
 * is valid.
 */
#define LOG_AS_STRING(str) \
    de::String __logSectionName = str; \
    de::Block __logSectionUtf8 = __logSectionName.toUtf8(); \
    LOG_AS(__logSectionUtf8.constData());

// End-user/game audience:
#define LOG_AT_LEVEL(level, str)        de::LogEntryStager(level, str)
#define LOG_XVERBOSE(str)               LOG_AT_LEVEL(de::LogEntry::XVerbose,  str)
#define LOG_VERBOSE(str)                LOG_AT_LEVEL(de::LogEntry::Verbose,   str)
#define LOG_MSG(str)                    LOG_AT_LEVEL(de::LogEntry::Message,   str)
#define LOG_INFO(str)                   LOG_AT_LEVEL(de::LogEntry::Important, str)
#define LOG_WARNING(str)                LOG_AT_LEVEL(de::LogEntry::Warning,   str)
#define LOG_ERROR(str)                  LOG_AT_LEVEL(de::LogEntry::Error,     str)
#define LOG_CRITICAL(str)               LOG_AT_LEVEL(de::LogEntry::Critical,  str)

// Custom combination of audiences:
#define LOG_XVERBOSE_TO(audflags, str)  LOG_AT_LEVEL(audflags | de::LogEntry::XVerbose,  str)
#define LOG_VERBOSE_TO(audflags, str)   LOG_AT_LEVEL(audflags | de::LogEntry::Verbose,   str)
#define LOG_MSG_TO(audflags, str)       LOG_AT_LEVEL(audflags | de::LogEntry::Message,   str)
#define LOG_INFO_TO(audflags, str)      LOG_AT_LEVEL(audflags | de::LogEntry::Important, str)
#define LOG_WARNING_TO(audflags, str)   LOG_AT_LEVEL(audflags | de::LogEntry::Warning,   str)
#define LOG_ERROR_TO(audflags, str)     LOG_AT_LEVEL(audflags | de::LogEntry::Error,     str)
#define LOG_CRITICAL_TO(audflags, str)  LOG_AT_LEVEL(audflags | de::LogEntry::Critical,  str)

// Resource developer audience:
#define LOG_RES_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Resource | level, str)
#define LOG_RES_XVERBOSE(str)           LOG_RES_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_RES_VERBOSE(str)            LOG_RES_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_RES_MSG(str)                LOG_RES_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_RES_INFO(str)               LOG_RES_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_RES_WARNING(str)            LOG_RES_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_RES_ERROR(str)              LOG_RES_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_RES_CRITICAL(str)           LOG_RES_AT_LEVEL(de::LogEntry::Critical, str)

// Map developer audience:
#define LOG_MAP_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Map | level, str)
#define LOG_MAP_XVERBOSE(str)           LOG_MAP_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_MAP_VERBOSE(str)            LOG_MAP_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_MAP_MSG(str)                LOG_MAP_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_MAP_INFO(str)               LOG_MAP_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_MAP_WARNING(str)            LOG_MAP_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_MAP_ERROR(str)              LOG_MAP_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_MAP_CRITICAL(str)           LOG_MAP_AT_LEVEL(de::LogEntry::Critical, str)

// Script developer audience:
#define LOG_SCR_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Script | level, str)
#define LOG_SCR_XVERBOSE(str)           LOG_SCR_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_SCR_VERBOSE(str)            LOG_SCR_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_SCR_MSG(str)                LOG_SCR_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_SCR_INFO(str)               LOG_SCR_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_SCR_WARNING(str)            LOG_SCR_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_SCR_ERROR(str)              LOG_SCR_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_SCR_CRITICAL(str)           LOG_SCR_AT_LEVEL(de::LogEntry::Critical, str)

// Audio audience:
#define LOG_AUDIO_AT_LEVEL(level, str)  LOG_AT_LEVEL(de::LogEntry::Audio | level, str)
#define LOG_AUDIO_XVERBOSE(str)         LOG_AUDIO_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_AUDIO_VERBOSE(str)          LOG_AUDIO_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_AUDIO_MSG(str)              LOG_AUDIO_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_AUDIO_INFO(str)             LOG_AUDIO_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_AUDIO_WARNING(str)          LOG_AUDIO_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_AUDIO_ERROR(str)            LOG_AUDIO_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_AUDIO_CRITICAL(str)         LOG_AUDIO_AT_LEVEL(de::LogEntry::Critical, str)

// Graphics audience:
#define LOG_GL_AT_LEVEL(level, str)     LOG_AT_LEVEL(de::LogEntry::GL | level, str)
#define LOG_GL_XVERBOSE(str)            LOG_GL_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_GL_VERBOSE(str)             LOG_GL_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_GL_MSG(str)                 LOG_GL_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_GL_INFO(str)                LOG_GL_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_GL_WARNING(str)             LOG_GL_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_GL_ERROR(str)               LOG_GL_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_GL_CRITICAL(str)            LOG_GL_AT_LEVEL(de::LogEntry::Critical, str)

// Input audience:
#define LOG_INPUT_AT_LEVEL(level, str)  LOG_AT_LEVEL(de::LogEntry::Input | level, str)
#define LOG_INPUT_XVERBOSE(str)         LOG_INPUT_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_INPUT_VERBOSE(str)          LOG_INPUT_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_INPUT_MSG(str)              LOG_INPUT_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_INPUT_INFO(str)             LOG_INPUT_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_INPUT_WARNING(str)          LOG_INPUT_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_INPUT_ERROR(str)            LOG_INPUT_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_INPUT_CRITICAL(str)         LOG_INPUT_AT_LEVEL(de::LogEntry::Critical, str)

// Network audience:
#define LOG_NET_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Network | level, str)
#define LOG_NET_XVERBOSE(str)           LOG_NET_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_NET_VERBOSE(str)            LOG_NET_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_NET_MSG(str)                LOG_NET_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_NET_INFO(str)               LOG_NET_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_NET_WARNING(str)            LOG_NET_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_NET_ERROR(str)              LOG_NET_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_NET_CRITICAL(str)           LOG_NET_AT_LEVEL(de::LogEntry::Critical, str)

// Native code developer audience:
#define LOG_DEV_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Dev | level, str)
#define LOG_DEV_XVERBOSE(str)           LOG_DEV_AT_LEVEL(de::LogEntry::XVerbose, str)
#define LOG_TRACE(str)                  LOG_DEV_XVERBOSE(str) // backwards comp
#define LOG_DEV_VERBOSE(str)            LOG_DEV_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_DEBUG(str)                  LOG_DEV_VERBOSE(str) // backwards comp
#define LOG_DEV_MSG(str)                LOG_DEV_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_DEV_INFO(str)               LOG_DEV_AT_LEVEL(de::LogEntry::Info,     str)
#define LOG_DEV_WARNING(str)            LOG_DEV_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_DEV_ERROR(str)              LOG_DEV_AT_LEVEL(de::LogEntry::Error,    str)

#ifdef DENG2_DEBUG
/**
 * Makes a developer-only extra verbose level log entry. Only enabled in debug builds; use this
 * for internal messages that might have a significant processing overhead. (Note that parameters
 * differ compared to the normal LOG_* macros.)
 */
#  define LOG_DEV_TRACE_DEBUGONLY(form, args) LOG_TRACE(form) << args
#else
#  define LOG_DEV_TRACE_DEBUGONLY(form, args)
#endif

#ifdef WIN32
#   undef ERROR
#endif

namespace de {

class LogBuffer;


/**
 * An entry to be stored in the log entry buffer. Log entries are created with
 * Log::enter().
 *
 * Log entry arguments must be created before the entry itself is created. The
 * LogEntryStager class is designed to help with this. Once an entry is
 * inserted to the log buffer, no modifications may be done to it any more
 * because another thread may need it immediately for flushing.
 *
 * @ingroup core
 */
class DENG2_PUBLIC LogEntry : public Lockable, public ISerializable
{
public:
    /// Target audience of the entry (bits). If not given, the entry is intended for the
    /// end-user/player.
    enum Audience
    {
        Resource = 0x10000,     ///< Resource or resource pack developer (files, etc.)
        Map      = 0x20000,     ///< Map developer
        Script   = 0x40000,     ///< Script developer
        GL       = 0x80000,     ///< GL developer (shaders, etc.)
        Audio    = 0x100000,    ///< Audio developer
        Input    = 0x200000,    ///< Input events, devices, etc.
        Network  = 0x400000,    ///< Network connections, packets, etc.
        Dev      = 0x800000,    ///< Native code developer (i.e., the programmer)

        AudienceMask = 0xff0000
    };

    static String audienceToText(Audience audience)
    {
        switch(audience)
        {
        case Resource: return "Resource";
        case Map:      return "Map";
        case Script:   return "Script";
        case GL:       return "GL";
        case Audio:    return "Audio";
        case Input:    return "Input";
        case Network:  return "Network";
        case Dev:      return "Dev";
        default:       return "";
        }
    }

    static Audience textToAudience(String text)
    {
        for(int i = 16; i < 32; ++i)
        {
            if(!audienceToText(Audience(1 << i)).compareWithoutCase(text))
                return Audience(1 << i);
        }
        throw de::Error("Log::textToAudience", "'" + text + "' is not a valid log audience");
    }

    /// Importance level of the log entry.
    enum Level
    {
        /**
         * Verbose messages should be used for logging additional/supplementary information. All
         * verbose messages can be safely ignored.
         */
        XVerbose = 1,
        Verbose = 2,

        /**
         * The base level: normal log entries.
         */
        Message = 3,

        /**
         * Important messages are intended for situations that are particularly noteworthy. They
         * will not cause an alert to be raised, but the information is deemed particularly
         * valuable.
         */
        Important = 4,

        /**
         * Warning messages are reserved for error situations that were automatically recovered
         * from. A warning might be logged for example when the expected resource could not be
         * found, and a fallback resource was used instead. Warnings will cause an alert to be
         * raised so that the target audience is aware of the problem.
         */
        Warning = 5,

        /**
         * Error messages are intended for errors that could not be (fully) recovered from. The
         * error is grave enough to possibly cause the shutting down of the current game, however
         * the engine can still remain running. Will cause an alert to be raised so that the
         * target audience is aware of the problem.
         */
        Error = 6,

        /**
         * Critical messages are intended for fatal errors that cause the engine to be shut down.
         */
        Critical = 7,

        MAX_LOG_LEVELS,
        LOWEST_LOG_LEVEL = XVerbose,

        LevelMask = 0x7
    };   

    static String levelToText(Level level)
    {
        switch(level)
        {
        case XVerbose:  return "XVerbose";
        case Verbose:   return "Verbose";
        case Message:   return "Message";
        case Important: return "Important";
        case Warning:   return "Warning";
        case Error:     return "Error";
        case Critical:  return "Critical";
        default:        return "";
        }
    }

    static Level textToLevel(String text)
    {
        for(int i = XVerbose; i < MAX_LOG_LEVELS; ++i)
        {
            if(!levelToText(Level(i)).compareWithoutCase(text))
                return Level(i);
        }
        throw de::Error("Log::textToLevel", "'" + text + "' is not a valid log level");
    }

    /**
     * Argument for a log entry. The arguments of an entry are usually created
     * automatically by LogEntryStager.
     *
     * @ingroup core
     */
    class DENG2_PUBLIC Arg : public String::IPatternArg, public ISerializable
    {
    public:
        /// The wrong type is used in accessing the value. @ingroup errors
        DENG2_ERROR(TypeError);

        enum Type {
            IntegerArgument,
            FloatingPointArgument,
            StringArgument
        };

        /**
         * Base class for classes that support adding to the arguments. Any
         * class that is derived from this class may be used as an argument for
         * log entries. In practice, all arguments are converted to either
         * numbers (64-bit integer or double) or text strings.
         */
        class Base {
        public:
            /// Attempted conversion from unsupported type.
            DENG2_ERROR(TypeError);

        public:
            virtual ~Base() {}

            virtual Type logEntryArgType() const = 0;
            virtual dint64 asInt64() const {
                throw TypeError("LogEntry::Arg::Base", "dint64 not supported");
            }
            virtual ddouble asDouble() const {
                throw TypeError("LogEntry::Arg::Base", "ddouble not supported");
            }
            virtual String asText() const {
                throw TypeError("LogEntry::Arg::Base", "String not supported");
            }
        };

    public:
        Arg()                    : _type(IntegerArgument)       { _data.intValue    = 0; }
        Arg(dint i)              : _type(IntegerArgument)       { _data.intValue    = i; }
        Arg(duint i)             : _type(IntegerArgument)       { _data.intValue    = i; }
        Arg(long int i)          : _type(IntegerArgument)       { _data.intValue    = i; }
        Arg(long unsigned int i) : _type(IntegerArgument)       { _data.intValue    = i; }
        Arg(duint64 i)           : _type(IntegerArgument)       { _data.intValue    = dint64(i); }
        Arg(dint64 i)            : _type(IntegerArgument)       { _data.intValue    = i; }
        Arg(ddouble d)           : _type(FloatingPointArgument) { _data.floatValue  = d; }
        Arg(void const *p)       : _type(IntegerArgument)       { _data.intValue    = dint64(p); }
        Arg(char const *s)       : _type(StringArgument)        { _data.stringValue = new String(s); }
        Arg(String const &s)     : _type(StringArgument)        { _data.stringValue = new String(s.data(), s.size()); }

        Arg(Base const &arg);
        Arg(Arg const &other);

        ~Arg();

        inline Type type() const { return _type; }
        inline dint64 intValue() const {
            DENG2_ASSERT(_type == IntegerArgument);
            return _data.intValue;
        }
        inline ddouble floatValue() const {
            DENG2_ASSERT(_type == FloatingPointArgument);
            return _data.floatValue;
        }
        inline QString stringValue() const {
            DENG2_ASSERT(_type == StringArgument);
            return *_data.stringValue;
        }

        // Implements String::IPatternArg.
        ddouble asNumber() const;
        String asText() const;

        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);

    private:
        Type _type;
        union Data {
            dint64 intValue;
            ddouble floatValue;
            String *stringValue;
        } _data;
    };

public:
    enum Flag
    {
        /// In simple mode, only print the actual message contents,
        /// without metadata.
        Simple = 0x1,

        /// Use escape sequences to format the entry with text styles
        /// (for graphical output).
        Styled = 0x2,

        /// Omit the section from the entry text.
        OmitSection = 0x4,

        /// Indicate that the section is the same as on the previous line.
        SectionSameAsBefore = 0x8,

        /// Parts of the section can be abbreviated because they are clear
        /// from the context (e.g., previous line).
        AbbreviateSection = 0x10,

        /// Entry is not from a local source. Could be used to mark entries
        /// originating from a remote LogBuffer (over the network).
        Remote = 0x20,

        /// Entry level is not included in the output.
        OmitLevel = 0x40
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef QList<Arg *> Args;

public:
    /**
     * Constructs a disabled log entry.
     */
    LogEntry();

    LogEntry(duint32 levelAndAudience, String const &section, int sectionDepth, String const &format, Args args);

    /**
     * Copy constructor.
     *
     * @param other       Log entry.
     * @param extraFlags  Additional flags to apply to the new entry.
     */
    LogEntry(LogEntry const &other, Flags extraFlags = 0);

    ~LogEntry();

    Flags flags() const;

    /// Returns the timestamp of the entry.
    Time when() const { return _when; }

    inline duint32 audience() const { return _levelAudience & AudienceMask; }

    inline Level level() const { return Level(_levelAudience & LevelMask); }

    /// Returns a reference to the entry's section part. Reference is valid
    /// for the lifetime of the entry.
    String const &section() const { return _section; }

    /// Returns the number of sub-sections in the entry's section part.
    int sectionDepth() const { return _sectionDepth; }

    /**
     * Converts the log entry to a string.
     *
     * @param flags           Flags that control how the text is composed.
     * @param shortenSection  Number of characters to cut from the beginning of the section.
     *                        With AbbreviateSection this limits which portion of the
     *                        section is subject to abbreviation.
     *
     * @return Composed textual representation of the entry.
     */
    String asText(Flags const &flags = 0, int shortenSection = 0) const;    

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    void advanceFormat(String::const_iterator &i) const;

private:
    Time _when;
    duint32 _levelAudience;
    String _section;
    int _sectionDepth;
    String _format;
    Flags _defaultFlags;
    bool _disabled;
    Args _args;
};

QTextStream &operator << (QTextStream &stream, LogEntry::Arg const &arg);

Q_DECLARE_OPERATORS_FOR_FLAGS(LogEntry::Flags)

/**
 * Provides means for adding log entries into the log entry buffer (LogBuffer).
 * Each thread has its own Log instance. A thread's Log keeps track of the
 * thread-local section stack.
 *
 * Note that there is only one LogBuffer where all the entries are collected.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Log
{
public:
    class DENG2_PUBLIC Section
    {
    public:
        /**
         * The Section does not take a copy of @c name, so whatever
         * it's pointing to must exist while the Section exists.
         *
         * @param name  Name of the log section.
         */
        Section(char const *name);
        ~Section();

        Log &log() const { return _log; }

    private:
        Log &_log;
        char const *_name;
    };

public:
    Log();
    virtual ~Log();

    /**
     * Begins a new section in the log. Sections can be nested.
     *
     * @param name  Name of the section. No copy of this string is made,
     *              so it must exist while the section is in use.
     */
    void beginSection(char const *name);

    /**
     * Ends the topmost section in the log.
     *
     * @param name  Name of the topmost section.
     */
    void endSection(char const *name);

    /**
     * Creates a new log entry with the default (Message) level, targeted to the end-user.
     *
     * @param format     Format template of the entry.
     * @param arguments  List of arguments. The entry is given ownership of
     *                   each Arg instance.
     */
    LogEntry &enter(String const &format, LogEntry::Args arguments = LogEntry::Args());

    /**
     * Creates a new log entry with the specified log entry level.
     *
     * @param levelAndAudience  Level of the entry and target audience bits.
     * @param format            Format template of the entry.
     * @param arguments         List of arguments. The entry is given ownership of
     *                          each Arg instance.
     */
    LogEntry &enter(duint32 levelAndAudience, String const &format, LogEntry::Args arguments = LogEntry::Args());

public:
    /**
     * Returns the logger of the current thread.
     */
    static Log &threadLog();

    /**
     * Deletes the current thread's log. Threads should call this before
     * they quit.
     */
    static void disposeThreadLog();

private:
    typedef QList<char const *> SectionStack;
    SectionStack _sectionStack;

    LogEntry *_throwawayEntry;
};

/**
 * Stages a log entry for insertion into LogBuffer. Instances of LogEntryStager
 * are built on the stack.
 *
 * You should use the @c LOG_* macros instead of using LogEntryStager directly.
 */
class DENG2_PUBLIC LogEntryStager
{
public:
    LogEntryStager(duint32 levelAndAudience, String const &format);

    /// Appends a new argument to the entry.
    template <typename ValueType>
    inline LogEntryStager &operator << (ValueType const &v) {
        if(!_disabled) {
            // Args are created only if the level is enabled.
            _args.append(new LogEntry::Arg(v));
        }
        return *this;
    }

    ~LogEntryStager() {
        if(!_disabled) {
            // Ownership of the entries is transferred to the LogEntry.
            LOG().enter(_level, _format, _args);
        }
    }

private:
    bool _disabled;
    duint32 _level;
    String _format;
    LogEntry::Args _args;
};

} // namespace de

#endif /* LIBDENG2_LOG_H */
