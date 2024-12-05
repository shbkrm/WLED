#include "wled.h"

class SerialTouchSwitchUsermod : public Usermod {
private:
    static constexpr uint8_t UART_BUFFER_SIZE = 128;
    uint8_t dataBuffer[UART_BUFFER_SIZE];
    int timeout = 300; // UART timeout for reading data

    uint8_t switchEffects[8] = {FX_MODE_RAINBOW, FX_MODE_BREATH, FX_MODE_FIRE_FLICKER,
                                FX_MODE_DUAL_SCAN, FX_MODE_COLOR_WIPE, FX_MODE_THEATER_CHASE,
                                FX_MODE_STROBE, FX_MODE_BREATH};
    bool switchStates[8] = {false};

    // UART timed read
    uint8_t timedRead() {
        int c;
        int startMillis = millis();
        do {
            c = Serial1.read();
            if (c >= 0)
                return c;
            if (timeout == 0)
                return -1;
            yield();
        } while (millis() - startMillis < timeout);
        return -1;
    }

    // Parse UART frame
    void parseUARTFrame(uint8_t *frame, uint8_t length) {
        uint8_t commandType = frame[1];
        uint8_t switchNumber = frame[2];
        uint8_t switchStatus = frame[3];

        if (commandType == 0x04 && switchStatus == 0x01) { // Switch pressed
            toggleSwitch(switchNumber);
        }
    }

    // Read UART data
    void handleUART() {
        if (Serial1.available()) {
            int tempFB = timedRead();
            if (tempFB == 0x7B) { // Start byte
                int tempLen = timedRead();
                int len = 0;
                while (len < tempLen) {
                    dataBuffer[len++] = timedRead();
                }
                parseUARTFrame(dataBuffer, len);
            }
        }
    }

    // Toggle a switch
    void toggleSwitch(uint8_t switchNumber) {
        if (switchNumber >= 1 && switchNumber <= 8) {
            uint8_t index = switchNumber - 1;
            switchStates[index] = !switchStates[index];
            if (switchStates[index]) {
                effectCurrent = switchEffects[index];
                bri = 128; // Turn on LEDs
            } else {
                bri = 0; // Turn off LEDs
            }
            colorUpdated(CALL_MODE_DIRECT_CHANGE);
        }
    }

public:
    void setup() override {
        Serial1.begin(9600);   // Initialize UART
        Serial.println("Serial Touch Usermod initiated");
    }

    void loop() override {
        handleUART();
    }

    void addToConfig(JsonObject& root) override {
        JsonArray effectsArray = root.createNestedArray(F("switchEffects"));
        for (int i = 0; i < 8; i++) {
            effectsArray.add(switchEffects[i]);
        }
    }

    bool readFromConfig(JsonObject& root) override {
        JsonArray effectsArray = root[F("switchEffects")];
        if (!effectsArray.isNull()) {
            for (int i = 0; i < 8; i++) {
                if (effectsArray[i].is<int>()) {
                    switchEffects[i] = effectsArray[i];
                }
            }
            Serial.println("Switch effects loaded from configuration.");
            return true;
        }
        return false;
    }

    void addToJsonInfo(JsonObject& root) override {
        JsonObject customSettings = root.createNestedObject(F("SerialTouchSwitch"));
        for (int i = 0; i < 8; i++) {
            customSettings[String("Switch ") + String(i + 1)] = String(JSON_mode_names[switchEffects[i]]);
        }
    }

    uint16_t getId() override {
        return USERMOD_ID_SERIAL_TOUCH_SWITCH;
    }
};
