#include "wled.h"

class SerialTouchSwitchUsermod : public Usermod {
private:
    static constexpr uint8_t UART_BUFFER_SIZE = 128;
    uint8_t dataBuffer[UART_BUFFER_SIZE];
    int timeout = 300; // UART timeout for reading data

    uint8_t switchEffects[8] = {FX_MODE_RAINBOW, FX_MODE_COLOR_WIPE, FX_MODE_FIRE_FLICKER,
                                FX_MODE_DUAL_SCAN, FX_MODE_BREATH, FX_MODE_THEATER_CHASE,
                                FX_MODE_STROBE, FX_MODE_BREATH};
    bool switchStates[8] = {false};
    uint8_t frameCount = 0;

    // UART timed read
    uint8_t timedRead() {
        int c;
        int startMillis = millis();
        do {
            c = Serial1.read();
            Serial.print(c, HEX);
            if (c >= 0)
                return c;
            if (timeout == 0)
                return -1;
            yield();
        } while (millis() - startMillis < timeout);
        return -1;
    }

    // Parse UART Frame for Push Button Logic
    void parseUARTFrame(uint8_t *frame, uint8_t length) {
        uint8_t commandType  = frame[1];
        uint8_t switchNumber = frame[2];
        uint8_t switchStatus = frame[3];

        if (commandType == 0x04) { // Switch Status Command
        if (switchStatus == 0x01) { // Button Pressed
            Serial.printf("Switch %d Pressed\n", switchNumber);
            for(int i =1;i<9;i++) {
                if(switchNumber == i) {
                    toggleSwitch(switchNumber); // Toggle state on button press
                } else {
                    if(switchStates[i-1]) {
                        switchStates[i-1] = false;
                        // Send feedback to the switchboard
                        sendSwitchState(i, switchStates[i-1]);
                    }
                }
            }
        }
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
            sendSwitchState(switchNumber, switchStates[index]);
            colorUpdated(CALL_MODE_DIRECT_CHANGE);
        }
    }

    void sendSwitchState(uint8_t switchNumber, bool state) {
        uint8_t frame[8];
        frame[0] = 0x7B;             // Start byte
        frame[1] = 0x04;             // Length
        frame[2] = frameCount++;     // Frame Count
        frame[3] = 0x05;             // Command type (Switch Status Feedback)
        frame[4] = switchNumber;     // Switch number (1–8)
        frame[5] = state ? 0x01 : 0x00; // Switch state (On = 0x01, Off = 0x00)
        frame[6] = calculateChecksum(frame, 6); // Checksum

        // Send the frame over UART
        for (uint8_t i = 0; i < 7; i++) {
            Serial.print(frame[i], HEX);
            Serial.print(" ");
            Serial1.write((unsigned char) frame[i]);
        }
        Serial.println();
        Serial.printf("Sent switch state: Switch %d -> %s\n", switchNumber, state ? "On" : "Off");
    }

    // Helper Functions
    uint8_t calculateChecksum(uint8_t *frame, uint8_t length) {
        uint8_t sum = 0;
        for (uint8_t i = 2; i < length; i++) {
            sum += frame[i];
        }
        return sum;
    }

public:
    void setup() override {
        //Serial1.begin(9600, SERIAL_8N1, 5, 17);   // Initialize UART
        Serial1.begin(9600, SERIAL_8N1, 5, 17);
        Serial.println(F("Serial Touch Usermod init"));
    }

    void loop() override {
        handleUART();
    }

    void addToConfig(JsonObject& root) override {
        // Remove any existing "Switch Effects" key to avoid conflicts
        if (root.containsKey(F("Switch Effects"))) {
            root.remove(F("Switch Effects"));
        }

        // Add the array to "Switch Effects"
        JsonArray effectsArray = root.createNestedArray(F("Switch Effects"));
        for (int i = 0; i < 8; i++) {
            effectsArray.add(switchEffects[i]); // Save mode indices
            Serial.printf("Saving Switch %d Effect: %d\n", i + 1, switchEffects[i]);
        }

        // Debugging: Print the updated JSON
        Serial.println(F("Config after addToConfig:"));
        serializeJson(root, Serial);
    }



    bool readFromConfig(JsonObject& root) override {
        // Check if "Switch Effects" exists and is an array
        if (root.containsKey(F("Switch Effects"))) {
            JsonVariant effectsVariant = root[F("Switch Effects")];

            // Handle unexpected structures (e.g., objects with "unknown" key)
            if (effectsVariant.is<JsonObject>()) {
                JsonObject effectsObject = effectsVariant.as<JsonObject>();
                if (effectsObject.containsKey(F("unknown"))) {
                    JsonArray effectsArray = effectsObject[F("unknown")];
                    Serial.println(F("Found nested 'unknown' key in Switch Effects. Fixing structure..."));
                    return loadEffectsArray(effectsArray);
                }
            }

            // Handle correct array structure
            if (effectsVariant.is<JsonArray>()) {
                JsonArray effectsArray = effectsVariant.as<JsonArray>();
                return loadEffectsArray(effectsArray);
            }
        }

        // Log and reset to defaults if "Switch Effects" key is missing
        Serial.println(F("No valid 'Switch Effects' found in config."));
        resetEffectsToDefault();
        return false;
    }

    // Helper function to load effects from a JSON array
    bool loadEffectsArray(JsonArray effectsArray) {
        for (int i = 0; i < 8; i++) {
            if (effectsArray[i].is<int>()) {
                switchEffects[i] = effectsArray[i]; // Load mode index
                Serial.printf("Loaded Switch %d Effect: %d\n", i + 1, switchEffects[i]);
            } else {
                switchEffects[i] = 0; // Reset to default if invalid
                Serial.printf("Invalid effect for Switch %d. Resetting to default.\n", i + 1);
            }
        }
        return true;
    }

    // Helper function to reset effects to default
    void resetEffectsToDefault() {
        for (int i = 0; i < 8; i++) {
            switchEffects[i] = 0; // Default effect index
        }
        Serial.println(F("Switch effects reset to default."));
    }


    void addToJsonInfo(JsonObject& root) override {
        JsonObject customSettings = root.createNestedObject(F("SerialTouchSwitch"));
        for (int i = 0; i < 8; i++) {
            customSettings[String("Switch ") + String(i + 1)] = switchEffects[i]; // Add effect index
        }
    }

    uint16_t getId() override {
        return USERMOD_ID_SERIAL_TOUCH_SWITCH;
    }
};