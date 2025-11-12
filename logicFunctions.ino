void saveCalToEEPROM()
{
  cal.magic = 0xC0A1B455UL;
  cal.version = 1;
  cal.tare1 = tare1;
  cal.tare2 = tare2;
  cal.tare3 = tare3;
  cal.tare4 = tare4;
  cal.scale1 = scale1;
  cal.scale2 = scale2;
  cal.scale3 = scale3;
  cal.scale4 = scale4;
  cal.checksum = 0;
  cal.checksum = calcChecksum(cal);
  EEPROM.put(0, cal);
}

bool loadCalFromEEPROM()
{
  EEPROM.get(0, cal);
  if (cal.magic != 0xC0A1B455UL || cal.version != 1)
    return false;
  uint16_t chk = cal.checksum;
  cal.checksum = 0;
  if (calcChecksum(cal) != chk)
    return false;

  tare1 = cal.tare1;
  tare2 = cal.tare2;
  tare3 = cal.tare3;
  tare4 = cal.tare4;
  scale1 = cal.scale1;
  scale2 = cal.scale2;
  scale3 = cal.scale3;
  scale4 = cal.scale4;
  return true;
}

long readRawAvg(HX711 &hx)
{
  long sum = 0;
  for (int i = 0; i < AVG_N; i++)
  {
    while (!hx.is_ready())
    { /* wait */
    }
    sum += hx.read();
  }
  return sum / AVG_N;
}

float toGrams(long raw, long tare, float scale)
{
  return (raw - tare) * scale;
}

// ---------- Reading + CoG
Reading getReading()
{
  long r1 = readRawAvg(hx1);
  long r2 = readRawAvg(hx2);
  long r3 = readRawAvg(hx3);
  long r4 = readRawAvg(hx4);

  float F1 = toGrams(r1, tare1, scale1);
  float F2 = toGrams(r2, tare2, scale2);
  float F3 = toGrams(r3, tare3, scale3);
  float F4 = toGrams(r4, tare4, scale4);

  if (F1 < 0)
    F1 = 0;
  if (F2 < 0)
    F2 = 0;
  if (F3 < 0)
    F3 = 0;
  if (F4 < 0)
    F4 = 0;

  float P = F1 + F2 + F3 + F4;
  float x = (P > 1e-3f) ? (W * (F2 + F4) / P) : NAN;
  float y = (P > 1e-3f) ? (D * (F3 + F4) / P) : NAN;

  return {F1, F2, F3, F4, P, x, y};
}

void cornerCal(const char *name, HX711 &hx, long &tare, float &scale, float known_g)
{
  waitForEnter(String("Place the known mass on corner ").concat(name));

  long r = readRawAvg(hx);
  long delta = r - tare;
  if (delta == 0)
  {
    delay(100);
    Serial.println("Delta=0! Try again or check wiring. Setting temporary scale=1.");
    scale = 1.0f;
  }
  else
  {
    scale = known_g / (float)delta; // grams per raw count
  }
  Serial.print("Corner ");
  Serial.print(name);
  Serial.print(" raw delta = ");
  Serial.print(delta);
  Serial.print(" -> scale = ");
  Serial.println(scale, 8);
  waitForEnter("Remove the mass from the plate");
}

void displayPostitionInTerminalAsGrid(float W, float D)
{
  Reading r = getReading();
  Serial.print("Position (x,y): ");
  Serial.print(r.x, 1);
  Serial.print(" mm, ");
  Serial.print(r.y, 1);
  Serial.println(" mm");

  const int gridWidth = int(W) / 10;
  const int gridHeight = int(D) / 10;
  char productGrid[gridWidth][gridHeight];

  for (int i = 0; i < gridHeight; i++)
  {
    for (int j = 0; j < gridWidth; j++)
    {
      productGrid[i][j] = '.';
    }
  }

  if (!isnan(r.x) && !isnan(r.y))
  {
    int gridX = constrain(static_cast<int>((r.x / W) * gridWidth), 0, gridWidth - 1);
    int gridY = constrain(static_cast<int>((r.y / D) * gridHeight), 0, gridHeight - 1);
    productGrid[gridY][gridX] = 'X'; // Mark position with 'X'
  }

  Serial.println("Weight Position Grid:");
  for (int i = 0; i < gridHeight; i++)
  {
    for (int j = 0; j < gridWidth; j++)
    {
      Serial.print(productGrid[i][j]);
    }
    Serial.println();
  }

  // Get current reading
  Reading cur = getReading();

  // Simple stability detection
  static Reading last = cur;
  static unsigned long lastChange = millis();

  float dP = fabs(cur.P - last.P);
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

  // Display current weight
  Serial.print("P=");
  Serial.print(cur.P, 1);
  Serial.println("g");
}

void matrixGrid(float W, float D)
{
  const int gridWidth = int(W);
  const int gridHeight = int(D);
  // im going to create a grid of three sections.
  // it is going to be a 3x3 grid.
  // if weight is in a section it will show which section it is in.

  Reading r = getReading();

  if (!isnan(r.x) && !isnan(r.y))
  {
    int sectionX = (r.x < W / 3) ? 0 : (r.x < 2 * W / 3) ? 1
                                                         : 2;
    int sectionY = (r.y < D / 3) ? 0 : (r.y < 2 * D / 3) ? 1
                                                         : 2;
    Serial.print("POSITION: ");
    Serial.println(productGrid[sectionY][sectionX]);
  }
  else
  {
    Serial.println(" Position unknown");
  }
}

void findRemovedItemPosition(float dTotal)
{
  if (fabs(dTotal) >= CHANGE_THRESH_G)
  {
    float dF1 = lastStable.F1 - before.F1;
    float dF2 = lastStable.F2 - before.F2;
    float dF3 = lastStable.F3 - before.F3;
    float dF4 = lastStable.F4 - before.F4;

    float dMx = W * (dF2 + dF4);
    float dMy = D * (dF3 + dF4);

    float x_event = dMx / dTotal;
    float y_event = dMy / dTotal;

    // Map event position to grid
    int sectionX = (x_event < W / 3) ? 0 : (x_event < 2 * W / 3) ? 1
                                                                 : 2;
    int sectionY = (y_event < D / 3) ? 0 : (y_event < 2 * D / 3) ? 1
                                                                 : 2;

    // Serial.println("----- EVENT -----");
    // Serial.print("Diff mass (g): ");
    // Serial.println(dTotal, 1);
    // Serial.print("Event x (mm):  ");
    // Serial.println(x_event, 1);
    // Serial.print("Event y (mm):  ");
    // Serial.println(y_event, 1);
    // Serial.println(dTotal < 0 ? "Removal" : "Addition");
    // Serial.print("Event position: ");
    // Serial.println(productGrid[sectionY][sectionX]);
    // Serial.println("-----------------");

    // output as json stringified formar
    Serial.print("{\"grms\":");
    Serial.print(dTotal, 1);
    Serial.print(",\"prod\":\"");
    Serial.print(productGrid[sectionY][sectionX]);
    Serial.print("\",\"action\":\"");
    Serial.print(dTotal < 0 ? "take" : "put");
    Serial.println("\"}");

    // Reset baseline
    before = lastStable;
  }
}