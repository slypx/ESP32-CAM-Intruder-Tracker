#include <Wire.h>
#include <NewPing.h>
#include <TFLI2C.h>

// Ultrasonic sensor
constexpr uint8_t TRIG_PIN = 9;
constexpr uint8_t ECHO_PIN = 10;
constexpr uint16_t MAX_DISTANCE_CM = 200;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);

// LiDAR/ToF sensor
constexpr uint8_t LIDAR_I2C_ADDRESS = 0x10;
TFLI2C lidar;
int16_t lidarDistanceCm = 0;

// PIR and gas sensor
constexpr uint8_t PIR_PIN = 7;
constexpr int GAS_SENSOR_PIN = A0;
constexpr int GAS_THRESHOLD = 130;
constexpr int GAS_CRITICAL_THRESHOLD = 800;

// Buzzer and LED outputs
constexpr uint8_t NORMAL_BUZZER_PIN = 8;
constexpr uint8_t LOUD_BUZZER_PIN = 6;
constexpr uint8_t LED_PIN = 13;

// Sequential Kalman-style estimator
float distanceEstimateCm = 0.0F;
float estimateCovariance = 1.0F;
constexpr float PROCESS_NOISE = 0.1F;
constexpr float SONAR_MEASUREMENT_NOISE = 0.3F;
constexpr float LIDAR_MEASUREMENT_NOISE = 0.1F;

// Moving average for sonar
constexpr uint8_t SONAR_WINDOW_SIZE = 5;
float sonarReadings[SONAR_WINDOW_SIZE] = {0.0F};
uint8_t sonarIndex = 0;
float sonarSum = 0.0F;

void flashBuzzer(uint8_t buzzerPin, uint8_t flashes, uint16_t durationMs) {
  for (uint8_t i = 0; i < flashes; ++i) {
    digitalWrite(buzzerPin, HIGH);
    delay(durationMs);
    digitalWrite(buzzerPin, LOW);
    delay(durationMs);
  }
}

void flashLedAndBuzzer(
    uint8_t buzzerPin,
    uint8_t flashes,
    uint16_t durationMs) {
  for (uint8_t i = 0; i < flashes; ++i) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(buzzerPin, HIGH);
    delay(durationMs);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(buzzerPin, LOW);
    delay(durationMs);
  }
}

void handleCriticalGasLeak() {
  Serial.println("CRITICAL GAS LEAK DETECTED! CONTINUOUS ALERT!");

  // A reset is required to leave this latched critical-alarm state.
  while (true) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(LOUD_BUZZER_PIN, HIGH);
    delay(500);

    digitalWrite(LED_PIN, LOW);
    delay(500);
  }
}

float getSmoothedSonarMeasurement() {
  const float readingCm = sonar.ping_cm();
  if (readingCm < 2.0F || readingCm > MAX_DISTANCE_CM) {
    return distanceEstimateCm;
  }

  sonarSum -= sonarReadings[sonarIndex];
  sonarReadings[sonarIndex] = readingCm;
  sonarSum += sonarReadings[sonarIndex];
  sonarIndex = (sonarIndex + 1) % SONAR_WINDOW_SIZE;

  return sonarSum / SONAR_WINDOW_SIZE;
}

float getLidarMeasurement() {
  if (lidar.getData(lidarDistanceCm, LIDAR_I2C_ADDRESS)) {
    return static_cast<float>(lidarDistanceCm);
  }

  return distanceEstimateCm;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(NORMAL_BUZZER_PIN, OUTPUT);
  pinMode(LOUD_BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  if (lidar.getData(lidarDistanceCm, LIDAR_I2C_ADDRESS)) {
    Serial.println("LiDAR initialized successfully.");
  } else {
    Serial.println("LiDAR initialization failed.");
  }
}

void loop() {
  const bool motionDetected = digitalRead(PIR_PIN) == HIGH;
  float sonarDistanceCm = getSmoothedSonarMeasurement();
  const float lidarDistance = getLidarMeasurement();
  const int gasLevel = analogRead(GAS_SENSOR_PIN);

  if (sonarDistanceCm < 2.0F || sonarDistanceCm > MAX_DISTANCE_CM) {
    Serial.println("Sonar reading invalid. Using LiDAR only.");
    sonarDistanceCm = lidarDistance;
  }

  // Predict.
  estimateCovariance += PROCESS_NOISE;

  // Update with sonar.
  const float sonarGain =
      estimateCovariance /
      (estimateCovariance + SONAR_MEASUREMENT_NOISE);
  distanceEstimateCm +=
      sonarGain * (sonarDistanceCm - distanceEstimateCm);
  estimateCovariance *= (1.0F - sonarGain);

  // Update with LiDAR/ToF.
  const float lidarGain =
      estimateCovariance /
      (estimateCovariance + LIDAR_MEASUREMENT_NOISE);
  distanceEstimateCm +=
      lidarGain * (lidarDistance - distanceEstimateCm);
  estimateCovariance *= (1.0F - lidarGain);

  Serial.println("===== Sensor Readings =====");
  Serial.print("Gas Level: ");
  Serial.println(gasLevel);
  Serial.print("Motion Detected: ");
  Serial.println(motionDetected ? "YES" : "NO");
  Serial.print("Sonar: ");
  Serial.print(sonarDistanceCm);
  Serial.print(" cm | LiDAR: ");
  Serial.print(lidarDistance);
  Serial.print(" cm | Estimated Distance (Kalman): ");
  Serial.println(distanceEstimateCm);

  if (gasLevel > GAS_CRITICAL_THRESHOLD) {
    handleCriticalGasLeak();
  } else if (gasLevel > GAS_THRESHOLD) {
    Serial.println("Gas leak detected! Activating loud alarm.");
    flashBuzzer(LOUD_BUZZER_PIN, 3, 500);
  }

  const bool objectInTriggerRange =
      sonarDistanceCm >= 30.0F && sonarDistanceCm <= 80.0F &&
      lidarDistance >= 30.0F && lidarDistance <= 80.0F;

  if (motionDetected && objectInTriggerRange) {
    flashLedAndBuzzer(LOUD_BUZZER_PIN, 3, 300);
  }

  delay(1000);
}
