#ifndef ___APP_STATUS_H_
#define ___APP_STATUS_H_



/*
Sdílený stav celé aplikace.
Používá se, aby asynchronní akce ve webserveru měly přístup ke všemu, co potřebují.
*/

#define STATUS_TEXT_MAX_LEN 255


enum ProblemLevel {
    NONE,
    WARNING,
    ERROR
};

class AppState
{
    public:

        AppState();

        //--- evidence problemu, common funkcionalita

        ProblemLevel globalState;
        char problemDesc[STATUS_TEXT_MAX_LEN + 5];
        long problemTime;

        void setProblem( ProblemLevel appState, const char * text );
        void clearProblem();

        bool isProblem();

        //---- per-app funkcionalita

        /** sirka */
        double targetLat;
        /** delka */
        double targetLon;
        /** max. odchylka od cile pro otevreni */
        long accuracy;


        /**
         * 0 = Nemame spojeni s GPS
         * 1 = Zadne satelity
         * 2 = Nejake satelity, zadny fix
         * 10 = Mame pozici, ale malo druzic
         * 11 = Mame pozici
         * 20 = V cili
         */
        int stav = 0;

        int vzdalenostMetru = -1;

        /** stav dveri */
        bool dvereOtevreny = false;

        /** true = otevri dvere */
        bool servoRequestWaiting = false;
        /** 
         * 0 = nic
         * 1 = otevri
         * 2 = zavri
         */
        int servoRequest = 0;


        bool debugMode = false;

        bool servoDirectCommand = false;
        int servo1Direct = 0;
        int servo2Direct = 0;


        double accuVoltage = -1.0;
        int accuPercent = 0;

        /** ktery text se nyni pouziva */
        int textKey = 0;

};

#endif