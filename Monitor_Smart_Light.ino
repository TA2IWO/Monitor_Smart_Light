
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
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
DNSServer dnsServer;
Preferences prefs;

IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

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
body{margin:0;padding:18px 12px;background:#111318;color:#f5f5f7;font-family:Arial,sans-serif;text-align:center}
.card{max-width:440px;margin:auto;padding:20px;background:#1d2028;border-radius:20px}
h2{margin:0 0 8px}
.small{color:#b8bdca;font-size:13px;line-height:1.5}
canvas{width:min(82vw,290px);height:min(82vw,290px);touch-action:manipulation;display:block;margin:12px auto}
#preview{width:64px;height:64px;border-radius:14px;margin:12px auto 8px;border:2px solid #ffffff55}
#hex{width:125px;text-align:center;text-transform:uppercase;font-size:17px;letter-spacing:1px;background:#101218;color:white;border:1px solid #4a5060;border-radius:8px;padding:9px}
.rgbrow{display:grid;grid-template-columns:24px 1fr 42px;align-items:center;gap:8px;margin:12px 0;text-align:left}
.rgbrow label{font-weight:bold}
.rgbrow output{text-align:right;font-variant-numeric:tabular-nums;color:#e3e6ee}
input[type=range]{width:100%;margin:0;accent-color:#d9dce5}
button{border:0;border-radius:10px;padding:12px 13px;margin:5px;background:#363b49;color:white;font-size:14px}
button.primary{background:#5367d9;width:100%;font-weight:bold}
button:active{transform:scale(.98)}
hr{border:0;border-top:1px solid #363b49;margin:22px 0}
</style>
</head>
<body>
<div class="card">
<h2>Monitor Light</h2>
<p class="small">Rengi çemberden seç veya RGB değerlerini hassas ayarla.</p>
<canvas id="wheel" width="290" height="290"></canvas>
<div id="preview"></div>
<div style="margin:8px 0 14px">
<label for="hex">HEX&nbsp; #</label>
<input id="hex" value="FF780A" maxlength="6" autocomplete="off" spellcheck="false">
</div>
<div class="rgbrow"><label for="r">R</label><input id="r" type="range" min="0" max="255" value="255"><output id="rv">255</output></div>
<div class="rgbrow"><label for="g">G</label><input id="g" type="range" min="0" max="255" value="120"><output id="gv">120</output></div>
<div class="rgbrow"><label for="b">B</label><input id="b" type="range" min="0" max="255" value="10"><output id="bv">10</output></div>
<button class="primary" onclick="sendColor()">Rengi kaydet ve uygula</button>
<p id="status" class="small">Hazır</p>
<hr>
<p>Çalışma modu</p>
<button onclick="setMode(1)">Bar</button>
<button onclick="setMode(2)">Arka LED</button>
<button onclick="setMode(3)">Tümü</button>
<button onclick="setMode(4)">Hyperion</button>
<p class="small">Wi-Fi: Monitor-Light</p>
</div>
<script>
const canvas=document.getElementById('wheel'),ctx=canvas.getContext('2d');
const W=canvas.width,C=W/2,R=C-2;
const fields={r:document.getElementById('r'),g:document.getElementById('g'),b:document.getElementById('b')};
const outputs={r:document.getElementById('rv'),g:document.getElementById('gv'),b:document.getElementById('bv')};
const hexInput=document.getElementById('hex');
let selected={r:255,g:120,b:10};
function rgbToHex(){return [selected.r,selected.g,selected.b].map(v=>v.toString(16).padStart(2,'0')).join('').toUpperCase()}
function syncUI(){
 for(const k of ['r','g','b']){fields[k].value=selected[k];outputs[k].value=selected[k]}
 const hex=rgbToHex();hexInput.value=hex;
 document.getElementById('preview').style.background='#'+hex;
 document.getElementById('preview').style.boxShadow='0 0 22px #'+hex+'66';
}
function drawWheel(){
 const img=ctx.createImageData(W,W);
 for(let y=0;y<W;y++)for(let x=0;x<W;x++){
  const dx=x-C,dy=y-C,dist=Math.hypot(dx,dy),p=(y*W+x)*4;
  if(dist>R){img.data[p+3]=0;continue}
  const hue=(Math.atan2(dy,dx)*180/Math.PI+360)%360;
  const sat=Math.min(1,dist/R),h=hue/60,i=Math.floor(h),f=h-i;
  const q=1-f,t=f;let rgb;
  switch(i%6){case 0:rgb=[1,t,0];break;case 1:rgb=[q,1,0];break;case 2:rgb=[0,1,t];break;case 3:rgb=[0,q,1];break;case 4:rgb=[t,0,1];break;default:rgb=[1,0,q]}
  img.data[p]=Math.round((1-sat+sat*rgb[0])*255);
  img.data[p+1]=Math.round((1-sat+sat*rgb[1])*255);
  img.data[p+2]=Math.round((1-sat+sat*rgb[2])*255);
  img.data[p+3]=255;
 }
 ctx.putImageData(img,0,0);
}
function pick(e){
 const rect=canvas.getBoundingClientRect(),x=(e.clientX-rect.left)*W/rect.width,y=(e.clientY-rect.top)*W/rect.height;
 if(Math.hypot(x-C,y-C)>R)return;
 const d=ctx.getImageData(Math.floor(x),Math.floor(y),1,1).data;
 selected={r:d[0],g:d[1],b:d[2]};syncUI();
}
for(const k of ['r','g','b'])fields[k].addEventListener('input',()=>{selected[k]=Number(fields[k].value);syncUI()});
hexInput.addEventListener('input',()=>{
 const s=hexInput.value.trim().replace(/^#/,'');
 if(/^[0-9a-fA-F]{6}$/.test(s)){selected={r:parseInt(s.slice(0,2),16),g:parseInt(s.slice(2,4),16),b:parseInt(s.slice(4,6),16)};syncUI()}
});
hexInput.addEventListener('change',()=>syncUI());
canvas.addEventListener('pointerdown',pick);
drawWheel();syncUI();
async function sendColor(){
 const status=document.getElementById('status');status.textContent='Kaydediliyor...';
 try{const r=await fetch('/color?rgb='+rgbToHex());status.textContent=r.ok?'Renk kaydedildi.':'Hata oluştu.'}
 catch(e){status.textContent='Bağlantı hatası.'}
}
async function setMode(m){
 try{const r=await fetch('/mode?m='+m);document.getElementById('status').textContent=r.ok?'Mod '+m+' seçildi.':'Mod değiştirilemedi.'}
 catch(e){document.getElementById('status').textContent='Bağlantı hatası.'}
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

  WiFi.softAPConfig(apIP, gateway, subnet);

  if (!WiFi.softAP(AP_NAME, AP_PASS)) {
    return;
  }

  // Captive portal: connected devices are directed to the local control page.
  dnsServer.start(53, "*", apIP);

  server.on("/", handleRoot);
  server.on("/color", handleColor);
  server.on("/mode", handleWebMode);
  server.onNotFound(handleRoot);
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
  dnsServer.processNextRequest();
  server.handleClient();
  handleHyperion();
}
