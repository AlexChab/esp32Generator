#include <Arduino.h>

#include "driver/rmt.h"
#include <ESP32Encoder.h>

ESP32Encoder encoder;
const int buttonPin = 5;

const int OUTPUT_PIN = 18;
const rmt_channel_t RMT_CHAN = RMT_CHANNEL_0;

// Set parameters by default
uint32_t pulseUs = 10;
uint32_t pauseMs = 1;
bool inverted = false;
bool isRunning = true;

void setup_rmt() {
  rmt_config_t config;
  config.rmt_mode = RMT_MODE_TX;
  config.channel = RMT_CHAN;
  config.gpio_num = (gpio_num_t)OUTPUT_PIN;
  config.mem_block_num = 1;
  config.clk_div = 1;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_en = false;
  // ALERT :  set start parameters on HIGH level, so the first pulse will be LOW (active) and then return to HIGH (idle)
  config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
  config.tx_config.idle_output_en = true;

  rmt_config(&config);
  rmt_driver_install(RMT_CHAN, 0, 0);
}

void send_pulse() {
  //  printf("Sending pulse: %u us, pause: %u ms, inverted: %s\n", pulseUs, pauseMs, inverted ? "YES" : "NO");

  uint32_t pauseUs = pauseMs * 1000;
  rmt_item32_t item;

  if (inverted) {
    item = (rmt_item32_t){{{
        (uint16_t)(pulseUs & 0x7FFF), 1,  // SET to 1(active pulse)
        (uint16_t)(pauseUs & 0x7FFF), 0   // Set to 0 (return to idle)
    }}};
    rmt_set_idle_level(RMT_CHAN, true, RMT_IDLE_LEVEL_LOW);
  } else {
    item = (rmt_item32_t){{{
        (uint16_t)(pulseUs & 0x7FFF), 0,  // SET to 0 (active pulse)
        (uint16_t)(pauseUs & 0x7FFF), 1   // Set to 1 (return to idle)
    }}};
    rmt_set_idle_level(RMT_CHAN, true, RMT_IDLE_LEVEL_HIGH);
  }

  rmt_write_items(RMT_CHAN, &item, 1, false);
}

void setup() {
  Serial.begin(115200);
  setup_rmt();
  ESP32Encoder::useInternalWeakPullResistors = UP; // Подтяжка к 3.3V
  encoder.attachHalfQuad(16, 17); // CLK, DT
  encoder.setCount(0); // Сброс счетчика

  // Настройка кнопки
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("Impulse generator is ready. Send: <PULSE/STOP> <NORMAL/INVERTED> <us> <ms>");
  Serial.println("Output on pin: " + String(OUTPUT_PIN));
  Serial.println("Example: PULSE INVERTED 10 100");
  Serial.println("Generation STARTED : Pulse " + String(pulseUs) + "us, Pause " + String(pauseMs) + "ms, Type:" + String(inverted ? "HIGH to LOW (INVERTED)" : "LOW to HIGH (NORMAL)"));
}

void loop() {
  static uint32_t lastMillis = 0;
  if (isRunning && (millis() - lastMillis >= pauseMs)) {
    lastMillis = millis();
    send_pulse();
  }

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    char mode[10];
    char type[10];
    uint32_t pW, pP;

    if (sscanf(input.c_str(), "%s %s %u %u", mode, type, &pW, &pP) == 4) {
      if (strcmp(mode, "PULSE") == 0) {
        isRunning = true;
        // Serial.println("Generation STARTED");
      } else if (strcmp(mode, "STOP") == 0) {
        isRunning = false;
        Serial.println("Generation STOPPED");
        // Option : rmt_stop(RMT_CHAN);
      } else if (strcmp(mode, "GET") == 0) {
        //         uint32_t pulseUs = 10;
        // uint32_t pauseMs = 1;
        // bool inverted = false;
        // bool isRunning = true;
        Serial.println("*VALpulseUs:" + String(pulseUs) + ", pauseMs:" + String(pauseMs) + ", inverted:" + String(inverted ? "YES" : "NO") + ", isRunning:" + String(isRunning ? "YES" : "NO")  );
        
      }

      if (strcmp(type, "INVERTED") == 0) {
        inverted = true;
      } else {
        inverted = false;
      }
      pulseUs = constrain(pW, 1, 32767);  // RMT item limit 32767 ticks (us)
      pauseMs = constrain(pP, 1, 2000);
      if (isRunning) {
        Serial.printf("Pulse=%uus, Pause=%umc, Pulse  %s\n", pulseUs, pauseMs, inverted ? "HIGH to LOW (INVERTED) " : " LOW to HIGH (NORMAL) ");
      }
    } else {
      Serial.println("Invalid command. Use: <PULSE/STOP> <NORMAL/INVERTED> <us> <ms>");
    }
  }
  
  
  static long lastCount = 0;
  long currentCount = encoder.getCount();

  if (currentCount != lastCount) {
    if (currentCount > lastCount) {
      // Крутим по часовой стрелке
      Serial.println("*VAL+1");  // Передаем 1
    } else {
      // Крутим против часовой стрелки
      Serial.println("*VAL-1"); // Передаем -1
    }
    lastCount = currentCount;
  }

  // Читаем кнопку (LOW при нажатии)
  if (digitalRead(buttonPin) == LOW) {
    Serial.println("*SET");
    // encoder.setCount(0); // Например, сброс при нажатии
    delay(200); // Антидребезг
  }
}
