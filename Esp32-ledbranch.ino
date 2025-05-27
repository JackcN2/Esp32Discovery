#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>

const char* ssid = "LED_Controller";

const int redPin = 27;
const int greenPin = 14;
const int bluePin = 12;

AsyncWebServer server(80);
Ticker animationTicker;

enum Mode { STATIC, RAINBOW, PULSE };
Mode currentMode = STATIC;

int animStep = 0;
int baseR = 255, baseG = 0, baseB = 0;

void setColor(int r, int g, int b) {
  analogWrite(redPin, r);
  analogWrite(greenPin, g);
  analogWrite(bluePin, b);
}

void stopAnimation() {
  animationTicker.detach();
  currentMode = STATIC;
}

void rainbow() {
  animStep = (animStep + 1) % 256;
  int r = (sin(animStep * 0.024 + 0) * 127) + 128;
  int g = (sin(animStep * 0.024 + 2) * 127) + 128;
  int b = (sin(animStep * 0.024 + 4) * 127) + 128;
  setColor(r, g, b);
}

void pulse() {
  float factor = (sin(animStep * 0.1) + 1.0) / 2.0;
  int r = baseR * factor;
  int g = baseG * factor;
  int b = baseB * factor;
  setColor(r, g, b);
  animStep++;
}

void startAnimation(Mode mode) {
  stopAnimation();
  currentMode = mode;
  animStep = 0;

  switch (mode) {
    case RAINBOW:
      animationTicker.attach_ms(30, rainbow);
      break;
    case PULSE:
      animationTicker.attach_ms(30, pulse);
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  WiFi.softAP(ssid);
  Serial.println("AP Started");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>LED Controller</title>
  <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><circle cx='50' cy='50' r='40' fill='%23ffcc00'/></svg>">
  <style>
    body {
      background: black; color: white; font-family: sans-serif;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      height: 100vh; margin: 0;
    }
    h1 { margin-bottom: 20px; }
    .buttons button {
      margin: 10px; padding: 12px 24px;
      font-size: 16px; border: none; border-radius: 8px;
      background: #444; color: white; cursor: pointer;
      transition: background 0.3s;
    }
    .buttons button:hover {
      background: #666;
    }
    .slider-container {
      width: 80%; max-width: 300px; text-align: center;
    }
    input[type=range] {
      width: 100%;
    }
  </style>
</head>
<body>
  <h1>LED Controller</h1>
  <div id="colorWheel"></div>
  <div class="buttons">
    <button onclick="sendMode('rainbow')">Rainbow</button>
    <button onclick="sendMode('pulse')">Pulse</button>
  </div>
  <div class="slider-container">
    <label>Brightness</label>
    <input type="range" id="brightness" min="0" max="255" value="255">
    <label>Saturation</label>
    <input type="range" id="saturation" min="0" max="255" value="255">
  </div>

  <script src="https://cdn.jsdelivr.net/npm/@jaames/iro@5"></script>
  <script>
    const wheel = new iro.ColorPicker("#colorWheel", {
      width: 250,
      color: "#ff0000",
      layout: [ { component: iro.ui.Wheel } ]
    });

    const brightnessSlider = document.getElementById("brightness");
    const saturationSlider = document.getElementById("saturation");

    function updateColor() {
      const c = wheel.color.rgb;
      const brightness = brightnessSlider.value;
      const saturation = saturationSlider.value;
      fetch(`/setColor?color=${c.r},${c.g},${c.b}&brightness=${brightness}&saturation=${saturation}`);
    }

    function sendMode(mode) {
      const c = wheel.color.rgb;
      const brightness = brightnessSlider.value;
      const saturation = saturationSlider.value;
      if (mode === "pulse") {
        fetch(`/setColor?color=${c.r},${c.g},${c.b}&brightness=${brightness}&saturation=${saturation}`)
          .then(() => fetch(`/mode?type=${mode}`));
      } else {
        fetch(`/mode?type=${mode}`);
      }
    }

    wheel.on("color:change", updateColor);
    brightnessSlider.addEventListener("input", updateColor);
    saturationSlider.addEventListener("input", updateColor);
  </script>
</body>
</html>
    )rawliteral");
  });

  server.on("/setColor", HTTP_GET, [](AsyncWebServerRequest *request) {
    stopAnimation();
    String color = request->getParam("color")->value();
    int brightness = request->getParam("brightness")->value().toInt();
    int saturation = request->getParam("saturation")->value().toInt();

    int r, g, b;
    sscanf(color.c_str(), "%d,%d,%d", &r, &g, &b);

    float s = saturation / 255.0;
    r = r + (255 - r) * (1.0 - s);
    g = g + (255 - g) * (1.0 - s);
    b = b + (255 - b) * (1.0 - s);

    r = r * brightness / 255;
    g = g * brightness / 255;
    b = b * brightness / 255;

    r = constrain(r, 0, 255);
    g = constrain(g, 0, 255);
    b = constrain(b, 0, 255);

    setColor(r, g, b);

    baseR = r;
    baseG = g;
    baseB = b;

    request->send(200, "text/plain", "Color set");
  });

  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("type")) {
      String mode = request->getParam("type")->value();
      if (mode == "rainbow") startAnimation(RAINBOW);
      else if (mode == "pulse") startAnimation(PULSE);
    }
    request->send(200, "text/plain", "Mode set");
  });

  server.begin();
}

void loop() {}
