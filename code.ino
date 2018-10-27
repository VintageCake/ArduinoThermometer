/*
   Projektarbete i datorteknik
   Love Samuelsson, Dennis Söderberg
   Programkod till arduino monterat på perfboard
   2018-10-12

   Tar temperatur från en LM35, visar upp det med hjälp av en 7-seg display. Tryckknapp används för att byta mellan nuvarande/min/max temp. Håll i 1s för att återställa min/max.

   (Old bootloader till kines-nanos)
*/

#include <SevSeg.h>
SevSeg sevseg;

unsigned long CM = 1500 + 50; // Ser till att första iterationen av temperatursensorn sker direkt, istället för att vänta 8s.
const unsigned long sendDelay = 30000; // Delay mellan varje avläsning.
const unsigned long updateDelay = 1500;
unsigned long sendTimer;
unsigned long updateTimer;

bool timer1Flag;
bool timer2Flag;

float temp; // Hållvariabel för temperaturen
float maxTemp;
float minTemp;

const byte thermoPin = 0; //Analog pin connected to LM35 (A0).
const byte ledPins[] = {17, 16, 15}; // R - G - B pins

char tempChar[4];
const float referenceVoltage = 1.1; // Referensspänning som används senare i en beräkning

bool resetFlag = true; // Flagga för reset, (true för att sätta min/maxvärden i början av programmet)
bool sendFlag = false;

// Button
const byte buttonPin = 19;
byte btnC;

unsigned long lastDebounceTime;
const unsigned long debounceDelay = 50;
unsigned long btnTimer;
const unsigned long btnHoldTimer = 1000; // Antal ms som knappen måste vara nedhållen för att återställa min/max.
byte buttonState;
bool buttonPressed;
byte lastButtonState = HIGH;
bool btnFlag;
// Button


// Sevseg
const byte numDigits = 4; // Antal siffror på displayen
const byte digitalPins[] = {2, 3, 4, 5}; // Segmentens katod-pinnar. Segment 1 = D2, S2 = D3... osv.
const byte segmentPins[] = {13, 11, 7, 9, 10, 12, 6, 8}; // Pins för varje del av segment, A,B,C,D... D13 = dot, kolla datasheet vid ändring. --> https://www.electrokit.com/uploads/productfile/41013/1LEDYELCC.pdf
// Sevseg


void setup()
{
  Serial.begin(9600);

  for (int i = 0; i < sizeof(digitalPins); i++) { // Initiering av digitalPins
    pinMode(digitalPins[i], OUTPUT);
  }
  for (int i = 0; i < sizeof(segmentPins); i++) { // Initiering av segmentPins
    pinMode(segmentPins[i], OUTPUT);
  }
  for (int i = 0; i < sizeof(ledPins); i++) { // Initering av led-pins
    pinMode(ledPins[i], OUTPUT);
  }
  pinMode(buttonPin, INPUT_PULLUP); // Sparar en resistor vid användning av INPUT_PULLUP

  // For more information see: http://arduino.cc/en/Reference/AnalogReference
  analogReference(INTERNAL); // LM35 spottar ut som max 1v, därför om man sätter ner den interna analoga referensen så kan man få en högre upplösning på sin mätning. Man kan ej mäta mer än 1.1v om man sätter analogReference till internal. --> https://playground.arduino.cc/Main/LM35HigherResolution

  sevseg.begin(COMMON_CATHODE, numDigits, digitalPins, segmentPins); // Definierar vilken typ av 7seg display som används, samt vilka pins de olika karaktärerna finns på.
  sevseg.setBrightness(70); // Ändrar ljusstrykan.
}

void loop() {
  /// Temperatur ( Tar fram temperaturen, tar fram vad som ska visas på displayen. )
  if ((CM - updateTimer) > updateDelay) { // Uppdaterar 'temp' vid ett visst intervall.
    temp = temp_sense(); // Hämtar temperatur från temperatur-funktionen.

    if (temp > maxTemp) { // Uppdaterar min/max.
      maxTemp = temp;
    }
    else if (temp < minTemp) {
      minTemp = temp;
    }
    timer2Flag = false; // Flagga som gör att timern börjar om igen.
  }

  if (btnC == 0) { // Ifsats som hanterar vilken temperatur som ska visas till displayen. (Kan optimeras med flaggor, som gör att de dtostrf inte körs konstant - onödigt då displayen ser korrekt ut.)
    set_temp(temp); // Omvandling till en char-array. Ser till att displayen visar börjar med den första siffran i det första segmentet.
    light_RGB(LOW, LOW, LOW);
  }
  else if (btnC == 1) {
    set_temp(minTemp);
    light_RGB(HIGH, LOW, LOW); // RGB, R = on, other = off
  }
  else if (btnC == 2) {
    set_temp(maxTemp);
    light_RGB(LOW, HIGH, LOW); // G = on
  }
  ///

  checkSerial(); // Void som hanterar allt som har med det seriella att göra.

  /// Start av timers
  if (timer1Flag == false) {
    sendTimer = CM;
    timer1Flag = true;
  }
  if (timer2Flag == false) {
    updateTimer = CM;
    timer2Flag = true;
  }
  ///

  /// Knappkod
  checkButton();
  ///

  /// Reset ( Deteketerar ett långt knapptryck, samt resettar min/max när ett långt knapptryck kännes. )
  if (buttonPressed == 1) {
    if (btnFlag == false) {
      btnTimer = CM;
      btnFlag = true;
    }
    else if ((CM - btnTimer) > btnHoldTimer && btnFlag == 1) {
      resetFlag = true;

      for (int i = 0; i < 2; i++) {
        light_RGB(LOW, LOW, HIGH);
        delay(50);
        light_RGB(LOW, LOW, LOW);
        delay(50);
      }
      btnFlag = false;
      btnC = 0;
    }
  }
  else btnFlag = false;

  if (resetFlag == true) {
    resetFlag = false;
    minmax_rst();
  }
  ///

  refresh();   // Uppdaterar display och nuvarande timer
}

double temp_sense() { // Returnerar ett celciusvärde genom att läsa en LM35.
  signed long reading = 0;
  for (int i = 0; i < 10; i++) {
    reading += analogRead(thermoPin);
    sevseg.refreshDisplay();
  }
  return (reading / 10 * (referenceVoltage / 1023)) * 100; // Reading/10 = ADC-v 0-1023, *(referenceVoltage/1023) = spänning i volt * 100 = spänning i 10^-2 volt, dvs 1 = 10mV. Enligt LM35 så är 1C = 10mV.
}

void checkSerial() {
  if (((CM - sendTimer) > sendDelay) && sendFlag == true) { /// Ifsats som kontrollerar när arduinon skickar temperatur-data till seriell
    Serial.print(temp, 1);
    Serial.println(" C");
    timer1Flag = false; // Resettar en timer
  }
  if (Serial.available()) { // En simpel ifsats som funkar som en enkel handskakning, om ett "T" skickas till arduinon genom den seriella porten, så kommer arduinon skicka tillbaka ett utropstecken.
    if (Serial.read() == 'T') {
      Serial.println("!");
      sendFlag = true; // Flagga som signalerar att det är dags att börja skicka temperatur-data.
    }
  }
}

void checkButton() {
  byte btnReading = digitalRead(buttonPin);
  if (btnReading != lastButtonState) {
    lastDebounceTime = CM;
  }
  if ((CM - lastDebounceTime) > debounceDelay) { // Button debounce, praktiskt taget en kopia av exemplet "Debounce"
    if (btnReading != buttonState) { //När en ändring sker i knappen, fortsätt
      buttonState = btnReading;
      if (buttonState == LOW) { // Om knäppen == HIGH, fortsätt
        btnC = (btnC + 1) % 3;
        buttonPressed = true;
      }
      else buttonPressed = false;
    }
  }
  lastButtonState = btnReading;
}

void light_RGB(int r, int g, int b) { // Funktion som tänder vissa led-lampor.
  digitalWrite(ledPins[0], r);
  digitalWrite(ledPins[1], g);
  digitalWrite(ledPins[2], b);
}

void refresh() {
  sevseg.setChars(tempChar); // Gör så att sevseg-funktionen håller tempchar vid varje display-refresh.
  sevseg.refreshDisplay();
  CM = millis();
}

void minmax_rst() { // Resettar min/max till akutell temperatur
  minTemp = temp;
  maxTemp = temp;
}

void set_temp(float x) { // dtostrf i en if( x == y ) resettade Y när y == 2, för någon anledning. Att stoppa in dtostrf i en ytterligare funktion löste problemet.
  dtostrf(x, 3, 1, tempChar);
}
