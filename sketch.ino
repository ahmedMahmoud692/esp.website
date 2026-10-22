// Libraries give us ready-made tools.
#include <WiFi.h>               // Connect the ESP32 to Wi-Fi
#include <WebServer.h>          // Run a website on the ESP32
#include <Wire.h>               // Communicate using I2C
#include <Adafruit_MPU6050.h>   // Read the MPU6050 sensor
#include "firebase_upload.h" // Sign in and upload sensor readings
#include <Adafruit_Sensor.h>    // Define the sensor reading format

// Define which ESP32 pins we are using.
#define LDR_PIN 34    // Connect the LDR's AO pin here
#define SDA_PIN 21    // MPU6050 data wire
#define SCL_PIN 22    // MPU6050 clock wire

// Wokwi's simulated Wi-Fi network has no password.
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Create an object that controls the MPU6050.
Adafruit_MPU6050 mpu;

// Create a web server on port 80, the standard HTTP port.
WebServer server(80);

// Variables store the latest light readings.
int lightRaw = 0;
float lightPercent = 0;

// Acceleration readings for the three axes.
float accelX = 0;
float accelY = 0;
float accelZ = 0;

// Angular velocity readings for the three axes.
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

// Timing variables let us read every 2 seconds.
unsigned long lastRead = 0;
const unsigned long readInterval = 2000; // 2000 milliseconds = 2 seconds

// This becomes true after taking the first reading.
bool haveReading = false;


// --------------------------------------------------
// Read both sensors and save their measurements.
// --------------------------------------------------
void updateSensors() {

  // millis() tells us how many milliseconds have passed
  // since the ESP32 started.
  //
  // If we already have a reading and 2 seconds have not
  // passed yet, leave this function without reading again.
  if (haveReading && millis() - lastRead < readInterval) {
    return;
  }

  // Remember when this reading started.
  lastRead = millis();

  // Read the LDR's analog output: a number from 0 to 4095.
  lightRaw = analogRead(LDR_PIN);

  // On Wokwi's LDR module:
  // more light → lower analog reading.
  //
  // Reverse the reading and express it as a percentage.
  // This is a relative brightness estimate, not lux.
  lightPercent = 100.0 * (4095 - lightRaw) / 4095.0;

  // These variables will hold the MPU6050 measurements.
  sensors_event_t acceleration, rotation, temperature;

  // Ask the MPU6050 library to fill those variables.
  // Temperature is available, but we do not display it.
  mpu.getEvent(&acceleration, &rotation, &temperature);

  // Copy acceleration readings into our variables.
  // The library gives these values in m/s².
  accelX = acceleration.acceleration.x;
  accelY = acceleration.acceleration.y;
  accelZ = acceleration.acceleration.z;

  // The library gives rotation speed in radians per second.
  // Multiply by 180 / PI to convert to degrees per second.
  gyroX = rotation.gyro.x * 180.0 / PI;
  gyroY = rotation.gyro.y * 180.0 / PI;
  gyroZ = rotation.gyro.z * 180.0 / PI;

  // We now have our first set of readings.
  haveReading = true;

  // Print readings in Wokwi's Serial Monitor.
  // %.1f means one decimal place.
  // %.2f means two decimal places.
  // %d means a whole number.
  // %% prints the percentage symbol.
  // \n starts a new line.
  Serial.printf(
    "Light: %.1f %% | ADC: %d\n",
    lightPercent, lightRaw
  );

  Serial.printf(
    "Acceleration: X=%.2f Y=%.2f Z=%.2f m/s²\n",
    accelX, accelY, accelZ
  );

  Serial.printf(
    "Angular velocity: X=%.2f Y=%.2f Z=%.2f degrees/s\n\n",
    gyroX, gyroY, gyroZ
  );
}


// --------------------------------------------------
// Send the website when a browser requests "/"
// --------------------------------------------------
void handleRoot() {

  // This special string lets us write a whole webpage
  // across many lines without escaping every quote.
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <!-- Allow symbols such as ° and ² -->
  <meta charset="UTF-8">

  <!-- Make the page fit phone and computer screens -->
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <title>ESP32 Sensor Readings</title>

  <!-- CSS controls the appearance of the webpage -->
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f2f4f7;
      color: #222;
      max-width: 650px;
      margin: 30px auto;
      padding: 15px;
    }

    /* Each section is a white box */
    section {
      background: white;
      padding: 20px;
      margin-bottom: 20px;
      border: 1px solid #ddd;
      border-radius: 6px;
    }

    h1 {
      font-size: 26px;
    }

    h2 {
      font-size: 20px;
      margin-top: 0;
    }

    table {
      width: 100%;
      border-collapse: collapse;
      text-align: center;
    }

    th, td {
      border-bottom: 1px solid #ddd;
      padding: 12px 5px;
    }

    /* Make the light percentage large */
    .value {
      font-size: 32px;
      font-weight: bold;
    }

    .note, #status {
      font-size: 14px;
      color: #555;
    }
  </style>
</head>

<body>
  <h1>ESP32 Sensor Readings</h1>

  <!-- Light sensor section -->
  <section>
    <h2>Light Level</h2>

    <!-- JavaScript replaces -- with the latest reading -->
    <p class="value" id="light">--</p>

    <p>ADC reading: <span id="raw">--</span></p>
    <p class="note">Relative brightness estimate, not lux.</p>
  </section>

  <!-- Acceleration section -->
  <section>
    <h2>Acceleration</h2>

    <table>
      <tr><th>X</th><th>Y</th><th>Z</th></tr>
      <tr>
        <td id="ax">--</td>
        <td id="ay">--</td>
        <td id="az">--</td>
      </tr>
    </table>

    <p class="note">Unit: m/s². Includes gravity.</p>
  </section>

  <!-- Angular velocity section -->
  <section>
    <h2>Angular Velocity</h2>

    <table>
      <tr><th>X</th><th>Y</th><th>Z</th></tr>
      <tr>
        <td id="gx">--</td>
        <td id="gy">--</td>
        <td id="gz">--</td>
      </tr>
    </table>

    <p class="note">Unit: degrees per second.</p>
  </section>

  <!-- Show whether the browser can receive readings -->
  <p id="status">Waiting for readings...</p>

  <!-- JavaScript runs inside the browser -->
  <script>

    // async lets this function wait for a network response.
    async function updateDashboard() {

      // Try to request and display the sensor readings.
      try {

        // Ask the ESP32's /data address for the latest values.
        // no-store tells the browser not to reuse an old response.
        const response = await fetch('/data', {
          cache: 'no-store'
        });

        // If the server reports an error, jump to catch below.
        if (!response.ok) {
          throw new Error('Could not read sensor data');
        }

        // Convert the JSON response into a JavaScript object.
        const data = await response.json();

        // Find each HTML element by its id and change its text.
        // toFixed(1) shows one decimal place.
        document.getElementById('light').textContent =
          data.light.toFixed(1) + ' %';

        document.getElementById('raw').textContent = data.raw;

        // Display acceleration with two decimal places.
        document.getElementById('ax').textContent =
          data.ax.toFixed(2);

        document.getElementById('ay').textContent =
          data.ay.toFixed(2);

        document.getElementById('az').textContent =
          data.az.toFixed(2);

        // Display angular velocity with two decimal places.
        document.getElementById('gx').textContent =
          data.gx.toFixed(2);

        document.getElementById('gy').textContent =
          data.gy.toFixed(2);

        document.getElementById('gz').textContent =
          data.gz.toFixed(2);

        // Show the time of the successful webpage update.
        document.getElementById('status').textContent =
          'Connected | Last update: ' +
          new Date().toLocaleTimeString();

      } catch (error) {

        // This runs if requesting or displaying the data fails.
        document.getElementById('status').textContent =
          'Connection error. Displayed readings may be old.';
      }

      // Wait 2 seconds, then request readings again.
      setTimeout(updateDashboard, 2000);
    }

    // Make the first request when the page opens.
    updateDashboard();

  </script>
</body>
</html>
)rawliteral";

  // Send the webpage to the browser.
  // 200 means the request succeeded.
  // text/html tells the browser this is a webpage.
  server.send(200, "text/html", html);
}


// --------------------------------------------------
// Send readings when a browser requests "/data"
// --------------------------------------------------
void handleData() {

  // Take a new reading if the 2-second interval has passed.
  updateSensors();

  // Build a JSON message containing the sensor values.
  // Example: {"light":60.0,"raw":1638,"ax":0.00,...}
  String json = "{";

  // \" adds a quotation mark inside a C++ string.
  // += adds more text to the end of the string.
  json += "\"light\":" + String(lightPercent, 1) + ",";
  json += "\"raw\":" + String(lightRaw) + ",";

  json += "\"ax\":" + String(accelX, 2) + ",";
  json += "\"ay\":" + String(accelY, 2) + ",";
  json += "\"az\":" + String(accelZ, 2) + ",";

  json += "\"gx\":" + String(gyroX, 2) + ",";
  json += "\"gy\":" + String(gyroY, 2) + ",";

  // The final value has no comma after it.
  json += "\"gz\":" + String(gyroZ, 2);

  json += "}";

  // Tell the browser not to save and reuse this response.
  server.sendHeader("Cache-Control", "no-store");

  // Send the JSON readings to the browser.
  server.send(200, "application/json", json);
}


// --------------------------------------------------
// setup() runs once when the ESP32 starts.
// --------------------------------------------------
void setup() {

  // Start Serial Monitor communication at 115200 baud.
  Serial.begin(115200);

  // Set up the LDR analog input.
  pinMode(LDR_PIN, INPUT);

  // Use 12-bit readings: numbers from 0 to 4095.
  analogReadResolution(12);

  // Select the wider ADC voltage measurement range.
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  // Start I2C communication using our SDA and SCL pins.
  Wire.begin(SDA_PIN, SCL_PIN);

  // Try to start the MPU6050.
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found. Check the wiring.");

    // Stop here if the sensor cannot be found.
    while (true) {
      delay(1000);
    }
  }

  Serial.println("MPU6050 connected!");

  // Choose the measurement ranges.
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);

  // Use the sensor's filter to reduce rapid fluctuations.
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Read the sensors once before starting the website.
  updateSensors();

  Serial.print("Connecting to Wokwi WiFi");

  // Connect to Wokwi-GUEST on Wi-Fi channel 6.
  WiFi.begin(ssid, password, 6);

  // Wait until the Wi-Fi connection succeeds.
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");

  // Print the ESP32's address on the simulated network.
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Choose which function handles each website address.
  server.on("/", handleRoot);      // "/" sends the webpage
  server.on("/data", handleData);  // "/data" sends the readings

  // Start accepting browser requests.
  server.begin();

  Serial.println("Web server started!");

  // Prepare time synchronization and ask for the device password.
  firebaseBegin();
}


// --------------------------------------------------
// loop() runs repeatedly while the ESP32 is on.
// --------------------------------------------------
void loop() {
  // Read the sensors when their 2-second interval has passed.
  updateSensors();

  // Handle requests from the website.
  server.handleClient();

  // Send the latest values to Firebase (approximately every 5 seconds).
  firebaseUpload(lightPercent, lightRaw, accelX, accelY, accelZ,
                 gyroX, gyroY, gyroZ);

  // Pause briefly so the program does not run a busy loop.
  delay(2);
}
