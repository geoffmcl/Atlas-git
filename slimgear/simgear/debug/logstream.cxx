// Stream based logging mechanism.
//
// Written by Bernie Bright, 1998
//
// Copyright (C) 1998  Bernie Bright - bbright@c031.aone.net.au
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id: logstream.cxx,v 1.2 2017/03/14 00:23:48 schack Exp $

#include <simgear_config.h>

#include "logstream.hxx"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// EYE - we don't want Boost
// #include <boost/foreach.hpp>

#include <simgear/sg_inlines.h>
#include <simgear/threads/SGThread.hxx>
#include <simgear/threads/SGQueue.hxx>
#include <simgear/threads/SGGuard.hxx>

#include <simgear/misc/sgstream.hxx>
#include <simgear/misc/sg_path.hxx>

#if defined (SG_WINDOWS)
// for AllocConsole, OutputDebugString
    #include <windows.h>
#endif

const char* debugClassToString(sgDebugClass c)
{
    switch (c) {
    case SG_NONE:       return "none";
    case SG_TERRAIN:    return "terrain";
    case SG_ASTRO:      return "astro";
    case SG_FLIGHT:     return "flight";
    case SG_INPUT:      return "input";
    case SG_GL:         return "opengl";
    case SG_VIEW:       return "view";
    case SG_COCKPIT:    return "cockpit";
    case SG_GENERAL:    return "general";
    case SG_MATH:       return "math";
    case SG_EVENT:      return "event";
    case SG_AIRCRAFT:   return "aircraft";
    case SG_AUTOPILOT:  return "autopilot";
    case SG_IO:         return "io";
    case SG_CLIPPER:    return "clipper";
    case SG_NETWORK:    return "network";
    case SG_ATC:        return "atc";
    case SG_NASAL:      return "nasal";
    case SG_INSTR:      return "instruments";
    case SG_SYSTEMS:    return "systems";
    case SG_AI:         return "ai";
    case SG_ENVIRONMENT:return "environment";
    case SG_SOUND:      return "sound";
    case SG_NAVAID:     return "navaid";
    case SG_GUI:        return "gui";
    case SG_TERRASYNC:  return "terrasync";
    case SG_PARTICLES:  return "particles";
    default:            return "unknown";
    }
}

//////////////////////////////////////////////////////////////////////////////

namespace simgear
{

LogCallback::LogCallback(sgDebugClass c, sgDebugPriority p) :
	m_class(c),
	m_priority(p)
{
}

bool LogCallback::shouldLog(sgDebugClass c, sgDebugPriority p) const
{
	 return ((c & m_class) != 0 && p >= m_priority);
}

void LogCallback::setLogLevels( sgDebugClass c, sgDebugPriority p )
{
	m_priority = p;
	m_class = c;
}

} // of namespace simgear

//////////////////////////////////////////////////////////////////////////////

class FileLogCallback : public simgear::LogCallback
{
public:
    FileLogCallback(const SGPath& aPath, sgDebugClass c, sgDebugPriority p) :
	    simgear::LogCallback(c, p)
    {
        m_file.open(aPath, std::ios_base::out | std::ios_base::trunc);
    }

    virtual void operator()(sgDebugClass c, sgDebugPriority p,
        const char* file, int line, const std::string& message)
    {
        if (!shouldLog(c, p)) return;
        m_file << debugClassToString(c) << ":" << (int) p
            << ":" << file << ":" << line << ":" << message << std::endl;
    }
private:
    sg_ofstream m_file;
};

class StderrLogCallback : public simgear::LogCallback
{
public:
    StderrLogCallback(sgDebugClass c, sgDebugPriority p) :
		simgear::LogCallback(c, p)
    {
    }

#if defined (SG_WINDOWS)
    ~StderrLogCallback()
    {
        FreeConsole();
    }
#endif

    virtual void operator()(sgDebugClass c, sgDebugPriority p,
        const char* file, int line, const std::string& aMessage)
    {
        if (!shouldLog(c, p)) return;

        fprintf(stderr, "%s\n", aMessage.c_str());
        //fprintf(stderr, "%s:%d:%s:%d:%s\n", debugClassToString(c), p,
        //    file, line, aMessage.c_str());
        fflush(stderr);
    }
};


#ifdef SG_WINDOWS

class WinDebugLogCallback : public simgear::LogCallback
{
public:
    WinDebugLogCallback(sgDebugClass c, sgDebugPriority p) :
		simgear::LogCallback(c, p)
    {
    }

    virtual void operator()(sgDebugClass c, sgDebugPriority p,
        const char* file, int line, const std::string& aMessage)
    {
        if (!shouldLog(c, p)) return;

        std::ostringstream os;
		os << debugClassToString(c) << ":" << aMessage << std::endl;
		OutputDebugStringA(os.str().c_str());
    }
};

#endif

class LogStreamPrivate : public SGThread
{
private:
    /**
     * storage of a single log entry. Note this is not used for a persistent
     * store, but rather for short term buffering between the submitting
     * and output threads.
     */
    class LogEntry
    {
    public:
        LogEntry(sgDebugClass c, sgDebugPriority p,
            const char* f, int l, const std::string& msg) :
            debugClass(c), debugPriority(p), file(f), line(l),
                message(msg)
        {
        }

        sgDebugClass debugClass;
        sgDebugPriority debugPriority;
        const char* file;
        int line;
        std::string message;
    };

    class PauseThread
    {
    public:
        PauseThread(LogStreamPrivate* parent) : m_parent(parent)
        {
            m_wasRunning = m_parent->stop();
        }

        ~PauseThread()
        {
            if (m_wasRunning) {
                m_parent->startLog();
            }
        }
    private:
        LogStreamPrivate* m_parent;
        bool m_wasRunning;
    };
public:
    LogStreamPrivate() :
        m_logClass(SG_ALL),
        m_logPriority(SG_ALERT),
        m_isRunning(false)
    {
        bool addStderr = true;
#if defined (SG_WINDOWS)
        // Check for stream redirection, has to be done before we call
        // Attach / AllocConsole
        const bool isFile = (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_DISK); // Redirect to file?
        if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
            // attach failed, don't install the callback
            addStderr = false;
        } else if (!isFile) {
			// No - OK! now set streams to attached console
			freopen("conout$", "w", stdout);
			freopen("conout$", "w", stderr);
		}
#endif
        if (addStderr) {
            m_callbacks.push_back(new StderrLogCallback(m_logClass, m_logPriority));
            m_consoleCallbacks.push_back(m_callbacks.back());
        }
#if defined (SG_WINDOWS) && !defined(NDEBUG)
		m_callbacks.push_back(new WinDebugLogCallback(m_logClass, m_logPriority));
		m_consoleCallbacks.push_back(m_callbacks.back());
#endif
    }

    ~LogStreamPrivate()
    {
	// EYE - replace Boost macro
        // BOOST_FOREACH(simgear::LogCallback* cb, m_callbacks) {
        //     delete cb;
        // }
	CallbackVec::iterator it;
	for (it = m_callbacks.begin(); it != m_callbacks.end(); it++) {
	    simgear::LogCallback* cb = *it;
            delete cb;
        }
    }

    SGMutex m_lock;
    SGBlockingQueue<LogEntry> m_entries;

    typedef std::vector<simgear::LogCallback*> CallbackVec;
    CallbackVec m_callbacks;
    /// subset of callbacks which correspond to stdout / console,
	/// and hence should dynamically reflect console logging settings
	CallbackVec m_consoleCallbacks;

    sgDebugClass m_logClass;
    sgDebugPriority m_logPriority;
    bool m_isRunning;

    void startLog()
    {
        SGGuard<SGMutex> g(m_lock);
        if (m_isRunning) return;
        m_isRunning = true;
        start();
    }

    virtual void run()
    {
        while (1) {
            LogEntry entry(m_entries.pop());
            // special marker entry detected, terminate the thread since we are
            // making a configuration change or quitting the app
            if ((entry.debugClass == SG_NONE) && !strcmp(entry.file, "done")) {
                return;
            }

            // submit to each installed callback in turn

	    // EYE - replace Boost macro
            // BOOST_FOREACH(simgear::LogCallback* cb, m_callbacks) {
            //     (*cb)(entry.debugClass, entry.debugPriority,
            //         entry.file, entry.line, entry.message);
            // }
	    CallbackVec::iterator it;
	    for (it = m_callbacks.begin(); it != m_callbacks.end(); it++) {
		simgear::LogCallback* cb = *it;
                (*cb)(entry.debugClass, entry.debugPriority,
		      entry.file, entry.line, entry.message);
	    }
        } // of main thread loop
    }

    bool stop()
    {
        SGGuard<SGMutex> g(m_lock);
        if (!m_isRunning) {
            return false;
        }

        // log a special marker value, which will cause the thread to wakeup,
        // and then exit
        log(SG_NONE, SG_ALERT, "done", -1, "");
        join();

        m_isRunning = false;
        return true;
    }

    void addCallback(simgear::LogCallback* cb)
    {
        PauseThread pause(this);
        m_callbacks.push_back(cb);
    }

    void removeCallback(simgear::LogCallback* cb)
    {
        PauseThread pause(this);
        CallbackVec::iterator it = std::find(m_callbacks.begin(), m_callbacks.end(), cb);
        if (it != m_callbacks.end()) {
            m_callbacks.erase(it);
        }
    }

    void setLogLevels( sgDebugClass c, sgDebugPriority p )
    {
        PauseThread pause(this);
        m_logPriority = p;
        m_logClass = c;
	// EYE - Replace Boost macro
	// BOOST_FOREACH(simgear::LogCallback* cb, m_consoleCallbacks) {
	//     cb->setLogLevels(c, p);
	// }
	CallbackVec::iterator it;
	for (it = m_callbacks.begin(); it != m_callbacks.end(); it++) {
	    simgear::LogCallback* cb = *it;
	    cb->setLogLevels(c, p);
	}
    }

    bool would_log( sgDebugClass c, sgDebugPriority p ) const
    {
        if (p >= SG_INFO) return true;
        return ((c & m_logClass) != 0 && p >= m_logPriority);
    }

    void log( sgDebugClass c, sgDebugPriority p,
            const char* fileName, int line, const std::string& msg)
    {
        LogEntry entry(c, p, fileName, line, msg);
        m_entries.push(entry);
    }
};

/////////////////////////////////////////////////////////////////////////////

static logstream* global_logstream = NULL;
static LogStreamPrivate* global_privateLogstream = NULL;
static SGMutex global_logStreamLock;

logstream::logstream()
{
    global_privateLogstream = new LogStreamPrivate;
    global_privateLogstream->startLog();
}

logstream::~logstream()
{
    popup_msgs.clear();
    global_privateLogstream->stop();
    delete global_privateLogstream;
}

void
logstream::setLogLevels( sgDebugClass c, sgDebugPriority p )
{
    global_privateLogstream->setLogLevels(c, p);
}

void
logstream::addCallback(simgear::LogCallback* cb)
{
    global_privateLogstream->addCallback(cb);
}

void
logstream::removeCallback(simgear::LogCallback* cb)
{
    global_privateLogstream->removeCallback(cb);
}

void
logstream::log( sgDebugClass c, sgDebugPriority p,
        const char* fileName, int line, const std::string& msg)
{
    global_privateLogstream->log(c, p, fileName, line, msg);
}

void
logstream::popup( const std::string& msg)
{
    popup_msgs.push_back(msg);
}

std::string
logstream::get_popup()
{
    std::string rv = "";
    if (!popup_msgs.empty())
    {
        rv = popup_msgs.front();
        popup_msgs.erase(popup_msgs.begin());
    }
    return rv;
}

bool
logstream::has_popup()
{
    return (popup_msgs.size() > 0) ? true : false;
}

bool
logstream::would_log( sgDebugClass c, sgDebugPriority p ) const
{
    return global_privateLogstream->would_log(c,p);
}

sgDebugClass
logstream::get_log_classes() const
{
    return global_privateLogstream->m_logClass;
}

sgDebugPriority
logstream::get_log_priority() const
{
    return global_privateLogstream->m_logPriority;
}

void
logstream::set_log_priority( sgDebugPriority p)
{
    global_privateLogstream->setLogLevels(global_privateLogstream->m_logClass, p);
}

void
logstream::set_log_classes( sgDebugClass c)
{
    global_privateLogstream->setLogLevels(c, global_privateLogstream->m_logPriority);
}


logstream&
sglog()
{
    // Force initialization of cerr.
    static std::ios_base::Init initializer;

    // http://www.aristeia.com/Papers/DDJ_Jul_Aug_2004_revised.pdf
    // in the absence of portable memory barrier ops in Simgear,
    // let's keep this correct & safe
    SGGuard<SGMutex> g(global_logStreamLock);

    if( !global_logstream )
        global_logstream = new logstream();
    return *global_logstream;
}

void
logstream::logToFile( const SGPath& aPath, sgDebugClass c, sgDebugPriority p )
{
    global_privateLogstream->addCallback(new FileLogCallback(aPath, c, p));
}

namespace simgear
{

void requestConsole()
{
    // this is a no-op now, stub exists for compatability for the moment.
}

void shutdownLogging()
{
    SGGuard<SGMutex> g(global_logStreamLock);
    delete global_logstream;
    global_logstream = 0;
}

} // of namespace simgear
