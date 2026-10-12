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

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Rotore</title><style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#07111e;color:#eef6ff;font-family:Arial,sans-serif}.panel{width:min(94vw,430px);padding:24px;border:1px solid #28425e;border-radius:24px;background:linear-gradient(145deg,#14263a,#0a1524);box-shadow:0 20px 50px #0008;text-align:center}header{display:flex;justify-content:space-between;align-items:center;color:#9db1c8;font-size:12px;letter-spacing:2px}.live{color:#64e6a0}.live:before{content:"";display:inline-block;width:8px;height:8px;margin-right:6px;border-radius:50%;background:currentColor;box-shadow:0 0 10px currentColor}.dial{width:min(78vw,320px);margin:18px auto 8px}.ring{fill:#0a1726;stroke:#365674;stroke-width:3}.mark{fill:#aac5df;font-size:17px;font-weight:bold}.needle{transform-origin:150px 150px;transition:transform .35s ease}.readout{font-size:48px;font-weight:bold;letter-spacing:-2px}.readout small{font-size:15px;letter-spacing:1px;color:#8fa8bf}.raw{margin-top:-4px;color:#8fa8bf;font-size:14px}.status{height:22px;margin:5px;color:#9db1c8;font-size:14px}.offset{display:flex;justify-content:center;align-items:center;gap:8px;margin:12px 0;color:#9db1c8;font-size:14px}.offset input{width:76px;padding:8px;border:1px solid #365674;border-radius:8px;background:#0a1726;color:#eef6ff;font:inherit;text-align:center}.controls{display:grid;grid-template-columns:1fr 1.2fr 1fr;gap:10px;margin-top:16px}button{border:0;border-radius:14px;min-height:58px;color:#ecf7ff;font-size:16px;font-weight:bold;cursor:pointer;background:#1e3955;box-shadow:inset 0 1px #ffffff22,0 5px 12px #0005}button:active{transform:translateY(2px);box-shadow:none}.stop{background:#d84b4b}.offline{color:#ff8585}.offline:before{box-shadow:none}</style></head><body><main class="panel"><header><span>ROTORE</span><span id="connection" class="live">ONLINE</span></header><svg class="dial" viewBox="0 0 300 300" role="img" aria-label="Bussola del rotore"><circle class="ring" cx="150" cy="150" r="132"/><g class="mark" text-anchor="middle"><text x="150" y="42">N</text><text x="263" y="156">E</text><text x="150" y="272">S</text><text x="37" y="156">W</text></g><g stroke="#527594" stroke-width="2"><path d="M150 55v14M245 150h-14M150 245v-14M55 150h14"/></g><g id="needle" class="needle"><path d="M150 61 164 154 150 150 136 154Z" fill="#eef6ff"/><path d="M150 150 164 154 150 239 136 154Z" fill="#607891"/><circle cx="150" cy="150" r="12" fill="#eef6ff"/><circle cx="150" cy="150" r="6" fill="#203d5b"/></g></svg><div class="readout"><span id="position">--</span> <small>°</small></div><div class="raw">Valore grezzo: <span id="raw-position">--</span> / 4095</div><div id="status" class="status">Lettura posizione...</div><label class="offset">Offset <input id="offset" type="number" value="0" step="1" aria-label="Offset in gradi">°</label><div class="controls"><button data-command="left" aria-label="Ruota a sinistra">LEFT</button><button class="stop" data-command="stop">STOP</button><button data-command="right" aria-label="Ruota a destra">RIGHT</button></div></main><script>
const positionEl=document.getElementById('position'),rawPositionEl=document.getElementById('raw-position'),statusEl=document.getElementById('status'),needle=document.getElementById('needle'),connection=document.getElementById('connection'),offsetEl=document.getElementById('offset');let state;
function paint(s){state=s;const p=Math.max(0,Math.min(4095,Number(s.position)||0)),offset=Number(offsetEl.value)||0,degrees=((Math.round(p*359/4095)+offset)%360+360)%360;positionEl.textContent=degrees;rawPositionEl.textContent=p;needle.style.transform=`rotate(${degrees}deg)`;statusEl.textContent=s.moving?`In movimento verso ${s.to_left?'sinistra':'destra'}`:'Fermo';connection.textContent='ONLINE';connection.className='live'}
async function request(path){const r=await fetch(`/api/${path}`,{cache:'no-store'});if(!r.ok)throw Error(r.status);return r.json()}
async function refresh(){try{paint(await request('position'))}catch(e){connection.textContent='OFFLINE';connection.className='live offline';statusEl.textContent='Connessione non disponibile'}}
async function command(direction){try{paint(await request(direction))}catch(e){await refresh()}}
document.querySelectorAll('[data-command]').forEach(button=>button.addEventListener('click',()=>command(button.dataset.command)));
offsetEl.addEventListener('input',()=>{if(state)paint(state)});
refresh();setInterval(refresh,250);
</script></body></html>
)HTML";


bool is_safe() {
  if(cur.moving) {
    if ((cur.to_left && cur.position< 10) || (!cur.to_left && cur.position> 4090) ) return 0;
  }
  return 1;
}

//HTTP METHODS
void handle_root() {
  srv.send_P(200, "text/html", INDEX_HTML);
}

void handle_analog() {
  update_position();
  char buf[50];
  snprintf(buf, sizeof(buf), "{\"position\":%u,\"moving\":%s,\"to_left\":%s}",
           cur.position, cur.moving ? "true" : "false", cur.to_left ? "true" : "false");
  srv.send(200, "application/json", buf);
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
  if(!is_safe() ) cur.moving=0;
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
  if(!is_safe() ) {
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
