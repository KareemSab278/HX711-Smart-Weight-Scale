#include <HX711.h>
#include <EEPROM.h>

// FUNCTION DECLARATIONS
void runWizard();
void waitForEnter(const char *prompt);
void doTare();
float readNumberFromSerial(const char *prompt);
void saveCalToEEPROM();
bool loadCalFromEEPROM();
long readRawAvg(HX711 &hx);
float toGrams(long raw, long tare, float scale);
void cornerCal(const char *name, HX711 &hx, long &tare, float &scale, float known_g);
void getWeight();
void displayPostitionInTerminalAsGrid(float W, float D);
void matrixGrid(float W, float D);
void findRemovedItemPosition(float dTotal);

// ========== CONSTANTS ==========
// ---------- Plate geometry (mm)
// Plate size (mm)
const float W = 230.0f;
const float D = 150.0f;

// Grid sections
const char *productGrid[3][3] = {
    {"G7", "H8", "I9"},
    {"D4", "E5", "F6"},
    {"A1", "B2", "C3"}};

// Pinout: share SCK, separate DOUT
const int PIN_SCK = 3;
const int PIN_DOUT3 = 5; // F3 back-left
const int PIN_DOUT1 = 6; // F1 front-left
const int PIN_DOUT2 = 7; // F2 front-right
const int PIN_DOUT4 = 4; // F4 back-right

HX711 hx1, hx2, hx3, hx4;

// ---------- Averaging / debounce
const int AVG_N = 1;
const float CHANGE_THRESH_G = 40.0f;
const float STABLE_DB_G = 5.0f; // “no change” band for stability
const unsigned long SETTLE_TIME_MS = 800;

// ---------- Persistent calibration storage
struct CalData
{
  uint32_t magic;                       // 0xC0A1B455
  uint16_t version;                     // 1
  long tare1, tare2, tare3, tare4;      // raw counts
  float scale1, scale2, scale3, scale4; // g per raw count
  uint16_t checksum;                    // simple sum
};

CalData cal;

// Defaults (in case no EEPROM data yet)
long tare1 = 0, tare2 = 0, tare3 = 0, tare4 = 0;
float scale1 = 1.0f, scale2 = 1.0f, scale3 = 1.0f, scale4 = 1.0f;

// ---------- Types
struct Reading
{
  float F1, F2, F3, F4; // grams
  float P;              // total grams
  float x, y;           // mm
};

// ---------- Arduino setup/loop
void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  hx1.begin(PIN_DOUT1, PIN_SCK);
  hx2.begin(PIN_DOUT2, PIN_SCK);
  hx3.begin(PIN_DOUT3, PIN_SCK);
  hx4.begin(PIN_DOUT4, PIN_SCK);

  Serial.println("\nScale starting...");

  bool ok = loadCalFromEEPROM();
  if (ok)
  {
    Serial.println("Loaded calibration from EEPROM.");
  }
  else
  {
    Serial.println("No valid calibration found. Running wizard...");
    runWizard();
  }

  Serial.println("Ready. Commands: 'w' = run wizard, 't' = re-tare, 'p' = print cal.");
}

Reading lastStable = {0, 0, 0, 0, 0, NAN, NAN};
Reading before = {0, 0, 0, 0, 0, NAN, NAN};
bool haveBefore = false;

void printCal()
{
  Serial.println("Current calibration:");
  Serial.print("Tare: ");
  Serial.print(tare1);
  Serial.print(", ");
  Serial.print(tare2);
  Serial.print(", ");
  Serial.print(tare3);
  Serial.print(", ");
  Serial.println(tare4);
  Serial.print("Scale: ");
  Serial.print(scale1, 8);
  Serial.print(", ");
  Serial.print(scale2, 8);
  Serial.print(", ");
  Serial.print(scale3, 8);
  Serial.print(", ");
  Serial.println(scale4, 8);
}

// Event stability tracking
void loop()
{
  // Commands
  if (Serial.available())
  {
    char c = Serial.read();
    if (c == 'w' || c == 'W')
    {
      runWizard();
      haveBefore = false; // reset event baseline
    }
    else if (c == 't' || c == 'T')
    {
      waitForEnter("Re-tare: ensure plate EMPTY.");
      doTare();
      saveCalToEEPROM();
      Serial.println("Re-tare done and saved.");
      haveBefore = false;
    }
    else if (c == 'p' || c == 'P')
    {
      printCal();
    }
  }

  Reading cur = getReading();
  static Reading last = cur;
  static unsigned long lastChange = millis();

  float dP = fabs(cur.P - last.P);

  // Only reset baseline if weight changes by more than 20 grams
  if (dP > 20.0f)
  {
    haveBefore = false;
  }

  if (dP > STABLE_DB_G)
  {
    lastChange = millis();
  }
  if (millis() - lastChange > SETTLE_TIME_MS)
  {
    lastStable = cur;
  }
  last = cur;

  if (!haveBefore)
  {
    before = lastStable;
    haveBefore = true;
  }

  float dTotal = lastStable.P - before.P;

  findRemovedItemPosition(dTotal);

  // Telemetry every ~200 ms
  static unsigned long t0 = 0;

  // if (millis() - t0 > 1000)
  // {
  //   t0 = millis();
  //   matrixGrid(W, D);
  //   Serial.print("P=");
  //   Serial.print(cur.P, 1);
  //   Serial.print(" g  x=");
  //   Serial.print(cur.x, 1);
  //   Serial.print(" mm  y="); 
  //   Serial.print(cur.y, 1);
  //   Serial.println(" mm");
  // }
}