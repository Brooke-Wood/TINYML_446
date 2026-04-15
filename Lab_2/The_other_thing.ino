#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

//CLAUDE USED TO CREATE CODE SKELETON
//because i am sick and have minimal brainpower today-

// --- Thresholds (tune these!) ---
#define HUMID_JUMP_THRESHOLD     3.0    // % RH change from baseline
#define TEMP_RISE_THRESHOLD      1.0    // °C change from baseline
#define MAG_SHIFT_THRESHOLD      10.0   // uT change from baseline
#define LIGHT_CHANGE_THRESHOLD   10    // clear channel change from baseline

// --- Cooldown (ms) to avoid rapid re-triggering ---
#define COOLDOWN_MS   3000

// --- Baseline values (set during first few readings) ---
float baselineHumidity    = -1;
float baselineTemp        = -1;
float baselineMag         = -1;
int   baselineClear       = -1;
bool  baselineReady       = false;
int   warmupCycles        = 0;
#define WARMUP_CYCLES     10

unsigned long lastEventTime = 0;

float getMagMagnitude(float mx, float my, float mz) {
    return sqrt(mx*mx + my*my + mz*mz);
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    if (!HS300x.begin()) {
        Serial.println("Failed to initialize HS3003.");
        while (1);
    }

    if (!IMU.begin()) {
        Serial.println("Failed to initialize IMU.");
        while (1);
    }

    if (!APDS.begin()) {
        Serial.println("Failed to initialize APDS9960.");
        while (1);
    }

    Serial.println("Environmental monitoring system started.");
    Serial.println("Warming up baseline...");
}

void loop() {
    // --- 1. Read humidity & temperature ---
    float rh   = HS300x.readHumidity();
    float temp = HS300x.readTemperature();

    // --- 2. Read magnetometer ---
    float mx, my, mz;
    float mag = 0.0;
    if (IMU.magneticFieldAvailable()) {
        IMU.readMagneticField(mx, my, mz);
        mag = getMagMagnitude(mx, my, mz);
    }

    // --- 3. Read APDS9960 color/light ---
    int r = 0, g = 0, b = 0, clear = 0;
    if (APDS.colorAvailable()) {
        APDS.readColor(r, g, b, clear);
    }

    // --- Baseline calibration (average first N cycles) ---
    if (!baselineReady) {
        if (baselineHumidity < 0) {
            // First reading — initialize
            baselineHumidity = rh;
            baselineTemp     = temp;
            baselineMag      = mag;
            baselineClear    = clear;
        } else {
            // Running average
            baselineHumidity = (baselineHumidity * warmupCycles + rh)   / (warmupCycles + 1);
            baselineTemp     = (baselineTemp     * warmupCycles + temp)  / (warmupCycles + 1);
            baselineMag      = (baselineMag      * warmupCycles + mag)   / (warmupCycles + 1);
            baselineClear    = (baselineClear    * warmupCycles + clear) / (warmupCycles + 1);
        }
        warmupCycles++;
        if (warmupCycles >= WARMUP_CYCLES) {
            baselineReady = true;
            Serial.println("Baseline ready.");
        }
        delay(200);
        return;
    }

    // --- Binary flags ---
    bool humid_jump          = abs(rh    - baselineHumidity) > HUMID_JUMP_THRESHOLD;
    bool temp_rise           = (temp     - baselineTemp)     > TEMP_RISE_THRESHOLD;
    bool mag_shift           = abs(mag   - baselineMag)      > MAG_SHIFT_THRESHOLD;
    bool light_or_color_change = abs(clear - baselineClear)  > LIGHT_CHANGE_THRESHOLD;

    // --- Rule-based event classification ---
    String label = "BASELINE_NORMAL";

    unsigned long now = millis();
    bool cooldownOver = (now - lastEventTime) > COOLDOWN_MS;

    if (cooldownOver) {
        if (mag_shift) {
            label = "MAGNETIC_DISTURBANCE_EVENT";
            lastEventTime = now;
        } else if (humid_jump || temp_rise) {
            label = "BREATH_OR_WARM_AIR_EVENT";
            lastEventTime = now;
        } else if (light_or_color_change) {
            label = "LIGHT_OR_COLOR_CHANGE_EVENT";
            lastEventTime = now;
        }
    }

    // --- Serial output ---
    Serial.print("raw,rh=");    Serial.print(rh, 2);
    Serial.print(",temp=");     Serial.print(temp, 2);
    Serial.print(",mag=");      Serial.print(mag, 2);
    Serial.print(",r=");        Serial.print(r);
    Serial.print(",g=");        Serial.print(g);
    Serial.print(",b=");        Serial.print(b);
    Serial.print(",clear=");    Serial.println(clear);

    Serial.print("flags,humid_jump=");          Serial.print(humid_jump);
    Serial.print(",temp_rise=");                Serial.print(temp_rise);
    Serial.print(",mag_shift=");                Serial.print(mag_shift);
    Serial.print(",light_or_color_change=");    Serial.println(light_or_color_change);

    Serial.print("event,"); Serial.println(label);
    Serial.println();

    delay(200);
}
