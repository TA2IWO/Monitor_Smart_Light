
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

// ==================================================
// DONANIM
// ==================================================

#define LED_PIN       6
#define BUTTON_PIN    5

#define TOTAL_LEDS    91
#define REAR_LEDS     69
#define BAR_START     69

#define SERIAL_RATE   115200

#define DEFAULT_BRIGHTNESS 40
#define BRIGHTNESS_STEP    8
#define LONG_PRESS_MS      600
#define STEP_INTERVAL_MS   160
#define DEBOUNCE_MS        35

const char* AP_NAME = "Monitor-Light";
const char* AP_PASS = "isik1234";

Adafruit_NeoPixel strip(
  TOTAL_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800
);

WebServer server(80);
Preferences prefs;

// ==================================================
// GLOBAL AYARLAR
// ==================================================

uint8_t currentMode = 1;
uint8_t brightness = DEFAULT_BRIGHTNESS;

uint8_t lampR = 255;
uint8_t lampG = 120;
uint8_t lampB = 10;

bool nextLongIncreases = false;
bool activeLongIncreases = false;

bool lastRawButton = HIGH;
bool stableButton = HIGH;
bool longPressActive = false;
bool brightnessChanged = false;

unsigned long debounceTime = 0;
unsigned long pressStart = 0;
unsigned long lastBrightnessStep = 0;

// ==================================================
// ADALIGHT PARSER - TÜM TANIMLAR
// ==================================================

enum ParserState {
  WAIT_A,
  WAIT_D,
  WAIT_A2,
  READ_HI,
  READ_LO,
  READ_CHECK,
  READ_PIXELS
};

ParserState parserState = WAIT_A;

uint8_t frameHi = 0;
uint8_t frameLo = 0;
uint8_t frameCheck = 0;

uint16_t expectedPixels = 0;
uint16_t pixelIndex = 0;

uint8_t colorComponent = 0;
uint8_t redValue = 0;
uint8_t greenValue = 0;
uint8_t blueValue = 0;

// ==================================================
// KALICI BELLEK
// ==================================================

void saveSettings() {
  prefs.putUChar("mode", currentMode);
  prefs.putUChar("bright", brightness);
  prefs.putUChar("red", lampR);
  prefs.putUChar("green", lampG);
  prefs.putUChar("blue", lampB);
}

// ==================================================
// LED MODLARI
// ==================================================

void renderMode() {
  strip.clear();

  // Mod 1: Yalnızca monitör barı
  if (currentMode == 1 || currentMode == 3) {
    for (int i = BAR_START; i < TOTAL_LEDS; i++) {
      strip.setPixelColor(
        i, strip.Color(lampR, lampG, lampB)
      );
    }
  }

  // Mod 2: Yalnızca arka LED'ler
  if (currentMode == 2 || currentMode == 3) {
    for (int i = 0; i < REAR_LEDS; i++) {
      strip.setPixelColor(
        i, strip.Color(lampR, lampG, lampB)
      );
    }
  }

  // Mod 4: Hyperion, gelen karelerle LED'leri doldurur.
  strip.setBrightness(brightness);
  strip.show();
}

void changeMode() {
  currentMode++;

  if (currentMode > 4) {
    currentMode = 1;
  }

  renderMode();
  saveSettings();
}

// ==================================================
// PARLAKLIK
// ==================================================

void adjustBrightness() {
  int value = brightness;

  if (activeLongIncreases) {
    value += BRIGHTNESS_STEP;
  } else {
    value -= BRIGHTNESS_STEP;
  }

  value = constrain(value, 0, 255);

  if (value != brightness) {
    brightness = (uint8_t)value;
    brightnessChanged = true;

    renderMode();
  }
}

// ==================================================
// BUTON
// ==================================================

void handleButton() {
  unsigned long now = millis();
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastRawButton) {
    lastRawButton = reading;
    debounceTime = now;
  }

  if (now - debounceTime < DEBOUNCE_MS) {
    return;
  }

  if (reading != stableButton) {
    stableButton = reading;

    if (stableButton == LOW) {
      pressStart = now;
      lastBrightnessStep = now;
      longPressActive = false;
      brightnessChanged = false;
    } else {
      if (!longPressActive) {
        changeMode();
      } else if (brightnessChanged) {
        saveSettings();
      }

      longPressActive = false;
    }
  }

  if (stableButton == LOW && currentMode != 4) {
    if (!longPressActive &&
        now - pressStart >= LONG_PRESS_MS) {

      longPressActive = true;
      activeLongIncreases = nextLongIncreases;
      nextLongIncreases = !nextLongIncreases;

      adjustBrightness();
      lastBrightnessStep = now;
    }

    if (longPressActive &&
        now - lastBrightnessStep >= STEP_INTERVAL_MS) {

      adjustBrightness();
      lastBrightnessStep = now;
    }
  }
}

// ==================================================
// WEB ARAYÜZÜ
// ==================================================

const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monitor Light</title>
<style>
*{box-sizing:border-box}
body{
 margin:0;padding:22px 14px;background:#111318;
 color:#f5f5f7;font-family:Arial,sans-serif;text-align:center
}
.card{
 max-width:420px;margin:auto;padding:22px;
 background:#1d2028;border-radius:20px
}
h2{margin-top:0}
canvas{
 width:min(82vw,300px);height:min(82vw,300px);
 touch-action:manipulation
}
#preview{
 width:58px;height:58px;border-radius:50%;
 margin:12px auto;border:3px solid #ffffff40
}
#hex{font-size:18px;letter-spacing:1px;margin:10px}
button{
 border:0;border-radius:10px;padding:12px 14px;
 margin:5px;background:#363b49;color:white;font-size:14px
}
.small{color:#b8bdca;font-size:13px;line-height:1.5}
</style>
</head>
<body>
<div class="card">
<h2>Monitor Light</h2>
<p class="small">Renk çemberinden bir renk seç</p>
<canvas id="wheel" width="300" height="300"></canvas>
<div id="preview" style="background:#ff780a"></div>
<div id="hex">#FF780A</div>
<button onclick="sendColor()">Rengi kaydet ve uygula</button>
<p id="status" class="small">Hazır</p>
<hr style="border-color:#363b49;margin:22px 0">
<p>Çalışma modu</p>
<button onclick="setMode(1)">Bar</button>
<button onclick="setMode(2)">Arka LED</button>
<button onclick="setMode(3)">Tümü</button>
<button onclick="setMode(4)">Hyperion</button>
<p class="small">Wi-Fi: Monitor-Light</p>
</div>
<script>
const canvas=document.getElementById('wheel');
const ctx=canvas.getContext('2d');
const W=300,C=150,R=148;
let selected={r:255,g:120,b:10};

function drawWheel(){
 const img=ctx.createImageData(W,W);
 for(let y=0;y<W;y++){
  for(let x=0;x<W;x++){
   const dx=x-C,dy=y-C;
   const dist=Math.hypot(dx,dy),p=(y*W+x)*4;
   if(dist>R){img.data[p+3]=0;continue}
   const hue=(Math.atan2(dy,dx)*180/Math.PI+360)%360;
   const sat=dist/R,h=hue/60,i=Math.floor(h),f=h-i;
   const q=1-f,t=f;
   let rgb;
   switch(i%6){
    case 0:rgb=[1,t,0];break;
    case 1:rgb=[q,1,0];break;
    case 2:rgb=[0,1,t];break;
    case 3:rgb=[0,q,1];break;
    case 4:rgb=[t,0,1];break;
    default:rgb=[1,0,q];
   }
   img.data[p]=Math.round((1-sat+sat*rgb[0])*255);
   img.data[p+1]=Math.round((1-sat+sat*rgb[1])*255);
   img.data[p+2]=Math.round((1-sat+sat*rgb[2])*255);
   img.data[p+3]=255;
  }
 }
 ctx.putImageData(img,0,0);
}

function updatePreview(){
 const hex='#'+[selected.r,selected.g,selected.b]
  .map(v=>v.toString(16).padStart(2,'0')).join('')
  .toUpperCase();
 document.getElementById('preview').style.background=hex;
 document.getElementById('hex').textContent=hex;
}

function pick(e){
 const rect=canvas.getBoundingClientRect();
 const x=(e.clientX-rect.left)*W/rect.width;
 const y=(e.clientY-rect.top)*W/rect.height;
 if(Math.hypot(x-C,y-C)>R)return;
 const d=ctx.getImageData(Math.floor(x),Math.floor(y),1,1).data;
 selected={r:d[0],g:d[1],b:d[2]};
 updatePreview();
}

canvas.addEventListener('pointerdown',pick);
drawWheel();
updatePreview();

async function sendColor(){
 const rgb=[selected.r,selected.g,selected.b]
  .map(v=>v.toString(16).padStart(2,'0')).join('');
 const status=document.getElementById('status');
 status.textContent='Kaydediliyor...';
 try{
  const r=await fetch('/color?rgb='+rgb);
  status.textContent=r.ok?'Renk kaydedildi.':'Hata oluştu.';
 }catch(e){status.textContent='Bağlantı hatası.'}
}

async function setMode(m){
 try{
  const r=await fetch('/mode?m='+m);
  document.getElementById('status').textContent=
   r.ok?'Mod '+m+' seçildi.':'Mod değiştirilemedi.';
 }catch(e){
  document.getElementById('status').textContent='Bağlantı hatası.';
 }
}
</script>
</body>
</html>
)HTML";

// ==================================================
// WEB SUNUCUSU
// ==================================================

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handleColor() {
  if (!server.hasArg("rgb")) {
    server.send(400, "text/plain", "Renk eksik");
    return;
  }

  String hex = server.arg("rgb");

  if (hex.length() != 6) {
    server.send(400, "text/plain", "Gecersiz renk");
    return;
  }

  char* endPtr;
  unsigned long value = strtoul(hex.c_str(), &endPtr, 16);

  if (*endPtr != '\0') {
    server.send(400, "text/plain", "Gecersiz renk");
    return;
  }

  lampR = (value >> 16) & 0xFF;
  lampG = (value >> 8) & 0xFF;
  lampB = value & 0xFF;

  saveSettings();

  if (currentMode != 4) {
    renderMode();
  }

  server.send(200, "text/plain", "OK");
}

void handleWebMode() {
  if (!server.hasArg("m")) {
    server.send(400, "text/plain", "Mod eksik");
    return;
  }

  int m = server.arg("m").toInt();

  if (m < 1 || m > 4) {
    server.send(400, "text/plain", "Gecersiz mod");
    return;
  }

  currentMode = m;
  renderMode();
  saveSettings();

  server.send(200, "text/plain", "OK");
}

void startWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  delay(200);

  if (!WiFi.softAP(AP_NAME, AP_PASS)) {
    return;
  }

  server.on("/", handleRoot);
  server.on("/color", handleColor);
  server.on("/mode", handleWebMode);
  server.begin();
}

// ==================================================
// ADALIGHT PARSER
// ==================================================

void resetParser() {
  parserState = WAIT_A;
}

void processAdalightByte(uint8_t incoming) {
  switch (parserState) {
    case WAIT_A:
      if (incoming == 'A') parserState = WAIT_D;
      break;

    case WAIT_D:
      if (incoming == 'd') parserState = WAIT_A2;
      else if (incoming != 'A') parserState = WAIT_A;
      break;

    case WAIT_A2:
      if (incoming == 'a') parserState = READ_HI;
      else if (incoming == 'A') parserState = WAIT_D;
      else parserState = WAIT_A;
      break;

    case READ_HI:
      frameHi = incoming;
      parserState = READ_LO;
      break;

    case READ_LO:
      frameLo = incoming;
      parserState = READ_CHECK;
      break;

    case READ_CHECK:
      frameCheck = incoming;

      if (frameCheck != (frameHi ^ frameLo ^ 0x55)) {
        resetParser();
        break;
      }

      expectedPixels =
        (((uint16_t)frameHi << 8) | frameLo) + 1;

      if (expectedPixels == 0) {
        resetParser();
        break;
      }

      pixelIndex = 0;
      colorComponent = 0;
      parserState = READ_PIXELS;
      break;

    case READ_PIXELS:
      if (pixelIndex < TOTAL_LEDS) {
        if (colorComponent == 0) {
          redValue = incoming;
        } else if (colorComponent == 1) {
          greenValue = incoming;
        } else {
          blueValue = incoming;

          strip.setPixelColor(
            pixelIndex,
            strip.Color(redValue, greenValue, blueValue)
          );
        }
      }

      colorComponent++;

      if (colorComponent >= 3) {
        colorComponent = 0;
        pixelIndex++;
      }

      if (pixelIndex >= expectedPixels) {
        if (currentMode == 4) {
          strip.setBrightness(brightness);
          strip.show();
        }

        resetParser();
      }
      break;
  }
}

void handleHyperion() {
  while (Serial.available() > 0) {
    processAdalightByte((uint8_t)Serial.read());
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // USB seri portu Hyperion/Adalight içindir; buraya debug metni yazdırma.
  Serial.begin(SERIAL_RATE);

  strip.begin();
  strip.clear();
  strip.show();

  prefs.begin("monitorLED", false);

  currentMode = prefs.getUChar("mode", 1);
  brightness = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);

  lampR = prefs.getUChar("red", 255);
  lampG = prefs.getUChar("green", 120);
  lampB = prefs.getUChar("blue", 10);

  if (currentMode < 1 || currentMode > 4) {
    currentMode = 1;
  }

  renderMode();
  startWiFi();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  handleButton();
  server.handleClient();
  handleHyperion();
}
