#include "LoggerInterface.h"


LoggerInterface::LoggerInterface()
{
    this->printPos = 0;
    this->printed[0] = 0;
}

size_t LoggerInterface::write(uint8_t ch )
{
    if( this->printPos >= LOG_PRINT_BUFFER_SIZE ) return 1;
    
    this->printed[ this->printPos++ ] = (char)ch;
    this->printed[ this->printPos ] = 0;
    return 1;
    
}

size_t LoggerInterface::write(const uint8_t* buffer, size_t size)
{
    int remaining = LOG_PRINT_BUFFER_SIZE - this->printPos - 2;
    int toCopy = size > remaining ? remaining : size; 
    if( toCopy > (LOG_PRINT_BUFFER_SIZE-this->printPos+1) ) {
        return size;
    }
    strncpy( (char*)(this->printed + this->printPos), (const char*)buffer, toCopy );
    this->printPos = this->printPos + toCopy;   
    this->printed[ this->printPos ] = 0;
    return size;
}