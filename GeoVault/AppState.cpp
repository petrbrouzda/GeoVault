#include <string.h>
#include <Arduino.h>

#include "AppState.h"

/*
viz popis v AppState.c
*/

AppState::AppState()
{
    this->clearProblem();

    // --- per-app ----

}

void AppState::setProblem(ProblemLevel state, const char *text)
{
    this->globalState = state;
    strncpy( this->problemDesc, text, STATUS_TEXT_MAX_LEN );
    this->problemDesc[STATUS_TEXT_MAX_LEN] = 0;
    this->problemTime = millis();
}

void AppState::clearProblem()
{
    this->globalState = NONE;
    this->problemDesc[0] = 0;
    this->problemTime = 0;
}

bool AppState::isProblem()
{
    return this->globalState!=NONE;
}
