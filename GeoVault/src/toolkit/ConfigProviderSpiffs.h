#ifndef _CONFIG_PROVIDER__SPIFFS_H
#define _CONFIG_PROVIDER__SPIFFS_H

/**
 * Zajišťuje načtení konfiguračního souboru z filesystému a jeho uložení na filesystém.
 * 
 * Pozor, využívá AppState pro hlášení případného problému.
 */

#include "../logging/LoggerInterface.h"
#include "BasicConfig.h"
#include "../../AppState.h"


class ConfigProviderSpiffs 
{
    public:
        ConfigProviderSpiffs( LoggerInterface * logger, BasicConfig * config, AppState * appState );

        /**
         * Otevre filesystem a nacte config.
         * Pokud jde o prvni spusteni, udela formatovani FS, to muze i 30 sekund trvat.
         */
        void openFsAndLoadConfig();

        /**
         * Nacte config; pocita s tim, ze filesystem je jiz otevren.
         */
        void loadConfig();

        /**
         * Ulozi config. Pocita s tim, ze filesystem je jiz otevren.
         */
        void saveConfig();

    private:
        LoggerInterface * logger;
        BasicConfig * config;
        AppState * appState;

};

#endif