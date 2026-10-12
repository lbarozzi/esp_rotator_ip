#include <stdint.h>
#include <SPI.h>
#include <ETH.h>
#include <WebServer.h>

//#define DEBUG 


//Board W5500 Ethernet
#define W5500_CS    14
#define W5500_RST    9
#define W5500_INT   10
#define W5500_MISO  12
#define W5500_MOSI  11
#define W5500_SCK   13

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10,10,10,1);
IPAddress gw(10,10,10,254);
IPAddress dns(8,8,8,8);
IPAddress mask(255,255,255,0);

//EthernetClient client;
WebServer srv(80);

//IO_PIN
const int PIN_ADC   =  1;   //GPIO_01
const int PIN_MOTOR_ON = 16;
const int PIN_DIR_LEFT = 17;

typedef struct {
  bool moving;
  bool to_left;
  uint16_t position;
} State;

State cur;

//HTTP METHODS
void handle_root() {
  srv.send(200,"text/html","<h1>Ciao peppa</h1>\n");    //Send OK
}

void handle_analog() {
  update_position();
  char buf[50];
  sprintf(buf,"{'position':%d, 'moving': %d, 'to_left':%d}\n", cur.position,cur.moving, cur.to_left);
  srv.send(200,"text/json",buf);    //Send OK
}

void handle_left(){
  if (cur.moving) {
    if(!cur.to_left){
      cur.moving=0;
      update();
      delay(500);
      cur.to_left=1;
      cur.moving=1;
      update();
    }
  } else {
    cur.to_left=1;
    cur.moving=1;
    update();
  }
  handle_analog();
}

void handle_right(){
  if (cur.moving) {
    if(cur.to_left){
      cur.moving=0;
      update();
      delay(500);
      cur.to_left=0;
      cur.moving=1;
      update();
    }
  } else {
    cur.to_left=0;
    cur.moving=1;
    update();
  }
  handle_analog();
}

void handle_stop(){
  cur.moving=0;
  update();
  handle_analog();
}

int update_position() {
  int tmp = analogRead(PIN_ADC);
  return cur.position= tmp;
}

void update(){
  digitalWrite(PIN_DIR_LEFT, cur.to_left);
  delay(100);
  //Safety check
  if( (cur.position <10 || cur.position >4090) && cur.moving)  cur.moving=0;
  digitalWrite(PIN_MOTOR_ON, cur.moving);
  delay(100);
}

void setup() {
  Serial.begin(115200);  // Start serial communication
  while (!Serial) {
    delay(50); // Wait for the serial port to be ready
  }

  // Initialize SPI with specified pin configuration
  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);

  // Initialize Ethernet using DHCP to obtain an IP address
  if (!ETH.begin(ETH_PHY_W5500, 1, W5500_CS, W5500_INT, W5500_RST, SPI)) {
    Serial.println("Errore inizializzazione W5500");
    return;
  }
  
  Serial.println("Attendo indirizzo IP...");
  while (!ETH.hasIP()) {
    delay(250);
  }
  
  // Print the assigned IP address
  Serial.print("IP Address: ");
  Serial.println(ETH.localIP());

  //IO-SETUP
  pinMode(PIN_ADC, INPUT);
  analogReadResolution(12);  // Valori attesi: 0–4095

  //* I/O
  pinMode(PIN_MOTOR_ON, OUTPUT);
  pinMode(PIN_DIR_LEFT, OUTPUT);
  digitalWrite(PIN_MOTOR_ON, LOW);
  digitalWrite(PIN_DIR_LEFT, LOW);
  //*/
  
  //Configure WebServer
  srv.on("/",HTTP_GET,handle_root);
  srv.on("/api/position",HTTP_GET,handle_analog);
  //* API
  srv.on("/api/position",HTTP_GET,handle_analog);
  srv.on("/api/left", HTTP_GET,handle_left);
  srv.on("/api/right", HTTP_GET,handle_right);
  srv.on("/api/stop", HTTP_GET,handle_stop);
  //  END API */
  srv.begin();
}

void loop() {
  cur.position = analogRead(PIN_ADC);

  //Safty check
  if( (cur.position <10 || cur.position >4090) && cur.moving) {
    cur.moving=0;
    update();
  }

#ifdef DEBUG
  Serial.print("Position: ");
  Serial.print(cur.position );
  Serial.print(cur.moving?" Moving ":" stop ");
  Serial.print(cur.to_left?"left":"right");
  Serial.println(" ");
#endif

  srv.handleClient();
  
  delay(100);  
}
