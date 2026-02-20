#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

const char* ssid= "YOUR_WiFi_NETWORK";  //replace ssid and password for it to connect to the network
const char* password= "YOUR_WiFi_PASSWORD";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Motor pins
int m1a = 27;
int m1b = 26;
int m2a = 32;
int m2b = 33;

// ---------- PWM (Motor 1 Soft Ramp) ----------
const int pwmChannelA = 4;
const int pwmChannelB = 5;
const int pwmFreq = 1000;
const int pwmResolution = 8;

int currentSpeed = 0;
int targetSpeed = 0;
bool directionForward = true;

const int stepSize = 5;
const int maxSpeed = 200;

// -------- Driver Sleep --------
int sleepPin = 25;

// -------- Servo --------
Servo myServo;
int servoPin = 13;

// -------- LED Flash --------

// -------- Servo --------
void moveServo(int angle) {
  myServo.write(angle);
  Serial.print("Servo -> ");
  Serial.println(angle);
}

// -------- Non-Blocking Soft Ramp --------
void updateMotor() {

  // Wake driver if motor is moving
  if (targetSpeed > 0 || currentSpeed > 0) {
    digitalWrite(sleepPin, HIGH);
  } else {
    digitalWrite(sleepPin, LOW);
  }

  if (currentSpeed < targetSpeed) {
    currentSpeed += stepSize;
    if (currentSpeed > targetSpeed)
      currentSpeed = targetSpeed;
  }

  if (currentSpeed > targetSpeed) {
    currentSpeed -= stepSize;
    if (currentSpeed < targetSpeed)
      currentSpeed = targetSpeed;
  }

  if (directionForward) {
    ledcWrite(pwmChannelB, 0);
    ledcWrite(pwmChannelA, currentSpeed);
  } else {
    ledcWrite(pwmChannelA, 0);
    ledcWrite(pwmChannelB, currentSpeed);
  }
}


// -------- Motor Control --------
void stopMotors() {
  targetSpeed = 0;
  digitalWrite(m2a, LOW);
  digitalWrite(m2b, LOW);
  Serial.println("stopped");
}


void forward() {
  directionForward = true;
  targetSpeed = maxSpeed;
  Serial.println("going forward");
}

void back() {
  directionForward = false;
  targetSpeed = maxSpeed;
  Serial.println("going backward");
}

void turn_left() {
  digitalWrite(sleepPin, HIGH);
  digitalWrite(m2a, HIGH);
  digitalWrite(m2b, LOW);
  Serial.println("rotating superstructure left");
}

void turn_right() {
  digitalWrite(sleepPin, HIGH);
  digitalWrite(m2a, LOW);
  digitalWrite(m2b, HIGH);
  Serial.println("rotating superstructure right");
}


// -------- WebSocket --------
void onWebSocketEvent(AsyncWebSocket * server,
                      AsyncWebSocketClient * client,
                      AwsEventType type,
                      void * arg,
                      uint8_t *data,
                      size_t len) {

  if (type == WS_EVT_DATA) {

    String msg = "";
    for (size_t i = 0; i < len; i++) {
      msg += (char)data[i];
    }

    if (msg == "f") forward();
    else if (msg == "b") back();
    else if (msg == "l") turn_left();
    else if (msg == "r") turn_right();
    else if (msg == "s") stopMotors();

    else if (msg.startsWith("servo:")) {
      int angle = msg.substring(6).toInt();
      moveServo(angle);
    }

  }
}



// -------- Web Page (unchanged) --------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { background:#111; color:white; text-align:center; font-family:Arial; }
button { width:90px; height:60px; font-size:20px; margin:6px; border-radius:12px; border:none; }
.stopBtn { background:#c0392b; color:white; }
.driverOn { background:#27ae60; color:white; }
.driverOff { background:#555; color:white; }
</style>
</head>
<body>

<h2>ESP32 WebSocket Control</h2>

<button data-key="q" data-cmd="l">Q</button>
<button data-key="w" data-cmd="f">W</button>
<button data-key="e" data-cmd="r">E</button><br>
<button data-key="a" data-servo="20">A</button>
<button class="stopBtn" id="stopBtn" data-key="s">S</button>
<button data-key="d" data-servo="160">D</button><br>
<button data-key="x" data-cmd="b">X</button><br><br>


<script>
let socket = new WebSocket(`ws://${location.host}/ws`);
socket.onopen = () => console.log("WS Connected");

// Generic send functions
function send(cmd){
  if (socket.readyState === WebSocket.OPEN) socket.send(cmd);
}

function sendServo(angle){
  send("servo:" + angle);
}

function emergencyStop(){
  send("s");
  sendServo(90);
}

// Select all buttons
const buttons = document.querySelectorAll('button');

// Pointer / Mouse events
buttons.forEach(btn => {
  btn.addEventListener('pointerdown', () => handlePress(btn));
  btn.addEventListener('pointerup', () => handleRelease(btn));
  btn.addEventListener('mousedown', () => handlePress(btn));
  btn.addEventListener('mouseup', () => handleRelease(btn));
});

// Keyboard events on document
const activeKeys = new Set();

document.addEventListener('keydown', e => {
  const key = e.key.toLowerCase();

  // Only allow these keys
  if (!["q","w","e","a","s","d","x"].includes(key)) return;

  // Ignore auto-repeat
  if (e.repeat) return;

  // Prevent multiple triggers
  if (activeKeys.has(key)) return;

  activeKeys.add(key);

  const btn = document.querySelector(`button[data-key="${key}"]`);
  if (btn) handlePress(btn);
});

document.addEventListener('keyup', e => {
  const key = e.key.toLowerCase();

  if (!activeKeys.has(key)) return;

  activeKeys.delete(key);

  const btn = document.querySelector(`button[data-key="${key}"]`);
  if (btn) handleRelease(btn);
});


function handlePress(btn){
  if(btn.dataset.cmd) send(btn.dataset.cmd);
  if(btn.dataset.servo) sendServo(btn.dataset.servo);
}

function handleRelease(btn){
  // Send stop command and set servo to 90°
  emergencyStop();
  console.log(btn.innerText + " released → STOP + Servo 90°");
}
</script>



</body>
</html>
)rawliteral";


void setup() {

  Serial.begin(115200);
  // PWM Setup
  ledcSetup(pwmChannelA, pwmFreq, pwmResolution);
  ledcAttachPin(m1a, pwmChannelA);

  ledcSetup(pwmChannelB, pwmFreq, pwmResolution);
  ledcAttachPin(m1b, pwmChannelB);

  ledcWrite(pwmChannelA, 0);
  ledcWrite(pwmChannelB, 0);

  // Second motor pins
  pinMode(m2a, OUTPUT);
  pinMode(m2b, OUTPUT);

  myServo.attach(servoPin);
  myServo.write(90);

  pinMode(sleepPin, OUTPUT);
  digitalWrite(sleepPin, LOW);

  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(750);
  }

  Serial.println(WiFi.localIP());

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
  updateMotor();
  delay(10);
}