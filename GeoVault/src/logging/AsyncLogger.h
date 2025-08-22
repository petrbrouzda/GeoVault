
#ifndef __ASYNC_LOGGER_H
#define __ASYNC_LOGGER_H

/**
 * AsyncLogger se pouziva pro logovani udalosti v asynchronnich aktivitach (detektor vyraceni, webserver...).
   Uklada zaznamy do pole v pameti a ty se pak vypisou v loop() pomoci volani dumpTo();
 */

#include <mutex>

#include "LoggerInterface.h"


/*
    Simple logging interface.
    Implements printf()-like functionality.
    
    If you want to log non-trivial objects (for example String or IPaddress) you can't pass them asi object:
        error: cannot pass objects of non-trivially-copyable type 'class IPAddress' through '...'
    raLogger implements Print interface for this.
    Printed data are stored into public char* logger->printed.
    Buffer is deleted after call to ->log, so this is the correct usage and you don't need to think of buffer used:

        IPAddress ip = WiFi.localIP();
        logger->print( ip );
        logger->log( "IP address = %s", logger->printed );
*/




#define ASYNCLOG_LINE_SIZE 500
#define ASYNCLOG_STORAGE_SIZE 10000


class AsyncLogger : public LoggerInterface
{
  public:
    AsyncLogger();
    void log ( const char * format, ... );
    
    char * getOutputBuffer();
    bool overload;

    /** 
     * Vypise obsah bufferu do streamu
     */
    void dumpTo( Stream * out );

  private:
    char msgBuffer[ ASYNCLOG_LINE_SIZE+2 ];

    char * outputBuffer[2];
    int bufferInUse;
    char currentBufferSize;

    std::mutex serial_mtx;
};

#endif   // __ASYNC_LOGGER_H
