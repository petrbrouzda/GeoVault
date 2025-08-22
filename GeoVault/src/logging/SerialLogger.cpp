
#include "SerialLogger.h"


SerialLogger::SerialLogger( Stream * out ) : LoggerInterface()
{
    this->out = out;
}


void SerialLogger::log( const char * format, ... )
{
    va_list args;
    va_start( args, format );
    int len = vsnprintf( this->msgBuffer, SERIALLOG_LINE_SIZE, format, args );
    va_end( args );
    this->msgBuffer[SERIALLOG_LINE_SIZE - 1] = 0;
    
    this->printPos = 0;
    this->printed[0] = 0;

    out->println( this->msgBuffer );
}


