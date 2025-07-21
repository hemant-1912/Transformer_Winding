#include <WiFi.h>
#include <WebServer.h>

// ================== WiFi Credentials ==================
const char* ssid = "MIE";
const char* password = "LEN9zJ3RKr33dHx";

// ================== Motor Pins ==================
const int DIR_PIN = 25;
const int STEP_PIN = 26;
const int EN_PIN = 27;

bool stepPinMode = HIGH;

// ================== Button Pin ==================
const int START_BTN_PIN = 32;

// ================== Stepper Config ==================
const int STEPS_PER_TURN = 400;
unsigned long STEP_INTERVAL_MICROS = 2000;  // Speed control

// ================== Layer Data ==================
const int MAX_LAYERS = 50;
struct Layer {
  int turns = 0;
};
Layer layers[MAX_LAYERS];
int totalLayers = 0;
int currentLayer = 0;
int currentTurn = 0;

// ================== State Flags ==================
bool paused = false;
bool running = false;
bool stepInProgress = false;
int stepCounter = 0;
int stepsToRun = 0;
unsigned long lastStepTime = 0;

// ================== Button Debounce ==================
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 300;

WebServer server(80);

// ================== Setup ==================
void setup() {
  Serial.begin(115200);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(START_BTN_PIN, INPUT_PULLUP);
  digitalWrite(EN_PIN, HIGH);  // Disable motor driver initially

  connectToWiFi();
  setupWebServer();

  totalLayers = 2;
  layers[0].turns = 18;
  layers[1].turns = 2;
  Serial.println("Default layers loaded: [18, 2 turns]");

  Serial.println("System ready.");
}

// ================== Main Loop ==================
void loop() {
  // server.handleClient();
  handleButtonPress();

  // Non-blocking stepper control
  if (stepInProgress && running && !paused) {
    unsigned long now = micros();
    if (now - lastStepTime >= STEP_INTERVAL_MICROS) {

    
      STEP_INTERVAL_MICROS = calcSpeed(stepCounter, stepsToRun);
    

      lastStepTime = now;

      digitalWrite(STEP_PIN, stepPinMode);

      if (stepPinMode == LOW) {
        stepCounter++;
        stepPinMode = HIGH;  // Toggle step pin
      } else {
        stepPinMode = LOW;
      }

      if (stepCounter % STEPS_PER_TURN == 0) {
        currentTurn = stepCounter / STEPS_PER_TURN;
      }

      if (stepCounter >= stepsToRun) {
        finishLayer();
      }
    }
  }
}

// ================== Accelerate ==================
int calcSpeed(int currentStep, int totalSteps)
{
  // Use this formula to calculate speed based on current step
  // float delay = min_delay_us - ((min_delay_us - max_speed_delay_us) * step / accel_steps);

  const int min_speed = 1000;
  int accel_steps = 1500;
  int decel_steps = 1500;
  int max_speed = 300.0;

  if (totalSteps < (accel_steps + decel_steps)) {
    max_speed = 500.0;
    accel_steps = (totalSteps - 100) / 2;
    decel_steps = (totalSteps - 100) - accel_steps;
    // Serial.printf("\nTotal Steps: %d, Step: %d, Speed - %d", totalSteps, currentStep, max_speed);
  }

  int speed  = max_speed;
  if (totalSteps > (accel_steps + decel_steps)) {
    if (currentStep < accel_steps) {
      speed = (min_speed - ((min_speed - max_speed) * currentStep / accel_steps));
    } else if (currentStep > (totalSteps - decel_steps)) {
      speed = (min_speed - ((min_speed - max_speed) * (totalSteps - currentStep) / decel_steps));
    } else{
      speed = max_speed;
    }

  }

  // Serial.printf("\nTotal Steps: %d, Step: %d, Speed - %d", totalSteps, currentStep, speed);
  return speed;
}

// ================== Button Control ==================
void handleButtonPress() {
  static bool buttonWasPressed = false;
  bool buttonState = digitalRead(START_BTN_PIN) == LOW;

  if (buttonState && !buttonWasPressed) {
    lastButtonPress = millis();
    buttonWasPressed = true;
  } else if (!buttonState && buttonWasPressed) {
    buttonWasPressed = false;
    if (millis() - lastButtonPress > DEBOUNCE_DELAY) {
      // Button was released and debounce passed
      if (!running && !paused) {
        beginLayerStep();  // START
      } else {
        togglePause();     // PAUSE / RESUME
      }
    }
  }
}

// ================== Stepper Control ==================
void beginLayerStep() {
  if (currentLayer >= totalLayers) return;

  stepCounter = 0;
  currentTurn = 0;
  stepsToRun = layers[currentLayer].turns * STEPS_PER_TURN;
  stepInProgress = true;
  running = true;

  digitalWrite(EN_PIN, LOW);  // Enable driver
  digitalWrite(DIR_PIN, HIGH);    // Adjust direction as needed
  lastStepTime = micros();

  Serial.printf("Starting Layer %d: %d turns (%d steps)\n", currentLayer + 1, layers[currentLayer].turns, stepsToRun);
}

void finishLayer() {
  running = false;
  stepInProgress = false;
  digitalWrite(EN_PIN, HIGH);  // Disable driver

  currentLayer++;
  if (currentLayer >= totalLayers) currentLayer = 0;

  Serial.println("Layer completed. Waiting for user to start next.");
  STEP_INTERVAL_MICROS = 2000;
}

void togglePause() {
  paused = !paused;
  digitalWrite(EN_PIN, paused ? HIGH : LOW);
  Serial.println(paused ? "Paused." : "Resumed.");

  if (paused){
    STEP_INTERVAL_MICROS = 2000;
  }
}

// ================== WiFi ==================
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

// ================== Web Server ==================
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
      <html><head><title>Wire Spooler</title></head><body>
      <h2>Wire Spooler Dashboard</h2>
      <div id="status">
        <b>Layer:</b> <span id="layer">0</span>/<span id="totalLayers">0</span><br>
        <b>Turn:</b> <span id="turn">0</span>/<span id="totalTurns">0</span><br>
        <b>Status:</b> <span id="state">Idle</span>
      </div><br>
      <button onclick="toggleAction()" id="actionButton">Start</button><br><br>
      <a href='/set'>[ Set Layers ]</a>
      <script>
      function updateStatus() {
        fetch('/status').then(res => res.json()).then(data => {
          document.getElementById('layer').textContent = data.layer;
          document.getElementById('totalLayers').textContent = data.totalLayers;
          document.getElementById('turn').textContent = data.turn;
          document.getElementById('totalTurns').textContent = data.totalTurns;
          document.getElementById('state').textContent = data.paused ? 'Paused' : (data.running ? 'Running' : 'Idle');
          const btn = document.getElementById('actionButton');
          if (data.running && !data.paused) btn.textContent = 'Pause';
          else if (data.paused) btn.textContent = 'Resume';
          else btn.textContent = 'Start';
        });
      }
      setInterval(updateStatus, 2000);
      function toggleAction() {
          fetch('/action').then(updateStatus);
      }
      </script>
      </body></html>
    )rawliteral";
    server.send(200, "text/html", html);
  });

  server.on("/status", HTTP_GET, []() {
    String json = "{";
    json += "\"layer\":" + String(currentLayer + 1) + ",";
    json += "\"totalLayers\":" + String(totalLayers) + ",";
    json += "\"turn\":" + String(currentTurn) + ",";
    json += "\"totalTurns\":" + String(layers[currentLayer].turns) + ",";
    json += "\"paused\":" + String(paused ? "true" : "false") + ",";
    json += "\"running\":" + String(running ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/set", HTTP_GET, []() {
    String html = R"rawliteral(
      <html><head><title>Set Layers</title></head><body>
      <h2>Define Number of Layers</h2>
      <form action='/setlayers' method='post'>
        Total Layers: <input type='number' name='count' min='1' max='50' required>
        <input type='submit' value='Next'>
      </form>
      <a href='/'>Back</a>
      </body></html>
    )rawliteral";
    server.send(200, "text/html", html);
  });

  server.on("/setlayers", HTTP_POST, []() {
    if (server.hasArg("count")) {
      int count = server.arg("count").toInt();
      if (count > 0 && count <= MAX_LAYERS) {
        totalLayers = count;
        currentLayer = 0;

        String form = "<html><body><h2>Enter Turns for Each Layer</h2><form action='/inputturns' method='post'>";
        for (int i = 0; i < count; i++) {
          form += "Layer " + String(i + 1) + ": <input name='turn" + String(i) + "' type='number' required><br>";
        }
        form += "<input type='submit' value='Submit'></form><br><a href='/'>Back</a></body></html>";
        server.send(200, "text/html", form);
      } else {
        server.send(400, "text/plain", "Invalid layer count");
      }
    }
  });

  server.on("/inputturns", HTTP_POST, []() {
    for (int i = 0; i < totalLayers; i++) {
      String argName = "turn" + String(i);
      if (server.hasArg(argName)) {
        layers[i].turns = server.arg(argName).toInt();
      }
    }
    server.send(200, "text/html", "<html><body><h3>Turns Saved!</h3><a href='/'>Back to Home</a></body></html>");
  });

  server.on("/action", HTTP_GET, []() {
    if (!running && !paused) {
      beginLayerStep();
    } else {
      togglePause();
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}
