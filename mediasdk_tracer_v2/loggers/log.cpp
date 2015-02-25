#include "log.h"

Log* Log::_sing_log = NULL;
bool Log::useGUI = false;

Log::Log()
{
    _log_level = LOG_LEVEL_DEFAULT;
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_CONSOLE, new LogConsole()));
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_FILE, new LogFile()));
#if defined(_WIN32) || defined(_WIN64)
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_ETW, new LogEtwEvents()));
#elif defined(ANDROID)
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_LOGCAT, new LogLogcat()));
#else
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_SYSLOG, new LogSyslog()));
#endif

    _log = _logmap[LOG_FILE];


#if 0
#if defined(ANDROID)
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_LOGCAT, new LogLogcat()));
    _log = _logmap[LOG_CONSOLE];
#else
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_CONSOLE, new LogConsole()));
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_FILE, new LogFile()));
#if defined(_WIN32) || defined(_WIN64)
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_ETW, new LogEtwEvents()));
#else
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_SYSLOG, new LogSyslog()));
#endif // _WIN32...
    _log = _logmap[LOG_CONSOLE];
#endif // ANDROID

    #endif
}

Log::~Log()
{
    for(std::map<eLogType,ILog*>::iterator it=_logmap.begin(); it!=_logmap.end(); ++it){
        delete it->second;
    }
    _logmap.clear();
}

void Log::SetLogType(eLogType type)
{
    if(!_sing_log)
        _sing_log = new Log();

    _sing_log->_log = _sing_log->_logmap[type];
}

void Log::SetLogLevel(eLogLevel level)
{
    if(!_sing_log)
        _sing_log = new Log();

    _sing_log->_log_level = level;
}

eLogLevel Log::GetLogLevel()
{
    if(!_sing_log)
        _sing_log = new Log();

    return _sing_log->_log_level;
}

void Log::clear()
{
    if (_sing_log)
    {
        delete _sing_log; 
        _sing_log = NULL;
    }
}

void Log::WriteLog(const std::string &log)
{       
    
#if defined(_WIN32) || defined(_WIN64)
    DWORD start_flag = 0;
    DWORD size = sizeof(DWORD);
    RegGetValue(HKEY_LOCAL_MACHINE, ("Software\\Intel\\MediaSDK\\Dispatch\\tracer"), ("_start"), REG_DWORD, 0, (BYTE*)&start_flag, &size); 
        
    if ((start_flag) || (!useGUI))
#endif
        {
            if(!_sing_log)
                _sing_log = new Log();
                    
            

            if(_sing_log->_log_level == LOG_LEVEL_DEFAULT){
                _sing_log->_log->WriteLog(log);
            }
            else if(_sing_log->_log_level == LOG_LEVEL_FULL){
                _sing_log->_log->WriteLog(log);
            }
            else if(_sing_log->_log_level == LOG_LEVEL_SHORT){
                if(log.find("function:") != std::string::npos /*&& log.find("-") != std::string::npos*/)
                    _sing_log->_log->WriteLog(log /*.substr(0, log.size()-2)*/);
            }

            
        }
      
        
}

