// ---------- Wizard helpers
void waitForEnter(const char* prompt) {
  Serial.println(prompt);
  Serial.println("(Press ENTER when ready)");
  // Flush any prior input
  while (Serial.available()) Serial.read();
  // Wait for newline
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
    }
  }
}

float readNumberFromSerial(const char* prompt) {
  Serial.println(prompt);
  Serial.println("Type number then press ENTER:");
  String s;
  // Clear buffer
  while (Serial.available()) Serial.read();
  while (true) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (s.length() > 0) {
          return s.toFloat();
        }
      } else {
        s += c;
      }
    }
  }
}


void doTare() {
  Serial.println("Taring empty plate... keep the plate clear and steady.");
  delay(1000);
  tare1 = readRawAvg(hx1);
  tare2 = readRawAvg(hx2);
  tare3 = readRawAvg(hx3);
  tare4 = readRawAvg(hx4);
  Serial.print("Tare raw: ");
  Serial.print(tare1); Serial.print(", ");
  Serial.print(tare2); Serial.print(", ");
  Serial.print(tare3); Serial.print(", ");
  Serial.println(tare4);
}


// ---------- Wizard main
void runWizard() {
  Serial.println();
  Serial.println("========== CALIBRATION WIZARD ==========");
  Serial.println("This will tare the empty plate, then calibrate F1..F4 using a known mass.");
  Serial.println("Positions: F1=front-left, F2=front-right, F3=back-left, F4=back-right.");
  Serial.println();

  waitForEnter("Make sure the plate is EMPTY.");
  doTare();

  float known = readNumberFromSerial("Enter the known mass in grams (e.g., 1000):");
  Serial.print("Using known mass: "); Serial.print(known, 2); Serial.println(" g");

  cornerCal("F1 (front-left)",  hx1, tare1, scale1, known);
  cornerCal("F2 (front-right)", hx2, tare2, scale2, known);
  cornerCal("F3 (back-left)",   hx3, tare3, scale3, known);
  cornerCal("F4 (back-right)",  hx4, tare4, scale4, known);

  Serial.println("Saving calibration to EEPROM...");
  saveCalToEEPROM();
  Serial.println("Saved. Wizard complete!");
  Serial.println("========================================");
}

