/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-mpu-6050-web-server/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid = "G6_2774";
const char* password = "senhaforte";
const int WIFI_CHANNEL = 6; // Speeds up the connection in Wokwi

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;  
unsigned long lastTimeTemperature = 0;
unsigned long lastTimeAcc = 0;
unsigned long gyroDelay = 10;
unsigned long temperatureDelay = 1000;
unsigned long accelerometerDelay = 200;

// Create a sensor object
const int MPU=0x68;

sensors_event_t a, g, temp;

float gyroX, gyroY, gyroZ;
float accX, accY, accZ;
float temperature;

//Gyroscope sensor deviation
float gyroXerror = 0.07;
float gyroYerror = 0.03;
float gyroZerror = 0.01;

#define PI 3.1415926535897932384626433832795

// Init MPU6050
void initMPU(){
    // Set Gyroscope Full-Scale Range to +/- 500 deg/s
  Wire.beginTransmission(MPU);
  Wire.write(0x1B);
  Wire.write(0x00); // FS_SEL = 0 (250 deg/s)
  Wire.endTransmission(true);

  // Set Accelerometer Full-Scale Range to +/- 4g
  Wire.beginTransmission(MPU);
  Wire.write(0x1C);
  Wire.write(0x08); // AFS_SEL = 1 (4g)
  Wire.endTransmission(true);
  
  //Inicializa o MPU-6050
  Wire.write(0); 
  Wire.endTransmission(true);
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password, WIFI_CHANNEL);
  Serial.println("");
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

String getGyroReadings(){
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  //Solicita os dados do sensor
  Wire.requestFrom(MPU,14,true);  

  float gyroX_temp_raw = Wire.read()<<8|Wire.read();
  float gyroY_temp_raw = Wire.read()<<8|Wire.read();
  float gyroZ_temp_raw = Wire.read()<<8|Wire.read();

  // Sensitivity scale factor for +/- 250 deg/s is 131.0
  // Convert raw values to degrees per second
  float gyroX_temp = (gyroX_temp_raw / 131.0) * (PI / 180.0);
  float gyroY_temp = (gyroY_temp_raw / 131.0) * (PI / 180.0);
  float gyroZ_temp = (gyroZ_temp_raw / 131.0) * (PI / 180.0);

  if(abs(gyroX_temp) > gyroXerror) {
    gyroX += gyroX_temp/50.00;
  }

  if(abs(gyroY_temp) > gyroYerror) {
    gyroY += gyroY_temp/70.00;
  }
  if(abs(gyroZ_temp) > gyroZerror) {
    gyroZ += gyroZ_temp/90.00;  
  }

  readings["gyroX"] = String(gyroX);
  readings["gyroY"] = String(gyroY);
  readings["gyroZ"] = String(gyroZ);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

String getAccReadings() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  //Solicita os dados do sensor
  Wire.requestFrom(MPU,14,true);  
  // Get current acceleration values
  accX = (Wire.read()<<8|Wire.read()) / 8192.0;  //0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L);
  accY = (Wire.read()<<8|Wire.read())  / 8192.0;  //0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L);
  accZ = (Wire.read()<<8|Wire.read())  / 8192.0;  //0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L);
  readings["accX"] = String(accX);
  readings["accY"] = String(accY);
  readings["accZ"] = String(accZ);
  String accString = JSON.stringify (readings);
  return accString;
}

String getTemperature(){
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  //Solicita os dados do sensor
  Wire.requestFrom((uint8_t)MPU, (size_t)14, (bool)true);
  temperature = Wire.read()<<8|Wire.read();  //0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L);
  return String(temperature);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(41, 42); // SDA, SCL
  initWiFi();
  initMPU();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", createHtml());
  }); 

  // Rotas para dados dos sensores
  server.on("/gyro", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getGyroReadings());
  });

  server.on("/acc", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getAccReadings());
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getTemperature());
  });

  // Rotas de reset
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroX = 0; gyroY = 0; gyroZ = 0;
    request->send(200, "text/plain", "OK");
  });
  server.on("/resetX", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroX = 0;
    request->send(200, "text/plain", "OK");
  });
  server.on("/resetY", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroY = 0;
    request->send(200, "text/plain", "OK");
  });
  server.on("/resetZ", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroZ = 0;
    request->send(200, "text/plain", "OK");
  });

  server.addHandler(&events);

  server.begin();
}

void loop() {
    if ((millis() - lastTime) > gyroDelay) {
    // Send Events to the Web Server with the Sensor Readings
    events.send(getGyroReadings().c_str(),"gyro_readings",millis());
    lastTime = millis();
  }
  if ((millis() - lastTimeAcc) > accelerometerDelay) {
    // Send Events to the Web Server with the Sensor Readings
    events.send(getAccReadings().c_str(),"accelerometer_readings",millis());
    lastTimeAcc = millis();
  }
  if ((millis() - lastTimeTemperature) > temperatureDelay) {
    // Send Events to the Web Server with the Sensor Readings
    events.send(getTemperature().c_str(),"temperature_reading",millis());
    lastTimeTemperature = millis();
  }
  delay(100);
}


String createHtml() {

String html = R"rawliteral( 
<!--
        Rui Santos
        Complete project details at https://RandomNerdTutorials.com/esp32-mpu-6050-web-server/

        Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
        The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
        -->

        <!DOCTYPE HTML><html>
        <head>
        <title>ESP Web Server</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <link rel="icon" href="data:,">
        <style>
            html {
            font-family: Arial;
            display: inline-block;
            text-align: center;
            }
            p {
            font-size: 1.2rem;
            }
            body {
            margin: 0;
            }
            .topnav {
            overflow: hidden;
            background-color: #003366;
            color: #FFD43B;
            font-size: 1rem;
            }
            .content {
            padding: 20px;
            }
            .card {
            background-color: white;
            box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
            }
            .card-title {
            color:#003366;
            font-weight: bold;
            }
            .cards {
            max-width: 800px;
            margin: 0 auto;
            display: grid; grid-gap: 2rem;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            }
            .reading {
            font-size: 1.2rem;
            }
            .cube-content{
            width: 100%;
            background-color: white;
            height: 400px; margin: auto;
            padding-top:2%;
            }
            #reset{
            border: none;
            color: #FEFCFB;
            background-color: #003366;
            padding: 10px;
            text-align: center;
            display: inline-block;
            font-size: 14px; width: 150px;
            border-radius: 4px;
            }
            #resetX, #resetY, #resetZ{
            border: none;
            color: #FEFCFB;
            background-color: #003366;
            padding-top: 10px;
            padding-bottom: 10px;
            text-align: center;
            display: inline-block;
            font-size: 14px;
            width: 20px;
            border-radius: 4px;
            }
  </style>
        <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
        <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/107/three.min.js"></script>
        <script src="https://cdn.jsdelivr.net/npm/three@0.107.0/examples/js/loaders/GLTFLoader.js"></script>
        </head>
        <body>
        <div class="topnav">
            <h1><i class="far fa-compass"></i> MPU6050 <i class="far fa-compass"></i></h1>
        </div>
        <div class="content">
            <div class="cards">
            <div class="card">
                <p class="card-title">GYROSCOPE</p>
                <p><span class="reading">X: <span id="gyroX"></span> rad</span></p>
                <p><span class="reading">Y: <span id="gyroY"></span> rad</span></p>
                <p><span class="reading">Z: <span id="gyroZ"></span> rad</span></p>
            </div>
            <div class="card">
                <p class="card-title">ACCELEROMETER</p>
                <p><span class="reading">X: <span id="accX"></span> ms<sup>2</sup></span></p>
                <p><span class="reading">Y: <span id="accY"></span> ms<sup>2</sup></span></p>
                <p><span class="reading">Z: <span id="accZ"></span> ms<sup>2</sup></span></p>
            </div>
            <div class="card">
                <p class="card-title">TEMPERATURE</p>
                <p><span class="reading"><span id="temp"></span> &deg;C</span></p>
                <p class="card-title">3D ANIMATION</p>
                <button id="reset" onclick="resetPosition(this)">RESET POSITION</button>
                <button id="resetX" onclick="resetPosition(this)">X</button>
                <button id="resetY" onclick="resetPosition(this)">Y</button>
                <button id="resetZ" onclick="resetPosition(this)">Z</button>
            </div>
            </div>
            <div class="cube-content">
            <div id="3Dcube"></div>
            </div>
        </div>
        <script>
                    /*
            Rui Santos
            Complete project details at https://RandomNerdTutorials.com/esp32-mpu-6050-web-server/

            Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
            The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
            */

            let scene, camera, renderer, rocket3D, rocketGroup;

            function init3D(){
              scene = new THREE.Scene();
              scene.background = new THREE.Color(0xffffff);


              // ADICIONE ESTAS DUAS LINHAS:
              const ambientLight = new THREE.AmbientLight(0xffffff, 1); // Luz branca, intensidade 1
              scene.add(ambientLight);

              camera = new THREE.PerspectiveCamera(
                75,
                parentWidth(document.getElementById("3Dcube")) / parentHeight(document.getElementById("3Dcube")),
                0.1,
                1000
              );

              renderer = new THREE.WebGLRenderer({ antialias: true });
              renderer.setSize(
                parentWidth(document.getElementById("3Dcube")),
                parentHeight(document.getElementById("3Dcube"))
              );

              document.getElementById('3Dcube').appendChild(renderer.domElement);

              // Carregue o modelo 3D do foguete (GLB)
              const loader = new THREE.GLTFLoader();
              loader.load('https://venerable-crostata-1c405a.netlify.app/Montagem_v12.glb', function(gltf) {
                rocket3D = gltf.scene;

                // Crie um grupo para ser o contêiner do modelo
                rocketGroup = new THREE.Group();
                rocketGroup.add(rocket3D);

                // Calcule a caixa delimitadora (bounding box) do modelo
                const box = new THREE.Box3().setFromObject(rocket3D);
                const center = box.getCenter(new THREE.Vector3());

                // Mova o modelo para que seu centro fique alinhado com a origem (0,0,0) do grupo
                rocket3D.position.set(-center.x, -center.y, -center.z);

                // Redimensione o grupo e adicione à cena
                rocketGroup.scale.set(0.4, 0.4, 0.4);
                rocketGroup.rotation.x = -1.5;
                rocketGroup.position.x = 20;
                rocketGroup.position.z = -500;
                scene.add(rocketGroup);

                camera.position.z = 5;
                renderer.render(scene, camera);
              });
            }

            // Resize the 3D object when the browser window changes size
            function onWindowResize(){
            camera.aspect = parentWidth(document.getElementById("3Dcube")) / parentHeight(document.getElementById("3Dcube"));
            //camera.aspect = window.innerWidth /  window.innerHeight;
            camera.updateProjectionMatrix();
            //renderer.setSize(window.innerWidth, window.innerHeight);
            renderer.setSize(parentWidth(document.getElementById("3Dcube")), parentHeight(document.getElementById("3Dcube")));

            }

            window.addEventListener('resize', onWindowResize, false);

            // Create the 3D representation
            init3D();

            // Create events for the sensor readings
            if (!!window.EventSource) {
              var source = new EventSource('/events');

              source.addEventListener('open', function(e) {
                  console.log("Events Connected");
              }, false);

              source.addEventListener('error', function(e) {
                  if (e.target.readyState != EventSource.OPEN) {
                  console.log("Events Disconnected");
                  }
              }, false);

              source.addEventListener('gyro_readings', function(e) {
                  //console.log("gyro_readings", e.data);
                  var obj = JSON.parse(e.data);
                  document.getElementById("gyroX").innerHTML = obj.gyroX;
                  document.getElementById("gyroY").innerHTML = obj.gyroY;
                  document.getElementById("gyroZ").innerHTML = obj.gyroZ;

                  // Change cube rotation after receiving the readinds
                  rocketGroup.rotation.x = obj.gyroY;
                  rocketGroup.rotation.z = obj.gyroX;
                  rocketGroup.rotation.y = obj.gyroZ;
                  renderer.render(scene, camera);
              }, false);

              source.addEventListener('temperature_reading', function(e) {
                  console.log("temperature_reading", e.data);
                  document.getElementById("temp").innerHTML = e.data;
              }, false);

              source.addEventListener('accelerometer_readings', function(e) {
                  console.log("accelerometer_readings", e.data);
                  var obj = JSON.parse(e.data);
                  document.getElementById("accX").innerHTML = obj.accX;
                  document.getElementById("accY").innerHTML = obj.accY;
                  document.getElementById("accZ").innerHTML = obj.accZ;
              }, false);
            }

            function resetPosition(element){
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/"+element.id, true);
            console.log(element.id);
            xhr.send();
            }

            function parentWidth(elem) {
              return elem.parentElement.clientWidth;
            }

            function parentHeight(elem) {
              return elem.parentElement.clientHeight;
            }
        </script>
        </body>
        </html>

)rawliteral";

return html;

} 