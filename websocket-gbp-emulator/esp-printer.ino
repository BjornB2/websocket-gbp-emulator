#define MISO 12
#define MOSI 13
#define SCLK 14

void processByte(byte data);
void storeData(byte *image_data);
unsigned int nextFreeFileIndex();

byte clock_count = 0x00;
byte current_byte = 0x00;
uint32_t packet_count = 0x00;
uint32_t packet_length = 0x00;
byte current_packet_type = 0x00;
bool printed = false;
byte inquiry_count = 0x00;
byte image_data[11520] = {};
uint32_t img_index = 0;

unsigned long lastByteReceived = 0;
unsigned long blinkClockHit = 0;
bool blinkCycle = false;

unsigned int freeFileIndex = 0;

void ICACHE_RAM_ATTR gbClockHit() {
  if (digitalRead(MOSI) == HIGH) {
    current_byte |= 0x01;
  }

  if (packet_count == (packet_length - 3)) {
    if (clock_count == 7) {
      digitalWrite(MISO, HIGH);
    }
  }
  if (packet_count == (packet_length - 2)) {
    if (clock_count == 0 || clock_count == 7) {
      digitalWrite(MISO, LOW);
    } else if (clock_count == 6) {
      digitalWrite(MISO, HIGH);
    }
  }

  if (clock_count == 7) {
    processByte(current_byte);
    clock_count = 0;
    current_byte = 0x00;
  } else {
    current_byte = current_byte << 1;
    clock_count++;
  }
}

void processByte(byte data) {
  if (packet_count == 2) { //command type
    current_packet_type = data;
    switch (data) {
      case 0x04:
      packet_length = 0x28A; // 650 bytes
      break;

      case 0x02:
      packet_length = 0x0E; // 14 bytes
      break;

      default:
      packet_length = 0x0A; // 10 bytes
      break;
    }
  }

  // Handles that special empty body data packet
  if ((current_packet_type == 0x04) && (packet_count == 4) && (data == 0x00)) {
    packet_length = 0x0A;
  }

  if ((current_packet_type == 0x04) && (packet_count >= 6) && (packet_count <= packet_length - 5)) {
    image_data[img_index++] = data;
  }

  if (current_packet_type == 0x02) {
    printed = true;
  }

  if (printed && (packet_count == 2) && (data == 0x0F)) {
    inquiry_count++;
  }

  if (packet_count == (packet_length - 1)) {
    packet_count = 0x00;
    if (inquiry_count == 4) {
      storeData(image_data);
    }
  } else {
    packet_count++;
  }

  // Blink while receiving data
  lastByteReceived = millis();
  if (blinkClockHit < lastByteReceived) {
    blinkClockHit = lastByteReceived + 50;
    blinkCycle = !blinkCycle;
    digitalWrite(LED_BLINK_PIN, blinkCycle);
  }
}

unsigned int nextFreeFileIndex() {
  for (int i = 1; i < 250; i++) {
    char path[31];
    sprintf(path, "/d/%05d.bin", i);
    if (!LittleFS.exists(path)) {
      return i + 1;
    }
  }
}

void resetValues() {
  clock_count = 0x00;
  current_byte = 0x00;
  packet_count = 0x00;
  packet_length = 0x00;
  current_packet_type = 0x00;
  printed = false;
  inquiry_count = 0x00;
  img_index = 0x00;

  lastByteReceived = 0;

  // Turn LED ON
  digitalWrite(LED_BLINK_PIN, false);
  Serial.println("Printer ready.");
}

void storeData(byte *image_data) {
  detachInterrupt(SCLK);

  unsigned long perf = millis();
  char fileName[31];
  sprintf(fileName, "/d/%05d.bin", freeFileIndex);

  File file = LittleFS.open(fileName, "w");

  if (!file) {
    Serial.println("file creation failed");
  }

  file.write("GB-BIN01", 8);
  file.write(image_data, img_index);
  file.close();

  perf = millis() - perf;
  Serial.printf("File /d/%05d.bin written in %lums\n", freeFileIndex, perf);

  freeFileIndex++;
  resetValues();

  attachInterrupt(SCLK, gbClockHit, RISING);
}

void espprinter_setup() {
  // Setup ports
  pinMode(MISO, OUTPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCLK, INPUT);

  freeFileIndex = nextFreeFileIndex();
  Serial.printf("Next file: /d/%05d.bin\n", freeFileIndex);

  lastByteReceived = millis();
  resetValues();

  // Setup Clock Interrupt
  attachInterrupt(SCLK, gbClockHit, RISING);
}

void espprinter_loop() {
  if (lastByteReceived != 0 && lastByteReceived + 500 < millis()) {
    resetValues();
  }
}
