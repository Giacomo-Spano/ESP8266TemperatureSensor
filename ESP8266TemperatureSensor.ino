
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <Time.h>
#include <string.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "user_interface.h"

#define Versione "1.02"

uint8_t MAC_array[6];
char MAC_char[18];


unsigned long lastSendTemperature = 0;
const unsigned int sendTemperature_interval = 5000;
unsigned long lastFlash = 0;
const int flash_interval = 20000; // 20 secondi

const int avTempsize = 10;
int avTempCounter = avTempsize;
float avTemp[avTempsize];
float localTemperature = 0;
float localAvTemperature = 0;
float oldLocalAvTemperature = 0;
bool statusChangeSent = false;
unsigned long lastStatusChange = 0;
const int lastStatusChange_interval = 10;

unsigned long lastNotification = 0;
const int Notification_interval = 20;

const char* ssid = "BUBBLES";
//const char* ssid = "Telecom-29545833";
//const char* password = "6oGzjkdMJU2q9XoQLfWiV3nj";

char networkSSID[32];// = "ssid";
char networkPassword[96];// = "password";

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D4
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
WiFiClient client;

//String st;

const int maxposdataChangeSetting = 150;
char databuff[maxposdataChangeSetting];
const int MAX_PAGE_NAME_LEN = 12;
const int MAX_PARAM_LEN = 12;//12;
const int maxposdata = 101; // massimo numero di caratteri della variabile posdata
const int maxvaluesize = maxposdata - 1/*10*/; // massimo numero di digit del valore di una variabile nel POS data

#define P(name) static const prog_uchar name[] PROGMEM // declare a static string
const byte EEPROM_ID = 0x99; // used to identify if valid data in EEPROM
const int ID_ADDR = 0; // the EEPROM address used to store the ID
const int TIMERTIME_ADDR = 1; // the EEPROM address used to store the pin

//byte mac[] = { 0x00, 0xA0, 0xB0, 0xC0, 0xD0, 0x09 };
//byte ip[] = { 192, 168, 1, 90 };
byte id = 0;
int localPort = 80;
const int servernamelen = 30;
char servername[servernamelen];
int serverPort = 8080;
const int boardnamelen = 30;
char boardname[boardnamelen];

// variables created by the build process when compiling the sketch
extern int __bss_end;
extern void *__brkval;

const int temperaturePin = 3;

#define STATUS_IDLE            1

#define			notification_statuschange		1
#define			notification_restarted			2
#define			notification_programstart		3
#define			notification_programend			4

int currentStatus = STATUS_IDLE;
bool netwokStarted = false;
int temperatureSensor = 0;

bool post(char* host, int port, char* path, char* param);
void callServer();
int findIndex(const char* data, const char* target);
void showMain(boolean isPost, char* param);
void showChangeSettings(char* GETparam);
void getPostdata(char *data, int maxposdata);
int parsePostdata(const char* data, const char* param, char* value);
void getStatus(char* GETparam);



void writeEPROM() {

	int addr = TIMERTIME_ADDR;
	EEPROM.write(ID_ADDR, EEPROM_ID); // write the ID to indicate valid data
	byte hiByte;
	byte loByte;

	// id
	EEPROM.write(addr++, id);
	// local port
	hiByte = highByte(localPort);
	loByte = lowByte(localPort);
	EEPROM.write(addr++, hiByte);
	EEPROM.write(addr++, loByte);
	// ssid	
	int res = EEPROM_writeAnything(addr, networkSSID);
	addr += res;
	// password
	res = EEPROM_writeAnything(addr, networkPassword);
	addr += res;
	// server name
	res = EEPROM_writeAnything(addr, servername);
	addr += res;
	// server port
	hiByte = highByte(serverPort);
	loByte = lowByte(serverPort);
	EEPROM.write(addr++, hiByte);
	EEPROM.write(addr++, loByte);
	// board name
	res = EEPROM_writeAnything(addr, boardname);
	addr += res;

	EEPROM.commit();
}

void readEPROM() {

	byte hiByte;
	byte lowByte;

	int addr = TIMERTIME_ADDR;
	// id
	id = EEPROM.read(addr++);
	// local port
	hiByte = EEPROM.read(addr++);
	lowByte = EEPROM.read(addr++);
	localPort = word(hiByte, lowByte);
	// ssid
	int res = EEPROM_readAnything(addr, networkSSID);
	addr += res;
	// password
	res = EEPROM_readAnything(addr, networkPassword);
	addr += res;
	//server name
	res = EEPROM_readAnything(addr, servername);
	addr += res;
	// server port
	hiByte = EEPROM.read(addr++);
	lowByte = EEPROM.read(addr++);
	serverPort = word(hiByte, lowByte);
	// board name
	res = EEPROM_readAnything(addr, boardname);
	addr += res;

}
void initEPROM()
{
	EEPROM.begin(512);

	byte id = EEPROM.read(ID_ADDR); // read the first byte from the EEPROM
	if (id == EEPROM_ID)
	{
		readEPROM();
	}
	else
	{
		writeEPROM();
	}
}

int memoryFree()
{
	int freeValue;

	if ((int)__brkval == 0)
		freeValue = ((int)&freeValue) - ((int)&__bss_end);
	else
		freeValue = ((int)&freeValue) - ((int)__brkval);
	return freeValue;
}

boolean sendStatus(float temperature)
{
	Serial.println(F("sendStatus"));

	char   buffer[150];
	//int len = sprintf(buffer, "{\"id\":%d,\"avtemperature\":%d.%02d,\"temperature\":%d.%02d }\n", (int)id, (int)temperature, (int)(temperature * 100.0) % 100, (int)temperature, (int)(temperature * 100.0) % 100);
	int len = sprintf(buffer, "{\"id\":%d,\"temperature\":%d.%02d,\"avtemperature\":%d.%02d,\"MAC\":\"%s\",\"name\":\"%s\"}", (int)id, (int)localTemperature, (int)(localTemperature * 100.0) % 100, (int)localAvTemperature, (int)(localAvTemperature * 100.0) % 100, MAC_char, boardname);
	
	post(servername, serverPort/*8080*/, "/webduino/sensor", buffer, len);
	return true;
}

int testWifi(void) {
	int c = 0;
	Serial.println("Waiting for Wifi to connect");
	while (c < 20) {
		if (WiFi.status() == WL_CONNECTED) {
			Serial.println("WiFi connected");
			return(20);
		}
		delay(500);
		Serial.print(WiFi.status());
		c++;
	}
	Serial.println("Connect timed out, opening AP");
	return(10);
}

void setupAP(void) {

	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	int n = WiFi.scanNetworks();
	Serial.println("scan done");
	if (n == 0)
		Serial.println("no networks found");
	else
	{
		Serial.print(n);
		Serial.println(" networks found");
		for (int i = 0; i < n; ++i)
		{
			// Print SSID and RSSI for each network found
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
			delay(10);
		}
	}
	Serial.println("");
	String st = "<ul>";
	for (int i = 0; i < n; ++i)
	{
		// Print SSID and RSSI for each network found
		st += "<li>";
		st += i + 1;
		st += ": ";
		st += WiFi.SSID(i);
		st += " (";
		st += WiFi.RSSI(i);
		st += ")";
		st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
		st += "</li>";
	}
	st += "</ul>";
	delay(100);
	WiFi.softAP(ssid);
	Serial.println("softap");
	Serial.println("");
	Serial.println("over");
}

void setup() {
	Serial.begin(115200);
	delay(10);

	Serial.println("");
	Serial.println("");
	Serial.println("");
	Serial.print("starting.... ");

	String str = "Versione ";
	str += Versione;
	Serial.print(str);

	// get MAC Address
	Serial.println("");
	Serial.print("MAC Address ");
	WiFi.macAddress(MAC_array);
	for (int i = 0; i < sizeof(MAC_array); ++i) {
		if (i > 0) sprintf(MAC_char, "%s:", MAC_char);
		sprintf(MAC_char, "%s%02x", MAC_char, MAC_array[i]);
		
	}
	Serial.println(MAC_char);


	initEPROM();

	sensors.begin();

	// Connect to WiFi network
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(networkSSID);
	Serial.println(networkPassword);

	WiFi.begin(networkSSID, networkPassword);
	//WiFi.begin(ssid, password);
	if (testWifi() == 20) {
		// Start the server
		server.begin();
		Serial.println("Server started");
		// Print the IP address
		Serial.println(WiFi.localIP());

		wdt_disable();
		wdt_enable(WDTO_8S);
		return;
	}
	setupAP();
	// Start the server
	server.begin();
	Serial.println("Local erver started...192.168.4.1");
	// Print the IP address
	wdt_disable();
	wdt_enable(WDTO_8S);
}

float readTemperature(){

	// call sensors.requestTemperatures() to issue a global temperature 
	// request to all devices on the bus
	Serial.print("Requesting temperatures...");
	sensors.requestTemperatures(); // Send the command to get temperatures
	Serial.println("DONE");
	// After we got the temperatures, we can print them here.
	// We use the function ByIndex, and as an example get the temperature from the first sensor only.
	Serial.print("Temperature for the device 1 (index 0) is: ");
	float dallasTemperature = sensors.getTempCByIndex(0);
	Serial.println(dallasTemperature);

	if (avTempCounter > avTempsize)
		avTempCounter = 0;
	avTemp[avTempCounter++] = dallasTemperature;

	return dallasTemperature;
}


float getAverageTemperature() {

	float average = 0;

	for (int i = 0; i < avTempsize; i++) {
		average += avTemp[i];
	}
	average = average / (avTempsize);

	return average;
}

void flash() {

	Serial.print("flash.. ");

	localTemperature = readTemperature();
	localAvTemperature = getAverageTemperature();

	Serial.print("localTemperature=");
	Serial.print(localTemperature, DEC);
	Serial.print(" localAvTemperature=");
	Serial.print(localAvTemperature, DEC);
	//Serial.print(" oldLocalAvTemperature=");
	//Serial.println(oldLocalAvTemperature, DEC);
}


void loop() {

	unsigned long currMillis = millis();

	//if ((currMillis - lastSendTemperature) < sendTemperature_interval) {

	//}

	if ((currMillis - lastFlash) > flash_interval) {
		lastFlash = currMillis;
		flash();
		sendStatus(getAverageTemperature());
		return;
	}

	int incomingByte = 0;
	/*if (Serial.available() > 0) {

	while (Serial.available() > 0) {
	// read the incoming byte:
	incomingByte = Serial.read();
	}

	// say what you got:
	Serial.print("I received: ");
	Serial.println(incomingByte, DEC);

	// call sensors.requestTemperatures() to issue a global temperature
	// request to all devices on the bus
	Serial.print("Requesting temperatures...");
	sensors.requestTemperatures(); // Send the command to get temperatures
	Serial.println("DONE");
	// After we got the temperatures, we can print them here.
	// We use the function ByIndex, and as an example get the temperature from the first sensor only.
	Serial.print("Temperature for the device 1 (index 0) is: ");
	double temperature = sensors.getTempCByIndex(0);
	Serial.println(temperature);

	//callServer();
	sendStatus(temperature);
	}*/



	// Check if a client has connected
	//WiFiClient client = server.available();
	char buffer[MAX_PAGE_NAME_LEN + 1]; // additional character for terminating null
	char parambuffer[MAX_PARAM_LEN];



	client = server.available();
	if (client) {

		int type = 0;
		while (client.connected()) {

			if (client.available()) {

				Serial.println("prova");
				// GET, POST, or HEAD
				memset(buffer, 0, sizeof(buffer)); // clear the buffer
				memset(parambuffer, 0, sizeof(parambuffer));
				if (client.readBytesUntil('/', buffer, MAX_PAGE_NAME_LEN)){
					Serial.println(buffer);
					if (strcmp(buffer, "GET ") == 0)
						type = 1;
					else if (strcmp(buffer, "POST ") == 0)
						type = 2;
					// look for the page name
					memset(buffer, 0, sizeof(buffer)); // clear the buffer
					int l;

					if (l = client.readBytesUntil(' ', buffer, MAX_PAGE_NAME_LEN))
					{
						Serial.println(l, DEC);
						Serial.println(buffer);
						l = findIndex(buffer, "?");
						int i = 0;
						if (l != -1) {
							while ((l + i) < MAX_PAGE_NAME_LEN && i < MAX_PARAM_LEN) {
								parambuffer[i] = buffer[l + i];
								i++;
							}
							buffer[l] = '\0';
						}
						else {
							;
						}
						Serial.println(l, DEC);
						Serial.println(buffer);
						Serial.println(parambuffer);
						Serial.println("-");

						if (strcmp(buffer, "main") == 0)
							showMain(type == 2, parambuffer);
						else if (strcmp(buffer, "chstt") == 0)
							showChangeSettings(parambuffer);
						/*else if (strcmp(buffer, "wol") == 0)
						showwol(parambuffer);*/
						else if (strcmp(buffer, "status") == 0)
							getStatus(parambuffer);
						else if (strcmp(buffer, "poststatus") == 0)
							postStatus(parambuffer);
						/*else
						unknownPage(buffer);*/
					}
				}
				break;
			}
		}
		// give the web browser time to receive the data
		delay(20);
		client.stop();
	}


}

const char* host = "www.adafruit.com";



void callServer() {
	Serial.print("connecting to ");
	Serial.println(host);

	// Use WiFiClient class to create TCP connections
	//WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(host, httpPort)) {
		Serial.println("connection failed");
		return;
	}

	// We now create a URI for the request
	String url = "/testwifi/index.html";
	Serial.print("Requesting URL: ");
	Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + host + "\r\n" +
		"Connection: close\r\n\r\n");
	delay(500);

	// Read all the lines of the reply from server and print them to Serial
	while (client.available()){
		String line = client.readStringUntil('\r');
		Serial.print(line);
	}

	Serial.println();
	Serial.println("closing connection");
}

bool post(char* host, int port, char* path, char* param, int len)
{
	// Use WiFiClient class to create TCP connections
	WiFiClient postClient;

	String data;
	Serial.println(F("post notification"));
	Serial.println(host);
	Serial.println(port);
	Serial.println(path);
	Serial.println(param);


	data = "";
	if (client.connect(host, port)) {
		Serial.println("connected");

		data += "POST ";
		data += path;
		data += " HTTP/1.1\r\nHost: ";
		data += host;
		data += "\r\nContent-Type: application/x-www-form-urlencoded\r\n";
		data += "Connection: close\r\n";
		data += "Content-Length: ";
		data += len;
		data += "\r\n\r\n";
		data += param;
		data += "\r\n";

		client.print(data);
		Serial.println(F("post data sent"));
		Serial.println(data);

		delay(1000);

		// Read all the lines of the reply from server and print them to Serial
		while (client.available()){
			Serial.println("client.available");
			String line = client.readStringUntil('\r');
			Serial.print(line);
		}
		Serial.println();
		Serial.println("closing connection");
		//delay(1000);

	}
	else {
		Serial.println(F("-NON CONNESSO-"));
		return false;
	}
	client.stop();
	return true;
}

bool _post(char* host, int port, char* path, char* param)
{
	// Use WiFiClient class to create TCP connections
	WiFiClient client;

	String data;
	data += "";
	data += param;

	Serial.println(F("post notification"));
	Serial.println(host);
	Serial.println(port);
	Serial.println(path);
	Serial.println(param);

	if (client.connect(host, port)) {
		Serial.println("connected");
		client.print("POST ");
		client.print(path);
		client.println(" HTTP/1.1\nHost: ");
		client.println(host);
		client.println("Content-Type: application/x-www-form-urlencoded");
		client.println("Connection: close");
		client.print("Content-Length: ");
		client.println(data.length());
		client.println();
		client.print(data);
		client.println();

		int counter = 0;
		while (client.connected()) {

			if (client.find("result")){
				if (client.find("=")){
					int result = client.parseInt();
					Serial.print(F("result= "));
					Serial.println(result);
					break;
				}
			}
			else {
				Serial.println(F("not found"));
				if (counter++ > 3)
					break;
			}

			delay(500); // check again in 1 seconds 
		}

	}
	else {
		Serial.println(F("-NON CONNESSO-"));
		return false;
	}
	client.stop();
	return true;
}

void showMain(boolean isPost, char* param)
{
	Serial.println(F("showMain "));

	String data;
	data += "";
	data += F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><head><title>Webduino</title><body>\r\n");
	// comandi
	data += F("\n<font color='#53669E' face='Verdana' size='2'><b>Temperature Sensor</b></font>\r\n<table width='80%' border='1'><colgroup bgcolor='#B6C4E9' width='20%' align='left'></colgroup><colgroup bgcolor='#FFFFFF' width='30%' align='left'></colgroup>");
	// temperature
	data += "<tr><td>Temperature: </td><td>" + String(localTemperature) + "</td></tr>";
	data += "<tr><td>AvTemperature: </td><td>" + String(localAvTemperature) + "</td></tr>";

	// sendbutton
	data += F("<tr><td>WOL</td><td><form action='/wol' method='POST'><input type='submit' value='send'></form></td><tr>");
	
	// send temperature update
	data += F("<tr><td>temperature</td><td><form action='/poststatus' method='POST'><input type='submit' value='send'></form></td><tr>");
	
	data += F("</table><font color='#53669E' face='Verdana' size='2'><b>Impostazioni </b></font>\r\n<form action='/chstt' method='POST'><table width='80%' border='1'><colgroup bgcolor='#B6C4E9' width='20%' align='left'></colgroup><colgroup bgcolor='#FFFFFF' width='30%' align='left'></colgroup>");
	// id
	data += F("<tr><td>ID</td><td><input type='num' name='id' value='");
	data += id;
	data += F("' size='2' maxlength='2'> </td></tr>");

	// MAC
	data += F("<tr><td>MAC address</td><td>");
	data += String(MAC_char);
	data += F("</td></tr>");
	
	// local port
	data += F("<tr><td>Local port</td><td><input type='num' name='localport");
	data += F("' value='");
	data += String(localPort);
	data += F("' size='4' maxlength='4'> </td></tr>");
	// board name
	data += F("<tr><td>Board name</td><td><input type='num' name='boardname' value='");
	data += String(boardname);
	data += F("' size='");
	data += String(boardnamelen - 1);
	data += F("' maxlength='");
	data += String(boardnamelen - 1);
	data += F("'> </td></tr>");
	// ssid
	data += F("<tr><td>SSID</td><td><input type='num' name='ssid");
	data += F("' value='");
	data += String(networkSSID);
	data += F("' size='32' maxlength='32'> </td></tr>");
	// password
	data += F("<tr><td>password</td><td><input type='num' name='password");
	data += F("' value='");
	data += String(networkPassword);
	data += F("' size='96' maxlength='96'> </td></tr>");
	// server name
	data += F("<tr><td>Server name</td><td><input type='num' name='servername' value='");
	data += String(servername);
	data += F("' size='");
	data += String(servernamelen - 1);
	data += F("' maxlength='");
	data += String(servernamelen - 1);
	data += F("'> </td></tr>");
	// server port
	data += F("<tr><td>Server port</td><td><input type='num' name='serverport");
	data += F("' value='");
	data += String(serverPort);
	data += F("' size='4' maxlength='4'> </td></tr>");
	// end table
	data += F("</table><input type='submit' value='save'/></form>");
	// versione
	data += "ver." + String(Versione);


	data += F("</body></html>");
	client.println(data);

	delay(100);
	client.stop();

}

void postStatus(char* param)
{
	Serial.println(F("post Status"));

	sendStatus(getAverageTemperature());

	String data;
	data += "";
	data += F("HTTP/1.1 200 OK\r\nContent-Type: text/html\n\n<html><head><meta HTTP-EQUIV='REFRESH' content='0; url=/main'><title>Timer</title></head><body></body></html>");
	client.print(data);
	client.stop();

}


void replaceEncodedSlash(char* source, char* dest) {
	
	char *ptr, *ptrdest;
	ptr = source;
	ptrdest = dest;

	Serial.print("source=");
	Serial.println(source);

	while (ptr != NULL && *ptr != '\0')  {

		if (*ptr == '%') {
			ptr++;
			if (ptr != NULL && *ptr != '\0' && *ptr == '2')  {
				ptr++;
				if (ptr != NULL && *ptr != '\0' && *ptr == 'F')  {

					*ptrdest = '/';
					ptrdest++;
					ptr++;
				}
				else
					ptr -= 2;
			}
			else
				ptr--;
		}

		*ptrdest = *ptr;
		ptrdest++;
		ptr++;
	}
	*ptrdest = '\0';
}

void showChangeSettings(char* GETparam) {

	Serial.println(F("showChangeSettings "));
	getPostdata(databuff, maxposdataChangeSetting);
	char posdata[maxposdata];

	int val;

	// localport
	val = parsePostdata(databuff, "localport", posdata);
	localPort = val;
	Serial.print("localPort=");
	Serial.println(localPort);
	// ssid
	val = parsePostdata(databuff, "ssid", posdata);
	memccpy_P(networkSSID, posdata, '\0', sizeof(networkSSID));
	Serial.print("networkSSID=");
	Serial.println(networkSSID);
	// password
	val = parsePostdata(databuff, "password", posdata);
	memccpy_P(networkPassword, posdata, '\0', sizeof(networkPassword));
	Serial.print("networkPassword=");
	//Serial.println(networkPassword);
	// server name
	val = parsePostdata(databuff, "servername", posdata);
	/*char tempbuffer[servernamelen];
	memccpy_P(tempbuffer, posdata, '\0', servernamelen);
	replaceEncodedSlash(tempbuffer,servername);*/
	memccpy_P(servername, posdata, '\0', servernamelen);
	Serial.print("servername=");
	Serial.println(servername);

	// server port
	val = parsePostdata(databuff, "serverport", posdata);
	serverPort = val;
	Serial.print("serverPort=");
	Serial.println(serverPort);
	// id
	val = parsePostdata(databuff, "id", posdata);
	Serial.print("id ");
	Serial.println(val);
	id = val;
	Serial.print("id=");
	Serial.println(id);
	// board name
	val = parsePostdata(databuff, "boardname", posdata);
	memccpy_P(boardname, posdata, '\0', boardnamelen);
	Serial.print("boardname=");
	Serial.println(boardname);

	String data;
	data += "";
	data += F("HTTP/1.1 200 OK\r\nContent-Type: text/html\n\n<html><head><meta HTTP-EQUIV='REFRESH' content='0; url=/main'><title>Timer</title></head><body></body></html>");
	client.print(data);
	client.stop();

	writeEPROM();
}

int len(const char* data) {

	int l = 0;
	while (data[l] != '\0')
		l++;
	return l;
}

int parsePostdata(const char* data, const char* param, char* value) {


	int pos = findIndex(data, param);

	if (pos != -1) {

		int i = 0;
		char c;
		while (i < len(data) && i < maxvaluesize) {
			if (data[i + pos + len(param) + 1] == '&' || data[i + pos + len(param) + 1] == '\0')
				break;
			value[i] = data[i + pos + len(param) + 1];
			i++;
		}
		//if (i < maxvaluesize) value[i] = '\0';
		/*if (i < maxvaluesize)*/ value[i] = '\0';
		int val = 0;

		for (int k = 0; k < i; k++) {

			val = (val * 10) + value[k] - 48;
		}
		return val;
	}
	return -1;
}

void getPostdata(char *data, int maxposdata) {

	//Serial.print(F("getPostdata"));
	//Serial.print(F("+++->Memory free: "));
	//Serial.println(memoryFree());

	int datalen = 0;

	if (client.findUntil("Content-Length:", "\n\r"))
	{
		datalen = client.parseInt();
	}


	delay(400);
	if (client.findUntil("\n\r", "\n\r"))
	{
		;
	}
	delay(400);
	client.read();

	if (datalen >= maxposdata) {
		Serial.println(F("maxposdat to long"));
	}


	int i = 0;
	while (i < datalen && i < maxposdata) {
		data[i] = client.read();
		//Serial.print(data[i]); // ailitare questa riga per vedere il contenuto della post
		delay(2);
		i++;
	}

	//Serial.println("");
	//Serial.print("datalen ");
	//Serial.print(datalen);
	if (i < maxposdata)
		data[i] = '\0';
}
int findIndex(const char* data, const char* target) {

	boolean found = false;
	int i = 0;
	while (data[i] != '\0') {
		i++;
	}
	i = 0;
	int k = 0;
	while (data[i] != '\0') {

		if (data[i] == target[0]) {
			found = true;
			k = 0;
			while (target[k] != '\0') {

				if (data[i + k] == '\0')
					return -1;
				if (data[i + k] != target[k]) {
					found = false;
					break;
				}
				k++;
			}
			if (found == true)
				return i;
		}
		i++;
	}
	return -1;
}

void getStatus(char* GETparam)
{
	Serial.print(F("getStatus"));
	//Serial.print(F("+++->Memory free: "));
	String data;
	data += "";
	data += F("HTTP/1.0 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nPragma: no-cache\r\n\r\n");

	data += "{\"id\":" + String(id);

	data += ",\"temperature\":" + String(localTemperature);

	data += ",\"avtemperature\":" + String(localAvTemperature);

	data += ",\"name\":\"" + String(boardname) + "\"";

	data += F("}");

	client.print(data);
	delay(10);

	client.stop();
}





