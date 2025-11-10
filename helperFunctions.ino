uint16_t calcChecksum(const CalData &c) {
  // Sum of bytes except checksum field itself
  const uint8_t *p = reinterpret_cast<const uint8_t*>(&c);
  size_t n = sizeof(CalData) - sizeof(uint16_t);
  uint32_t s = 0;
  for (size_t i=0;i<n;i++) s += p[i];
  return (uint16_t)(s & 0xFFFF);
}

void getWeight() {
  Reading r = getReading();
  Serial.print("Weight: ");
  Serial.print(r.P,1);
  Serial.println(" g");
}

