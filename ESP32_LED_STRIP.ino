#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h> // dont remove Over The Air Updates ---------------------------------------------------------------------------------

// NOTE when uploading new code over the air 
// you have to enter the network password

const char * ssid = "SSID";
const char * password = "PASSWORD";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

const int BUILTIN_LED = 2;

const int RED_PIN = 25;
const int GREEN_PIN = 32;
const int BLUE_PIN = 33;

int redValue = 0;
int greenValue = 0;
int blueValue = 0;

StaticJsonDocument<200> doc_tx;
StaticJsonDocument<200> doc_rx;

const char webpage[] = R"---(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED-slinga</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            background-color: #333; /* Dark grey background */
        }
        .color-picker-container {
            text-align: center;
            padding: 20px;
            background-color: #444; /* Slightly lighter dark grey for the container */
            border-radius: 8px;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.3);
            width: 350px;
        }
        canvas {
            /*cursor: crosshair; */
            border-radius: 50%;
        }
        #colorDisplay {
            width: 100px;
            height: 100px;
            margin-top: 20px;
            border-radius: 50%;
            border: 2px solid #222;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
        }
        #indicator {
            position: absolute;
            width: 12px;
            height: 12px;
            border: 2px solid white;
            background-color: #555; /* Dark grey indicator */
            border-radius: 50%;
            pointer-events: none;
            box-shadow: 0 0 5px rgba(0, 0, 0, 0.3);
        }
        #offButton {
            width: 160px;
            height: 80px;
            font-size: 30px;
            border: 2px solid white;
            background-color: #555; /* Dark grey button */
            border-radius: 12px; /* Rounded edges */
            margin-left: 20px;
            color: white;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
        }
        .flex-container {
            display: flex;
            align-items: center;
            justify-content: center;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="color-picker-container">
        <h2 style="color: #fff;">Välj en färg:</h2> <!-- White text for better contrast -->
        <div id="indicator"></div>
        <canvas id="colorWheel" width="300" height="300"></canvas>
        <div class="flex-container"></div>
            <div id="colorDisplay"></div>
            <button id="offButton">STÄNG AV</button>
        </div>
    </div>

    <script>
        var Socket;
        const canvas = document.getElementById('colorWheel');
        const context = canvas.getContext('2d');
        const colorDisplay = document.getElementById('colorDisplay');
        const indicator = document.getElementById('indicator');
        let isDragging = false;
		const radius = canvas.width / 2;
        const center = { x: radius, y: radius };

        function init() {
            Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
            Socket.onmessage = function(event) {
                processCommand(event);
            };
        }

        // Function to draw the color wheel
        function drawColorWheel() {
            const gradient = context.createConicGradient(0, center.x, center.y);
            
            for (let i = 0; i < 360; i++) {
                gradient.addColorStop(i / 360, `hsl(${i}, 100%, 50%)`);
            }
            
            context.clearRect(0, 0, canvas.width, canvas.height);
            context.beginPath();
            context.arc(center.x, center.y, radius, 0, 2 * Math.PI);
            context.fillStyle = gradient;
            context.fill();
        }

        // Draw the color wheel on load
        drawColorWheel();

        // Get the color from the wheel based on mouse position
        function getColorFromWheel(event) {
            const rect = canvas.getBoundingClientRect();
            const x = event.clientX - rect.left;
            const y = event.clientY - rect.top;
            const imageData = context.getImageData(x, y, 1, 1).data;
            const [r, g, b] = imageData;
            const hexColor = rgbToHex(r, g, b);

            // colorDisplay.style.backgroundColor = hexColor;

            // Position the indicator circle
            indicator.style.left = `${event.clientX - 8}px`;
            indicator.style.top = `${event.clientY - 8}px`;
            
            const colorString = `${r},${g},${b}`;
            Socket.send(colorString);
        }

        // Convert RGB to HEX
        function rgbToHex(r, g, b) {
            return `#${((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1)}`;
        }

        // Mouse events to handle drag-and-select functionality
        canvas.addEventListener('mousedown', (event) => {
            isDragging = true;
            getColorFromWheel(event);
        });

        canvas.addEventListener('mousemove', (event) => {
            if (isDragging) {
                getColorFromWheel(event);
            }
        });

        canvas.addEventListener('mouseup', () => {
            isDragging = false;
        });

        canvas.addEventListener('mouseleave', () => {
            isDragging = false;
        });

        document.getElementById('offButton').addEventListener('click', function() {
            const colorString = `0,0,0`;
            Socket.send(colorString);
            console.log("Sent RGB values:", colorString);
        });

        function processCommand(event) {
            let colorData = JSON.parse(event.data);

            let colorString = colorData.color;
            let rgbParts = colorString.match(/R:(\d+),G:(\d+),B:(\d+)/);

            let redValue = parseInt(rgbParts[1]);
            let greenValue = parseInt(rgbParts[2]);
            let blueValue = parseInt(rgbParts[3]);

            let hexColor = rgbToHex(redValue, greenValue, blueValue);

            colorDisplay.style.backgroundColor = hexColor;
        }

        window.onload = function(event) {
            init();
        }
    </script>
</body>
</html>
)---";

void setup() {
  setCpuFrequencyMhz(80);  // Lower the clock speed to 80 MHz
  btStop();  // Disables bluetooth
  Serial.begin(115200);

  pinMode(BUILTIN_LED, OUTPUT);
  ledcAttach(RED_PIN, 2000, 8); // 12 kHz frekvens, 8-bitars upplösning // ändrade till 4 kHz
  ledcAttach(GREEN_PIN, 2000, 8);
  ledcAttach(BLUE_PIN, 2000, 8);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(500);
    digitalWrite(BUILTIN_LED, LOW);
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text\html", webpage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  ArduinoOTA.setHostname("LED-Strip ESP32");

  ArduinoOTA.begin(); // dont remove Over The Air Updates ---------------------------------------------------------------------------------

}

void loop() {

  ArduinoOTA.handle(); // dont remove Over The Air Updates ---------------------------------------------------------------------------------

  server.handleClient();
  webSocket.loop();

  ledcWrite(RED_PIN, redValue);
  ledcWrite(GREEN_PIN, greenValue);
  ledcWrite(BLUE_PIN, blueValue);

  String jsonString = "{\"color\": \"R:"+String(redValue)+",G:"+String(greenValue)+",B:"+String(blueValue)+"\"}";

  webSocket.broadcastTXT(jsonString);
  delay(50);

}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {

  String payloadString = (const char *)payload;

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Client Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Client Connected");
      break;
    case WStype_TEXT:
      {
        // Extract RGB values from payloadString
        int r, g, b;
        sscanf(payloadString.c_str(), "%d,%d,%d", &r, &g, &b);
        
        // Update RGB values
        redValue = r;
        greenValue = g;
        blueValue = b;
        
        Serial.printf("Received RGB values - R: %d, G: %d, B: %d\n", r, g, b);
      }
      break;
  }

}
