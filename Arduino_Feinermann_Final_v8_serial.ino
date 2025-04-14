#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <avr/wdt.h> // Für Watchdog-Reset

// *************************************************************
// ********************** Display-Setup ************************
// *************************************************************
MCUFRIEND_kbv tft; // Objekt für das TFT-Display
// Auflösung: 480 x 320

// *************************************************************
// ********************** Pin-Definitionen **********************
// *************************************************************
const int valve1 = 31;
const int valve2 = 33;
const int valve3 = 35;
const int valve4 = 37;

const int pressureSensorPin = A8; 
const int switchPin3       = 50;  // Drehschalter-Kontakt "3" (Pullup)
const int switchPin1       = 52;  // Drehschalter-Kontakt "1" (Pullup)
const int confirmButton    = 46;  // Grüner Taster (NO, Pullup)
const int emergencyButton  = 48;  // Roter Not-Aus (NC, Pullup)

// *************************************************************
// ********************** Globale Variablen ********************
// *************************************************************
bool  processRunning  = false;  
int   mode            = 0;      
int   lastMode        = -1;     

// *************************************************************
// ************** Funktions-Prototypen (Übersicht) *************
// *************************************************************
void modeSelection();
void startMode1();
void startMode2();
void startMode3();

void emergencyStop();
void displayEmergencyScreen();

void showStartupScreen();
void drawModeMenu(int selectedMode);
void updateDisplay(const char* status);

int  readRotarySwitchStable();

// Zum einmaligen Bestätigen für Modus 1
void waitForConfirmReleaseAndPress(const char* infoText);

void closeAllValves();
void openNehmerUeberdruck();
void openNehmerUnterdruck();
void openGeberUnterdruck();
void openEntlueftungUmgebungsdruck();

void softwareReset();

// *************************************************************
// ************************ Setup ******************************
// *************************************************************
void setup() {
    Serial.begin(9600);
    Serial.println("Starte Setup...");

    tft.begin(0x9486);
    tft.setRotation(1); // Querformat

    pinMode(valve1, OUTPUT);
    pinMode(valve2, OUTPUT);
    pinMode(valve3, OUTPUT);
    pinMode(valve4, OUTPUT);

    pinMode(switchPin3,      INPUT_PULLUP);
    pinMode(switchPin1,      INPUT_PULLUP);
    pinMode(confirmButton,   INPUT_PULLUP);
    pinMode(emergencyButton, INPUT_PULLUP);

    closeAllValves();

    showStartupScreen();
    modeSelection();
}

// *************************************************************
// ************************ Loop *******************************
// *************************************************************
void loop() {
    // Roter Not-Aus (NC) -> Pin HIGH = Notfall
    if (digitalRead(emergencyButton) == HIGH) {
        Serial.println("[Loop] Not-Aus gedrückt -> emergencyStop()");
        emergencyStop();
        return;
    }

    // Wenn kein Prozess läuft, gehen wir ins Menü
    if (!processRunning) {
        modeSelection();
    }
}

// *************************************************************
// *************** 1) Startup-Screen (MAGURA) ******************
// *************************************************************
void showStartupScreen() {
    Serial.println("[showStartupScreen] Anzeige Startbildschirm...");

    tft.fillScreen(TFT_YELLOW);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(5);

    // "MAGURA" zentriert
    int startX = 150; 
    int startY = 120;

    tft.setCursor(startX, startY);
    tft.println("MAGURA");
    tft.setTextSize(2);
    tft.setCursor(startX - 100, startY + 60);
    tft.println("Bleed your brake, not your mind!");
    tft.setCursor(20, 300);
    tft.println("Softwareversion_v8");
    delay(3000);
}

// *************************************************************
// ************ 2) Modus-Auswahl (Hauptmenü) *******************
// *************************************************************
void modeSelection() {
    int newMode = readRotarySwitchStable();

    if (newMode != lastMode) {
        drawModeMenu(newMode);
        lastMode = newMode;
    }
    mode = newMode;

    // Einmaliges Drücken -> Modus starten
    if (digitalRead(confirmButton) == LOW) {
        Serial.print("[modeSelection] Bestaetigung fuer Modus ");
        Serial.println(mode);

        switch (mode) {
            case 1: startMode1(); break;
            case 2: startMode2(); break;
            case 3: startMode3(); break;
            default:
                Serial.println("[modeSelection] Unbekannter Modus!");
                break;
        }
    }
}

// *************************************************************
// ******** 2a) Menü grafisch darstellen ***********************
// *************************************************************
void drawModeMenu(int selectedMode) {
    Serial.print("[drawModeMenu] Menu zeichnen, Mode=");
    Serial.println(selectedMode);

    tft.fillScreen(TFT_YELLOW);

    tft.setTextSize(3);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(40, 30);
    tft.println("Moduswahl:");

    int startY = 100;
    int lineSpacing = 40;

    for (int i=1; i<=3; i++) {
        int thisY = startY + (i-1)*lineSpacing;
        if (i == selectedMode) {
            tft.setTextColor(TFT_GREEN);
        } else {
            tft.setTextColor(TFT_BLACK);
        }
        tft.setTextSize(3);
        tft.setCursor(40, thisY);

        switch (i) {
            case 1: tft.println("1. Entlueften"); break;
            case 2: tft.println("2. Absaugen");   break;
            case 3: tft.println("3. Ansaugen");   break;
        }
    }

    // Zusatzhinweise unten
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(20, 280);
    switch (selectedMode) {
        case 1:
            tft.println("Automatischer Modus 3 Minuten");
            break;
        case 2:
            tft.println("Manueller Modus");
            break;
        case 3:
            tft.println("Manueller Modus");
            break;
        default:
            break;
    }
}

// *************************************************************
// ********* 3) ENTLUEFTEN (Mode 1, automatisch) ***************
// *************************************************************
void startMode1() {
    Serial.println("[startMode1] Modus 1: Entlueften (automatisch).");

    // Zeige Anleitung, einmal bestätigen
    tft.fillScreen(TFT_YELLOW);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);

    tft.setCursor(20, 50);
    tft.println("GRUENEN Schlauch mit GEBER verbinden!");
    tft.setCursor(20, 90);
    tft.println("GELBEN Schlauch mit NEHMER verbinden!");
    tft.setCursor(20, 130);
    tft.println("Ventile an den Schlauchen oeffnen!");  
    tft.setCursor(20, 170);
    tft.println("Ventil am NEHMER oeffnen!");       
    tft.setCursor(20, 210);
    tft.println("Druecke GRUENE Taste zum Start...");

    waitForConfirmReleaseAndPress("Entlueften-Begin");

    processRunning = true;

    // Automatischer Ablauf
    struct {
        const char* stepText;
        void (*action)();
        unsigned long duration;
    } steps[] = {
        {"Entlueften Umgebung",   openEntlueftungUmgebungsdruck,  2000},
        {"Nehmer Ueberdruck",     openNehmerUeberdruck,           10000},

        {"Geber Unterdruck",      openGeberUnterdruck,            10000},
        {"Nehmer Ueberdruck",     openNehmerUeberdruck,           10000},
        {"Entlueften Umgebung",   openEntlueftungUmgebungsdruck,  5000},
        {"Nehmer Unterdruck",     openNehmerUnterdruck,           60000},
        {"Entlueften Umgebung",   openEntlueftungUmgebungsdruck,  5000},
        {"Nehmer Ueberdruck",     openNehmerUeberdruck,           5000},
        {"Geber Unterdruck",      openGeberUnterdruck,            10000},
        {"Nehmer Ueberdruck",     openNehmerUeberdruck,           5000},
        {"Entlueften Umgebung",   openEntlueftungUmgebungsdruck,  5000}
    };

    for (auto &s : steps) {
        Serial.print("[startMode1] Schritt: ");
        Serial.print(s.stepText);
        Serial.print(" ");
        Serial.print(s.duration);
        Serial.println("ms");

        updateDisplay(s.stepText);
        s.action();

        unsigned long startT = millis();
        while (millis() - startT < (unsigned long)s.duration) {
            if (digitalRead(emergencyButton) == HIGH) {
                Serial.println("[startMode1] -> Not-Aus!");
                emergencyStop();
                processRunning = false;
                return;
            }
            delay(50);
        }
    }

    closeAllValves();
    processRunning = false;
    Serial.println("[startMode1] Entlueften beendet.");

    // Display löschen (z. B. wieder in Gelb)
    tft.fillScreen(TFT_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(20, 50);
    tft.println("Entlueften abgeschlossen!");
    tft.setCursor(20, 90);
    tft.println("Ventil am Nehmer schliessen!");
    tft.setCursor(20, 130);
    tft.println("Ventile an den Schlaeuchen schliessen");  
    tft.setCursor(20, 170);
    tft.println("Schlaeuche vorsichtig abziehen!");       
    tft.setCursor(20, 210);
    tft.println("EBT Schraube montieren!");
    tft.setCursor(20, 250);
    tft.println("GRUENE Taste fuer Menue!");
        
    // Noch einmal warten, dann zurück
    waitForConfirmReleaseAndPress("Mode 1 abgeschlossen.");
    lastMode = -1;
    modeSelection();
}

// *************************************************************
// ******** 4) ABSAUGEN (Mode 2): Anzeige, dann direkt halten ***
/*************************************************************
  Ablauf:
    1) Einmaliges Drücken im Menü => Anweisung.
    2) Anweisung auf Gelb. Dann Abwarten, bis User
       Taster "gedrückt hält" => Ventil an, "Absaugen...".
    3) Loslassen => Schließen, entlüften, zurück Menü.
*************************************************************/
void startMode2() {
    Serial.println("[startMode2] Modus 2: Absaugen (direkt halten).");

    // Zeige Anweisungen
    tft.fillScreen(TFT_YELLOW);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);

    tft.setCursor(20, 50);
    tft.println("BLAUEN Schlauch mit NEHMER verbinden!");
    tft.setCursor(20, 90);
    tft.println("EBT Schraube herausnehmen!");
    tft.setCursor(20, 130);
    tft.println("Halte GRUENE TASTE fuer Absaugen.");

    // Jetzt warte, bis der User wirklich drückt + hält
    // => Dann Ventil öffnen, "Absaugen" zeigen
    processRunning = true;

    Serial.println("[startMode2] Warte, bis Taster gedrueckt wird (LOW).");
    while (digitalRead(confirmButton) == HIGH) {
        // Not-Aus abfragen
        if (digitalRead(emergencyButton) == HIGH) {
            Serial.println("[startMode2] Not-Aus in Wartephase!");
            emergencyStop();
            processRunning = false;
            return;
        }
        delay(20);
    }

    // Taster jetzt gedrückt => Ventil an
    Serial.println("[startMode2] Taster gedrueckt => Ventil GeberUnterdruck an.");
    openGeberUnterdruck();

    // Solange gedrückt halten -> Absaugen
    while (digitalRead(confirmButton) == LOW) {
        updateDisplay("Absaugen...");
        if (digitalRead(emergencyButton) == HIGH) {
            Serial.println("[startMode2] Not-Aus waehrend Absaugen!");
            emergencyStop();
            processRunning = false;
            return;
        }
        delay(100);
    }

    // Taster los => Beenden
    Serial.println("[startMode2] Taster los -> Ventile zu, entlueften, zurueck Menue.");
    closeAllValves();
    openEntlueftungUmgebungsdruck();
    delay(2000);
    closeAllValves();

    processRunning = false;
    lastMode = -1;
    modeSelection();
}

// *************************************************************
// ******** 5) ANSAUGEN (Mode 3): Anzeige, dann direkt halten ***
/*************************************************************
  Ablauf:
    1) Einmaliges Drücken im Menü => Anweisung Gelb.
    2) User haelt Taster => Ventil an, "Ansaugen...".
    3) Loslassen => Schließen, entlueften, Menu.
*************************************************************/
void startMode3() {
    Serial.println("[startMode3] Modus 3: Ansaugen (direkt halten).");

    // Zeige Anweisungen
    tft.fillScreen(TFT_YELLOW);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);

    tft.setCursor(20, 50);
    tft.println("GELBEN Schlauch mit Gebinde verbinden.");
    tft.setCursor(20, 130);
    tft.println("Halte GRUENE TASTE fuer Ansaugen.");

    processRunning = true;

    Serial.println("[startMode3] Warte, bis Taster gedrueckt wird (LOW).");
    while (digitalRead(confirmButton) == HIGH) {
        if (digitalRead(emergencyButton) == HIGH) {
            Serial.println("[startMode3] Not-Aus in Wartephase!");
            emergencyStop();
            processRunning = false;
            return;
        }
        delay(20);
    }

    // Taste jetzt gedrückt
    Serial.println("[startMode3] Taster gedrueckt => NehmerUnterdruck an.");
    openNehmerUnterdruck();

    // Solange gedrückt => Ansaugen
    while (digitalRead(confirmButton) == LOW) {
        updateDisplay("Ansaugen...");
        if (digitalRead(emergencyButton) == HIGH) {
            Serial.println("[startMode3] Not-Aus waehrend Ansaugen!");
            emergencyStop();
            processRunning = false;
            return;
        }
        delay(100);
    }

    // Loslassen => Ende
    Serial.println("[startMode3] Taster los -> Ventile zu, entlueften, Menue.");
    closeAllValves();
    openEntlueftungUmgebungsdruck();
    delay(2000);
    closeAllValves();

    processRunning = false;
    lastMode = -1;
    modeSelection();
}

// *************************************************************
// ******************* NOTFALL-STOPP ***************************
// *************************************************************
void emergencyStop() {
    Serial.println("[emergencyStop] !!! NOTFALL !!!");
    displayEmergencyScreen(); 
    closeAllValves();

    delay(2000);
    openEntlueftungUmgebungsdruck();
    delay(5000);
    closeAllValves();

    // Warte auf Bestaetigung -> Neustart
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(40, 60);
    tft.println("NOTFALL beendet!");
    tft.setCursor(40, 100);
    tft.println("Druecke GRUEN fuer NEUSTART");

    while (digitalRead(confirmButton) == LOW) {
        delay(20);
    }
    while (digitalRead(confirmButton) == HIGH) {
        delay(20);
    }

    Serial.println("[emergencyStop] Neustart per Watchdog...");
    softwareReset();
}

void displayEmergencyScreen() {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.setCursor(100, 120);
    tft.println("NOTFALLMODUS");
}

// *************************************************************
// ********** Hilfsfunktion: Warte auf einmaliges Drücken *******
// ******** (Nur für Modus 1, entlueften, da autom.) ************
void waitForConfirmReleaseAndPress(const char* infoText) {
    // 1) Falls Taster noch gedrückt -> warte auf Loslassen
    while (digitalRead(confirmButton) == LOW) {
        delay(20);
    }
    // 2) Warte auf Drücken
    while (digitalRead(confirmButton) == HIGH) {
        if (digitalRead(emergencyButton) == HIGH) {
            Serial.println("[waitForConfirmReleaseAndPress] Not-Aus -> emergencyStop()");
            emergencyStop();
            return;
        }
        delay(20);
    }
    // 3) Warte auf Loslassen
    while (digitalRead(confirmButton) == LOW) {
        if (digitalRead(emergencyButton) == HIGH) {
            Serial.println("[waitForConfirmReleaseAndPress] Not-Aus -> emergencyStop()");
            emergencyStop();
            return;
        }
        delay(20);
    }
    Serial.print("[waitForConfirmReleaseAndPress] Bestaetigung abgeschlossen: ");
    Serial.println(infoText);
}

// *************************************************************
// ****************** Display-Aktualisierung *******************
// *************************************************************
void updateDisplay(const char* status) {
    tft.fillScreen(TFT_YELLOW);

    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(20, 40);
    tft.println(status);

    // 1) ADC-Rohwert auslesen
    int rawAdc = analogRead(pressureSensorPin);

    // 2) Spannung berechnen
    float voltage = (rawAdc * 5.0) / 1023.0;

    // 3) Druck in bar berechnen (Sensor 1..5 V -> -1..+1 bar)
    float pressure_bar = 0.5 * voltage - 1.5;

    // 4) Ausgabe auf dem Display
    tft.setCursor(20, 100);
    tft.print("Druck: ");
    tft.print(pressure_bar, 2); // z.B. 2 Nachkommastellen
    tft.println(" bar");

    // Serial-Debug
    Serial.print("[updateDisplay] Status: ");
    Serial.print(status);
    Serial.print(" | ADC: ");
    Serial.print(rawAdc);
    Serial.print(" -> ");
    Serial.print(voltage);
    Serial.print(" V -> ");
    Serial.print(pressure_bar, 2);
    Serial.println(" bar");
}


// *************************************************************
// ********** Rotationsschalter stabil auslesen ***************
// *************************************************************
int readRotarySwitchStable() {
    // INPUT_PULLUP => Pins normal HIGH
    // switchPin1 LOW => Mode=1
    // switchPin3 LOW => Mode=3
    // sonst => Mode=2
    if (digitalRead(switchPin1) == LOW) {
        return 1;
    }
    else if (digitalRead(switchPin3) == LOW) {
        return 3;
    }
    else {
        return 2;
    }
}

// *************************************************************
// ******************** Ventilsteuerungen **********************
// *************************************************************
void closeAllValves() {
    digitalWrite(valve1, LOW);
    digitalWrite(valve2, LOW);
    digitalWrite(valve3, LOW);
    digitalWrite(valve4, LOW);
    Serial.println("[closeAllValves] -> Alle Ventile zu.");
}

void openNehmerUeberdruck() {
    closeAllValves();
    digitalWrite(valve2, HIGH);
    Serial.println("[openNehmerUeberdruck] Ventil2=HIGH");
}

void openNehmerUnterdruck() {
    closeAllValves();
    digitalWrite(valve1, HIGH);
    digitalWrite(valve3, HIGH);
    Serial.println("[openNehmerUnterdruck] Ventil1+3=HIGH");
}

void openGeberUnterdruck() {
    closeAllValves();
    digitalWrite(valve1, HIGH);
    digitalWrite(valve4, HIGH);
    Serial.println("[openGeberUnterdruck] Ventil1+4=HIGH");
}

void openEntlueftungUmgebungsdruck() {
    closeAllValves();
    digitalWrite(valve3, HIGH);
    digitalWrite(valve4, HIGH);
    Serial.println("[openEntlueftungUmgebungsdruck] Ventil3+4=HIGH");
}

// *************************************************************
// ******************** Software-Neustart **********************
// *************************************************************
void softwareReset() {
    Serial.println("[softwareReset] -> Watchdog Reset in 15ms ...");
    wdt_enable(WDTO_15MS);
    for (;;) { /* warte auf Reset */ }
}
