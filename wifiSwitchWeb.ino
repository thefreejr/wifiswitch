
/*
 * TimeNTP_ESP8266WiFi.ino
 * Example showing time sync to NTP time source
 *
 * This sketch uses the ESP8266WiFi library
 */
#include <EEPROM.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server ( 80 );


//define section
#define DEBUG //uncomment if you want  debug

const char ssid[] = "Wive-NG-MT";  //  your network SSID (name)
const char pass[] = "1234567891";       // your network password

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

//const int timeZone = 1;     // Central European Time
const int timeZone = 5;     //ufa
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t prevDisplay = 0; // when the digital clock was displayed
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);


int hourON = 19;
int minuteON = 00;
int hourOFF = 23;
int minuteOFF = 00;


const int led = 13; // for http
int relayPin = 4;
bool relayStatus;

void setup()
{
  #ifdef DEBUG
  Serial.begin(9600);
  delay(250);
  Serial.println("TimeNTP Example");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
 //////////////////////////////////WIFI////////////////////////////
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) 
	{
		delay(500);
		#ifdef DEBUG
		Serial.print(".");
		#endif
	}
  #ifdef DEBUG
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  #endif
  Udp.begin(localPort);
  #ifdef DEBUG
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  #endif
  ///////////////////////////////////WEB SREVER////////////////////////
  if ( MDNS.begin ( "esp8266" ) ) 
    {
		#ifdef DEBUG
		Serial.println ( "MDNS responder started" );
		#endif
	}
	server.on ( "/", handleRoot );
	//server.on ( "/test.svg", drawGraph );
	server.on ( "/inline", []() 
	{
		server.send ( 200, "text/plain", "this works as well" );
	} );
	server.onNotFound ( handleNotFound );
	server.begin();
	#ifdef DEBUG
	Serial.println ( "HTTP server started" );
	#endif
	
	//////////////////////////////////TIME////////////////////////////
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  pinMode(relayPin, OUTPUT); 
  pinMode ( led, OUTPUT );
  digitalWrite ( led, 0 );
  
  //////////////////////////////////save uptime///////////////////////
  EEPROM_ulong_write(5,now());
  
  //////////////////////////////////eeprom////////////////////////
  EEPROM.begin(512);
  eepromRead();
}



void loop()
{
  server.handleClient();
  if (timeStatus() != timeNotSet) 
  {
    if (now() != prevDisplay) 
	{ //update the display only if time has changed
		prevDisplay = now();
		#ifdef DEBUG
		digitalClockDisplay();
		#endif
		relayStatus = relayStatusCheck();
		if (relayStatus) 
		{
			digitalWrite (relayPin,LOW);
		}
		if (!relayStatus)
		{
			digitalWrite (relayPin,HIGH);
		}
	}
  }
/////////////////////////////////args check///////////////	
checkWebArgs();
	
}

void checkWebArgs()
{
	

String  stemp;
long temp = 100;

if (!server.hasArg("hourON")) return;
	for (int i=0; i < 4; ++i)
	{
		stemp = server.arg(i);
		if (stemp != "") temp = stemp.toInt();
		else temp = 100;
		if ((i == 0) && (temp <= 24) && (temp >= 0)) hourON = temp;
		if ((i == 1) && (temp <= 60) && (temp >= 0)) minuteON = temp;
		if ((i == 2) && (temp <= 24) && (temp >= 0)) hourOFF = temp;
		if ((i == 3) && (temp <= 60) && (temp >= 0)) minuteOFF = temp;
		
	}
eepromWrite();	
}

void eepromRead()
{
	hourON = EEPROM.read(0);
	minuteON = EEPROM.read(1);
	hourOFF = EEPROM.read(2);
	minuteOFF = EEPROM.read(3);
}

void eepromWrite()
{
	EEPROM.write(0, hourON);
	EEPROM.write(1, minuteON);
	EEPROM.write(2, hourOFF);
	EEPROM.write(3, minuteOFF);
	EEPROM.commit();
}

// чтение
unsigned long EEPROM_ulong_read(int addr) {    
  byte raw[4];
  for(byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr+i);
  unsigned long &num = (unsigned long&)raw;
  return num;
}

// запись
void EEPROM_ulong_write(int addr, unsigned long num) {
  byte raw[4];
  (unsigned long&)raw = num;
  for(byte i = 0; i < 4; i++) EEPROM.write(addr+i, raw[i]);
}


bool relayStatusCheck()
{
	bool s;
	unsigned long todaySecs;
	unsigned long duration;
	long secON;
	long secOFF;
	secON = (hourON * 3600) + (minuteON * 60);
	secOFF = (hourOFF * 3600) + (minuteOFF * 60);
	todaySecs = now();
	todaySecs %= 86400; //узнаем сколько секунд прошло за день

	if (secOFF > secON)
	{
		if ((todaySecs >= secON) && (todaySecs < secOFF)) s = true; // если врем¤ больше включние¤ и меньше выключени¤ то ON
		else s = false;
		
	}
	if (secOFF < secON)
	{
		
		if ((todaySecs >= secOFF) && (todaySecs < secON)) s = false; // врем¤ больше выключени¤ и меньше включени¤ - OFF
		else s = true;
	}
	
	#ifdef DEBUG
	Serial.print(F("now = "));
	Serial.print(todaySecs);
	Serial.print(F("; on = "));
	Serial.print(secON);
	Serial.print(F("; off= "));
	Serial.println(secOFF);
	Serial.print(F("; relayStatus = "));
	Serial.println(s);
	#endif
	return s;
}



void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  #ifdef DEBUG
  Serial.println("Transmit NTP Request");
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  #endif
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
	#ifdef DEBUG
      Serial.println("Receive NTP Response");
    #endif 
	  Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  #ifdef DEBUG
  Serial.println("No NTP Response :-(");
  #endif 
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}




void handleRoot() {
	digitalWrite ( led, 1 );
	char temp[800];
	int sec = now() % 86400;
	int min = sec / 60;
	int hr = min / 60;
	long uptime = (int)(now() - EEPROM_ulong_read(5))/3600;
	

	snprintf ( temp, 800,

"<html>\
  <head>\
	<meta content='width=device-width, initial-scale=1' name='viewport'>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Time: %02d:%02d:%02d // Uptime: %02d hours</p>\
	<p>Relay ON = %02d:%02d</p>\
    <p>Relay OFF = %02d:%02d</p>\
	<p>Relay Status = %02d</p>\
	<form> \
		<p> ON Time: <input type=text name=hourON size=2> : <input type=text name=minuteON size=2>  </p>\
		<p> OFF Time: <input type=text name=hourOFF size=2> : <input type=text name=minuteOFF size=2> </p>\
		<p><input type=submit value=Save></p> \
	</form>\
  </body>\
</html>",

		hr, min % 60, sec % 60, uptime, hourON, minuteON, hourOFF, minuteOFF, relayStatus
	);
	server.send ( 200, "text/html", temp );
	digitalWrite ( led, 0 );
}

void handleNotFound() {
	digitalWrite ( led, 1 );
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for ( uint8_t i = 0; i < server.args(); i++ ) {
		message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
	}

	server.send ( 404, "text/plain", message );
	digitalWrite ( led, 0 );
}


void drawGraph() {
	String out = "";
	char temp[100];
	out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
 	out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
 	out += "<g stroke=\"black\">\n";
 	int y = rand() % 130;
 	for (int x = 10; x < 390; x+= 10) {
 		int y2 = rand() % 130;
 		sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
 		out += temp;
 		y = y2;
 	}
	out += "</g>\n</svg>\n";

	server.send ( 200, "image/svg+xml", out);
}