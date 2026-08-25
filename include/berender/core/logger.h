#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace berender {

enum class LogLevel : uint8_t { Trace=0,Debug=1,Info=2,Warn=3,Error=4,Fatal=5,Off=6 };
using LogCallback = std::function<void(LogLevel level,const char* file,int line,const char* message)>;

class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel level);
    LogLevel level() const { return m_level; }
    void setCallback(LogCallback callback);
    void log(LogLevel level,const char* file,int line,const char* fmt,...);
private:
    Logger();
    LogLevel m_level;
    LogCallback m_callback;
    bool m_useStderr;
};

#define BERENDER_LOG(level,...) ::berender::Logger::instance().log(level,__FILE__,__LINE__,__VA_ARGS__)
#define BERENDER_TRACE(...) BERENDER_LOG(::berender::LogLevel::Trace,__VA_ARGS__)
#define BERENDER_DEBUG(...) BERENDER_LOG(::berender::LogLevel::Debug,__VA_ARGS__)
#define BERENDER_INFO(...)  BERENDER_LOG(::berender::LogLevel::Info,__VA_ARGS__)
#define BERENDER_WARN(...)  BERENDER_LOG(::berender::LogLevel::Warn,__VA_ARGS__)
#define BERENDER_ERROR(...) BERENDER_LOG(::berender::LogLevel::Error,__VA_ARGS__)
#define BERENDER_FATAL(...) BERENDER_LOG(::berender::LogLevel::Fatal,__VA_ARGS__)

class ScopedLogLevel {
public:
    explicit ScopedLogLevel(LogLevel level):m_old(Logger::instance().level()){Logger::instance().setLevel(level);}
    ~ScopedLogLevel(){Logger::instance().setLevel(m_old);}
private: LogLevel m_old;
};

} // namespace berender
