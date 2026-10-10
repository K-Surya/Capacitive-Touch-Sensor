#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     21
#define OLED_CS     22
#define OLED_RESET  19

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// --- Hardware Pins ---
const int touchPin = 4;        
const int ledPin = 2;          

// --- Sensing & Calibration Variables ---
unsigned long baselineDrainTime = 0;
const unsigned long TOUCH_OFFSET = 5; // Microsecond margin above baseline to register a touch
unsigned long touchThreshold = 0;

// --- Adaptive Timing Variables ---
const int DEBOUNCE = 30;         
float dotDuration = 100.0;     // Initial baseline dot duration in ms
int dotDashLimit = 250;        // Dynamically updated limit between dot and dash
int letterSpaceLimit = 400;    // Dynamically updated limit to signify end of character

// --- State Tracking ---
bool wasTouched = false;
unsigned long touchStartTime = 0;
unsigned long lastReleaseTime = 0;
bool spacePrinted = true;

// --- Data Storage ---
String currentMorseChar = "";    
String decodedMessage = ""; 

const char* morseLetters[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--.."
};
const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// --- Function Prototypes ---
void initDisplay();
void calibrateTouchSensor();
unsigned long getDrainTime();
void handleTouchEdge(bool isTouched);
void updateAdaptiveTiming(unsigned long duration);
void checkLetterTimeout(bool isTouched);
void translateAndDisplay();
void updateOLEDDisplay();

void setup() {
    Serial.begin(115200);        
    pinMode(ledPin, OUTPUT);    
    digitalWrite(ledPin, LOW);  

    initDisplay();
    calibrateTouchSensor();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Ready to type.");
    display.display();
}

void loop() {
    unsigned long drainTime = getDrainTime();
    bool isTouched = (drainTime > touchThreshold);

    handleTouchEdge(isTouched);
    checkLetterTimeout(isTouched);

    wasTouched = isTouched;
    delay(10); 
}

void initDisplay() {
    if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
    }
    display.clearDisplay();
    display.setTextSize(2);      
    display.setTextColor(SSD1306_WHITE); 
    display.setCursor(0, 0);     
    display.println("Booting...");
    display.display();
}

void calibrateTouchSensor() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Calibrating");
    display.println("Do not touch!");
    display.display();

    unsigned long totalDrain = 0;
    const int numSamples = 50;

    for (int i = 0; i < numSamples; i++) {
    totalDrain += getDrainTime();
    delay(20); // allow capacitor to settle between samples 
    }
  
    baselineDrainTime = totalDrain / numSamples;
    touchThreshold = baselineDrainTime + TOUCH_OFFSET;

    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Base: ");
    display.println(baselineDrainTime);
    display.print("Thresh: ");
    display.println(touchThreshold);
    display.display();
    delay(1500);
}

unsigned long getDrainTime() {
    pinMode(touchPin, OUTPUT);
    digitalWrite(touchPin, HIGH);
    delayMicroseconds(10); 

    unsigned long startTime = micros();
    pinMode(touchPin, INPUT);

    while (digitalRead(touchPin) == HIGH) {
    if (micros() - startTime > 10000) break; // 10ms safety timeout
    }
    return micros() - startTime;
}

void handleTouchEdge(bool isTouched) {
    // Edge Detection: Touch Started (Rising Edge)
    if (isTouched && !wasTouched) {
        touchStartTime = millis();  
        digitalWrite(ledPin, HIGH); 
        spacePrinted = false;       
    } 
    // Edge Detection: Touch Ended (Falling Edge)
    else if (!isTouched && wasTouched) {
        digitalWrite(ledPin, LOW);  

        unsigned long touchDuration = millis() - touchStartTime; 
        lastReleaseTime = millis(); 

        if (touchDuration > DEBOUNCE) {
            if (touchDuration < dotDashLimit) {
                currentMorseChar += ".";
            updateAdaptiveTiming(touchDuration);
            } else {
                currentMorseChar += "-";
            }
        }
    }
}

void updateAdaptiveTiming(unsigned long duration) {
    // Exponential Moving Average (EMA) to map to user's tapping cadence
    // Only adapts on dots, as dashes vary more widely in human timing
    dotDuration = (dotDuration * 0.7) + (duration * 0.3);
    dotDashLimit = dotDuration * 2.5;     // Threshold midway to 3x standard dash
    letterSpaceLimit = dotDuration * 4.0; // Margin added for space threshold
}

void checkLetterTimeout(bool isTouched) {
    if (!isTouched && !spacePrinted) {
    if (millis() - lastReleaseTime > letterSpaceLimit) {
        if (currentMorseChar.length() > 0) {
            translateAndDisplay();
        }
            spacePrinted = true; 
        }
    }
}

void translateAndDisplay() {
    char translatedLetter = '?';
    for (int i = 0; i < 26; i++) {
        if (currentMorseChar == morseLetters[i]) {
            translatedLetter = alphabet[i];
            break;
        }
    }

    if (translatedLetter != '?') {
        decodedMessage += translatedLetter; 
        updateOLEDDisplay();                
    }

    currentMorseChar = ""; 
}

void updateOLEDDisplay() {
    display.clearDisplay(); 
    display.setCursor(0, 0); 

    if (decodedMessage.length() > 40) {
        decodedMessage = ""; // Clear buffer to prevent off-screen rendering
    }

    display.print(decodedMessage); 
    display.display();             
}