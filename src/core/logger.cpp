#include "berender/core/logger.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace berender {
namespace {
const char* levelToString(LogLevel level) {
    switch(level){case LogLevel::Trace:return "TRACE";case LogLevel::Debug:return "DEBUG";case LogLevel::Info:return "INFO";case LogLevel::Warn:return "WARN";case LogLevel::Error:return "ERROR";case LogLevel::Fatal:return "FATAL";default:return "?";}
}
std::mutex& logMutex(){static std::mutex m;return m;}
}

Logger& Logger::instance(){static Logger inst;return inst;}
Logger::Logger():m_level(LogLevel::Info),m_useStderr(true){}
void Logger::setLevel(LogLevel level){m_level=level;}
void Logger::setCallback(LogCallback callback){std::lock_guard<std::mutex> lock(logMutex());m_callback=std::move(callback);m_useStderr=false;}
void Logger::log(LogLevel level,const char* file,int line,const char* fmt,...){
    if(level<m_level)return;
    char buffer[2048]; va_list args; va_start(args,fmt); vsnprintf(buffer,sizeof(buffer),fmt,args); va_end(args);
    std::lock_guard<std::mutex> lock(logMutex());
    if(m_callback){m_callback(level,file,line,buffer);}
    else if(m_useStderr){
        const char* filename=strrchr(file,'/');if(!filename)filename=strrchr(file,'\\');filename=filename?filename+1:file;
        fprintf(stderr,"[%s] %s:%d %s\n",levelToString(level),filename,line,buffer);
    }
}
} // namespace berender
