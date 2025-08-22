#include "BasicConfig.h"

#define CFG_NAME_MAX 30
#define CFG_VALUE_MAX 512

BasicConfig::BasicConfig()
{
    this->fields = (char**) malloc( this->size * sizeof(char *) );
    this->values = (char**) malloc( this->size * sizeof(char *) );
    this->length = 0;
    this->dirty = false;
}

BasicConfig::~BasicConfig()
{
    for( int i = 0; i<this->length; i++ ) {
        free( this->fields[i] );
        free( this->values[i] );
    }
    free(this->fields);
    free(this->values);
}


/**
 * Pokud je jiz pole zaplneno, prodlouzi ho.
 */ 
void BasicConfig::reserveSize()
{
    if( this->length >= this->size ) {
        this->size = this->size * 2;
        this->fields = (char**)realloc( this->fields, this->size * sizeof(char *) );
        this->values = (char**)realloc( this->values, this->size * sizeof(char *) );
    }
}

void trimRight( char * text )
{
    int n = strlen(text) - 1;
    while( n>=0 ) {
        if( text[n]==' ' ) {
            text[n] = 0;
        } else {
            break;
        }
        n--;
    }
}



void BasicConfig::zapisVystup( char * name, int namePos, char * value, int valuePos )
{
    this->reserveSize();

    name[namePos] = 0;
    trimRight( name );
    value[valuePos] = 0;
    trimRight( value );

    this->values[this->length] = (char*)malloc( strlen(value)+1 );
    strcpy( this->values[this->length], value );

    this->fields[this->length] = (char*)malloc( strlen(name)+1 );
    strcpy( this->fields[this->length], name );

    this->length++;
}



void BasicConfig::parseFromString(  char * input )
{
    this->dirty = false;

    char * p = input;
    char name[CFG_NAME_MAX+1];
    int namePos = 0;
    char value[CFG_VALUE_MAX+1];
    int valuePos = 0;

    /**
     * 0 = zaciname / pred jmenem
     * 1 = ve jmenu
     * 2 = pred hodnotou
     * 3 = v hodnote
     */ 
    int state = 0;

    while( true )
    {
        if( *p==0 ) {
            // konec vstupniho textu
            if( state==3 ) {
                this->zapisVystup( name, namePos, value, valuePos );
            }
            break;
        }
        if( state==0 ) {
            if( *p==' ' || *p==13 || *p==10 || *p==8 ) {
                // nic
            } else {
                state = 1;
                continue; // protoze jinak by se na konci posunulo na dalsi pozici
            }
        } else if( state==1 ) {
            if( *p==13 || *p==10 ) {
                state = 0;
                namePos = 0;
            } else if( *p=='=' )  {
                state = 2;
            } else {
                if( namePos<CFG_NAME_MAX ) {
                    name[namePos++] = *p;
                }
            }
        } else if( state==2 ) {
            if( *p == ' ' ) {
                // nedelame nic - preskakujeme mezery na zacatku
            } else {
                state = 3;
                continue; // protoze jinak by se na konci posunulo na dalsi pozici
            }
        } else if( state==3 ) {
            if( *p==13 || *p==10 ) {
                this->zapisVystup( name, namePos, value, valuePos );
                namePos = 0;
                valuePos = 0;
                state = 0;
            } else {
                if( valuePos < CFG_VALUE_MAX ) {
                    value[valuePos++] = *p;
                }
            }
        }
        p++;
    } // while( true )
}

const char * BasicConfig::getString( const char * fieldName, const char * defaultValue )
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    int i = this->findKey( fieldName );
    if( i!=-1 ) {
        return this->values[i];
    } else {
        return defaultValue;
    }
}

long BasicConfig::getLong( const char * fieldName, long defaultValue )
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    int i = this->findKey( fieldName );
    if( i!=-1 ) {
        return atol(this->values[i]);
    } else {
        return defaultValue;
    }
}

/**
 * Vraci klic v poli nebo -1 
 */ 
int BasicConfig::findKey( const char * fieldName )
{
    for(int i = 0; i < this->length; i++ ) {
        if( strcmp( this->fields[i], fieldName )== 0 ) {
            return i;
        }
    }
    return -1;
}


/**
 * Umi zmenit hodnotu existujiciho klice ci pridat novy
 */
void BasicConfig::setValue( const char * fieldName, const char * value )
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    int i = this->findKey( fieldName );
    if( i!=-1 ) {
        // nalezeno
        free( this->values[i] );
        this->values[i] = (char*)malloc( strlen(value)+1 );
        strcpy( this->values[i], value );
    } else {
        // nenalezena polozka
        this->reserveSize();
        this->fields[this->length] = (char*)malloc( strlen(fieldName)+1 );
        strcpy( this->fields[this->length], fieldName );
        this->values[this->length] = (char*)malloc( strlen(value)+1 );
        strcpy( this->values[this->length], value );
        this->length++;
    }
    this->dirty = true;
}

void BasicConfig::setValue( const char * fieldName, long value )
{
    char buffer[20];
    sprintf( buffer, "%d", value );
    this->setValue( fieldName, buffer );
}


void BasicConfig::printTo( Print& p )
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);
    
    for(int i = 0; i < this->length; i++ ) {
        p.print( this->fields[i] );
        p.print( '=' );
        p.print( this->values[i] );
        p.print( '\n' );
    }
    this->dirty = false;
}

bool BasicConfig::isDirty()
{
    return this->dirty;
}



