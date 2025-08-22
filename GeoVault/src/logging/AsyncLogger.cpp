
#include "AsyncLogger.h"


AsyncLogger::AsyncLogger() : LoggerInterface()
{
    this->bufferInUse = 0;
    this->outputBuffer[0] = (char *)malloc( ASYNCLOG_STORAGE_SIZE +10 );
    this->outputBuffer[0][0] = 0;
    this->outputBuffer[1] = (char *)malloc( ASYNCLOG_STORAGE_SIZE +10 );
    this->outputBuffer[1][0] = 0;
    this->currentBufferSize = 0;
    this->overload = false;
}

void AsyncLogger::log( const char * format, ... )
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    va_list args;
    va_start( args, format );
    int len = vsnprintf( this->msgBuffer, ASYNCLOG_LINE_SIZE, format, args );
    va_end( args );
    this->msgBuffer[ASYNCLOG_LINE_SIZE - 1] = 0;
    
    this->printPos = 0;
    this->printed[0] = 0;

    if( this->currentBufferSize + len > ASYNCLOG_STORAGE_SIZE ) {
        this->overload = true;
        return;
    }

    memcpy( this->outputBuffer[this->bufferInUse] + this->currentBufferSize, this->msgBuffer, len );
    strcpy( this->outputBuffer[this->bufferInUse] + this->currentBufferSize + len, "\n" );
    this->currentBufferSize += len + 1;
    this->outputBuffer[this->bufferInUse][this->currentBufferSize] = 0;
}




char* AsyncLogger::getOutputBuffer()
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);
    
    if( this->currentBufferSize == 0 ) {
        // nemame nic k tisku
        return NULL;
    }
    char * out = this->outputBuffer[this->bufferInUse];
    if( this->bufferInUse == 0 ) {
        this->bufferInUse = 1;
    } else {
        this->bufferInUse = 0;
    }
    this->currentBufferSize = 0;
    this->overload = false;
    this->outputBuffer[this->bufferInUse][0]=0;

    return out;
}


/** 
 * Vypise obsah bufferu do streamu
 */
void AsyncLogger::dumpTo( Stream * out )
{
  bool overload = this->overload;
  char * p = this->getOutputBuffer();
  if( p!=NULL ) {
    out->print( p );
    if( overload ) {
      out->println( "(LOG PREPLNEN, cast ztracena)");
    }
  }
}
