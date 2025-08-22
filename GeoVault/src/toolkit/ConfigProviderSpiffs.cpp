#include "ConfigProviderSpiffs.h"

#include <Arduino.h>
#include <SPIFFS.h>

#define CONFIG_FILE_NAME (char*)"/app.config"

ConfigProviderSpiffs::ConfigProviderSpiffs(LoggerInterface *logger, BasicConfig *config, AppState * appState )
{
    this->logger = logger;
    this->config = config;
    this->appState = appState;
}

void ConfigProviderSpiffs::loadConfig()
{
    const char * fileName = CONFIG_FILE_NAME;
   
    if (! SPIFFS.exists( fileName )) {
        this->logger->log("Config: config file not found (yet?): %s", fileName);
        return;
    }
    
    File configFile = SPIFFS.open( fileName, "r");
    if( !configFile) {
        this->logger->log("Config: problem opening config file: %s", fileName);
        this->appState->setProblem( ERROR, "Config: je problem s otevrenim existujiciho konfiguracniho souboru!" );
        return; 
    }

    // Allocate a buffer to store contents of the file.
    size_t size = configFile.size();
    char * buf = (char*)malloc( size+1 );
    configFile.readBytes(buf, size);
    buf[size] = 0;

    this->config->parseFromString( buf );
    free( buf );

    this->logger->log("Config: loaded from %s", CONFIG_FILE_NAME);
}

void ConfigProviderSpiffs::openFsAndLoadConfig()
{
    // true na ESP32 = format on fail. Bez toho neprobehne prvotni incializace u ESP-32.
    if (! SPIFFS.begin(true) ) {
        this->logger->log("Config: failed to mount FS: nastavte rozdeleni flash, aby tam bylo alespon kus SPIFS");
        this->appState->setProblem( ERROR, "Config: nelze otevrit filesystem, je nejaky SPIFS?" );
        return;
    }

    this->loadConfig();
}

void ConfigProviderSpiffs::saveConfig()
{
    this->logger->log("Config: saving config");

    if( !this->config->isDirty() ) {
        this->logger->log("Config: not dirty, not saving");
        return;
    }
    
    File configFile = SPIFFS.open( CONFIG_FILE_NAME, "w");
    if (!configFile) {
        this->logger->log("Config: failed to write config file");
        this->appState->setProblem( ERROR, "Config: nepodarilo se zapsat konfiguraci" );
    } else {
        this->config->printTo(configFile);
        configFile.close();
        this->logger->log("Config: config saved to %s", CONFIG_FILE_NAME );
    }
}
