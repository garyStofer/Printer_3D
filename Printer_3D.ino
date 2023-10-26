// Using the WebDAV server with Rigidbot 3D printer.
// Printer controller is a variation of Rambo running Marlin firmware

/* Using the WebDAV server from a PC

  To make the device discoverable via mDNS and a hostname do the following: 
	
	From windows < 10 install apples Bonjour 2.xx  ( minus the apple auto update and the printer wizzard )
	From "My Computer" using "Add a network location" connect to it via \\HOSTNAME.local\davwwwroot (HOSTNAME as per define below)
	From "My Computer" using "Map network drive"      connect to it via \\HOSTNAME.local\davwwwroot  ( Hostname can not include special chars)
	
  Without a mDNS host running on the Windows machine, the device needs to be connected using IP address
	as in :
	using "Add a network location" connect to it via \\192.168.1.123\davwwwroot   (IP address of device to be found on the router)
	using "Map network drive"      connect to it via \\192.168.1.123              ( "" )
	
	 */
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>      // allows discovery of device by PC via mDNS service 
#include <ESPWebDAV.h>

#ifdef DBG_PRINTLN
	#undef DBG_INIT
	#undef DBG_PRINT
	#undef DBG_PRINTLN
	#define DBG_INIT(...)		{ Serial.begin(__VA_ARGS__); }
	#define DBG_PRINT(...) 		{ Serial.print(__VA_ARGS__); }
  #define DBG_PRINTLN(...) 	{ Serial.println(__VA_ARGS__); }
	//#define DBG_INIT(...)		{}
	//#define DBG_PRINT(...) 		{}
	//define DBG_PRINTLN(...) 	{}
#endif

// LED is connected to GPIO2 on this board
#define INIT_LED			{digitalWrite(2, HIGH);pinMode(2, OUTPUT);}
#define LED_ON				{digitalWrite(2, LOW);}
#define LED_OFF				{digitalWrite(2, HIGH);}

// 2nd LED is connected to GPIO16
#define INIT_LED2			{digitalWrite(16, HIGH);pinMode(16, OUTPUT);}
#define LED2_ON				{digitalWrite(16, LOW);}
#define LED2_OFF			{digitalWrite(16, HIGH);}


#define HOSTNAME		"ender3d"
#define SERVER_PORT		80
#define SPI_BLOCKOUT_PERIOD	20000UL

#define SD_CS		4
#define MISO		12
#define MOSI		13
#define SCLK		14
#define CS_SENSE	5


const char *ssid = 		"Dialup";
const char *password = 	"Andromeda";

ESPWebDAV dav;
String statusMessage;
bool initFailed = false;

volatile long spiBlockoutTime = 0;
volatile bool weHaveBus = false;



// ------------------------
void setup() {
// ------------------------
	// ----- GPIO -------
	INIT_LED;
  INIT_LED2;

	pinMode(CS_SENSE, INPUT);
  relenquishBusControl();       // for good measure -- all pins set to inputs, we don't have the bus
 
// Detect when other master uses SD card
	attachInterrupt(CS_SENSE, []() 
  {
		if(!weHaveBus)
			spiBlockoutTime = millis() + SPI_BLOCKOUT_PERIOD;
	}, FALLING);
	
	DBG_INIT(115200);
  DBG_PRINT("\n.\n.\n");
	DBG_PRINTLN(HOSTNAME);
  DBG_PRINT("Trying to connect to "); DBG_PRINTLN(ssid);  
  Serial.flush();
  
  blink();
	

	// ----- WIFI -------
	// Set hostname first
	WiFi.hostname(HOSTNAME);    // This is only for the DHCP -- name can be seen in the router that does the dhcp for example
	
  // Reduce startup surge current
	WiFi.setAutoConnect(false);
	WiFi.mode(WIFI_STA);
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);
	WiFi.begin(ssid, password);
  
	// Wait for connection
	while(WiFi.status() != WL_CONNECTED) 
  {
		blink();
		DBG_PRINT(".");
	}

	DBG_PRINTLN("\n");
	DBG_PRINT("Connected to "); DBG_PRINTLN(ssid);
	DBG_PRINT("IP address: "); DBG_PRINTLN(WiFi.localIP());
	DBG_PRINT("RSSI: "); DBG_PRINTLN(WiFi.RSSI());
	DBG_PRINT("Mode: "); DBG_PRINTLN(WiFi.getPhyMode());

  // This allows a PC to connect via HOSTNAME.local using mDNS (bonjour)
  if (!MDNS.begin(HOSTNAME))        
  {
    DBG_PRINTLN("Error setting up mDNS");
  }
  else
  {
    DBG_PRINT("mDNS started with name:");
    DBG_PRINTLN( HOSTNAME);
  }

	// wait for other master to assert SPI bus first  -- When adapter is inserted and powers up device 
  // allow this time for system where the card is inserted to access it first
  LED2_ON;
  DBG_PRINTLN("Waiting for machine controller to access card first")
	delay(SPI_BLOCKOUT_PERIOD/2);
  LED2_OFF;
 
	// ----- SD Card and Server -------
	// Check to see if other master is using the SD card
	while(millis() < spiBlockoutTime)
		blink2();

	takeBusControl();

	
	// start the SD DAV server
	if(!dav.init(SD_CS, (SPISettings) SPI_FULL_SPEED, SERVER_PORT))		
  {
		statusMessage = "Failed to initialize SD Card";
		DBG_PRINT("ERROR: "); DBG_PRINTLN(statusMessage);
		// indicate error on LED
		errorBlink();
		initFailed = true;
	}
	else
  {
    statusMessage ="SD and Dav OK";
  }

	relenquishBusControl();
	DBG_PRINTLN("WebDAV server is started");

}




void loop() 
{
  // check to see if the printer has the SD card 
	if(millis() < spiBlockoutTime)
		blink2();      // printer is using sd card
 return;
	// do it only if there is a need to read FS
	if(dav.isClientWaiting())	
  {
		if(initFailed)
			return dav.rejectClient(statusMessage);
		
		// has other master been using the bus in last few seconds
		if(millis() < spiBlockoutTime)
			return dav.rejectClient("Marlin is reading from SD card");
		
		// a client is waiting and FS is ready and other SPI master is not using the bus
		takeBusControl();
		dav.handleClient();
		relenquishBusControl();
	}
 
}



// ------------------------
void takeBusControl()	{
// ------------------------
	weHaveBus = true;
	LED_ON;
	pinMode(MISO, SPECIAL);	  // SPECIAL means that the GPIO defaults to the special buses . i.e. SPI mode
	pinMode(MOSI, SPECIAL);	
	pinMode(SCLK, SPECIAL);	
	pinMode(SD_CS, OUTPUT);
}



// ------------------------
void relenquishBusControl()	{
// ------------------------
	pinMode(MISO, INPUT);	
	pinMode(MOSI, INPUT);	
	pinMode(SCLK, INPUT);	
	pinMode(SD_CS, INPUT);
	LED_OFF;
	weHaveBus = false;
}




// ------------------------
void blink()	{
// ------------------------
	LED_ON; 
	delay(100); 
	LED_OFF; 
	delay(400);
}
void blink2()	{
// ------------------------
	LED2_ON; 
	delay(100); 
	LED2_OFF; 
	delay(400);
}

// ------------------------
void errorBlink()	{
// ------------------------
	for(int i = 0; i < 100; i++)	{
		LED_ON; 
		delay(50); 
		LED_OFF; 
		delay(50);
    LED_ON; 
		delay(50); 
		LED_OFF; 
		delay(50);
	}
}



