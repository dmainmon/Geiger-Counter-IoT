/* ESP8266 Geiger Counter
   This sketch uses an ESP8266 to read the LED signal from the Mighty OHM Geiger counter. The data is then displayed
   using the serial console or a web browser.

     Viewing the data via web browser by going to the ip address. In this sketch the address is
     http://192.168.1.220


   ///////////////////////////////////////
   Arduino IDE Setup
   File:
      Preferences
        Add the following link to the "Additional Boards Manager URLs" field:
        http://arduino.esp8266.com/stable/package_esp8266com_index.json
   Tools:
      board: NodeMCU 1.0 (ESP-12 Module)
      programmer: USBtinyISP


  ///////////////////////////////
*/
#include <ESP8266WiFi.h>
#include "FS.h" // FOR SPIFFS


// if testing over 1000 cpm turn off debugging or the serial buffer will overload
boolean debugmode = false;

// MaxChartElements is the number of elements to display on the graph. Setting this too high
// will cause the page to take a long time to load and possibly time out

const int MaxChartElements = 60;
const char* ssid = "WiFiSSID";
const char* password = "WiFiPassword";
const int interruptPin = 4; // ~D2 // connected to + LED
const int MAXFILELINES = 1440; // approximately 24 hours
const int MAXFILES = 45; // 45 days of data


int hitCount = 0;
double startMillis;
double seconds;
double minutes;
double CPM = 0.0;
double uSv = 0.0;
double lastcSv = 0.0;
int webRefresh = 60; // how often the webpage refereshes
int WiFiStrength = 0;
int minHolder = 0;
int minuteHits = 0;
int lineCount = 0;
int fileCount = 0;
String fName = "/geiger/gdata";

boolean WiFiConnected = true;


WiFiServer server(80);

//////////////////////////////////////////////////////////////////////////////////////
// this function executes everytime there is hit to the Geiger counter             ///
//////////////////////////////////////////////////////////////////////////////////////
void pinChanged()
{

  detachInterrupt(interruptPin);


  seconds = (millis() - startMillis) / 1000;
  minutes = seconds / 60;
  hitCount++;
  minuteHits++;

  CPM = (hitCount) / minutes;
  uSv = CPM * 0.0057;

  if (debugmode)
  {
    //  Good for debugging
    Serial.print(" Hit Count: "); Serial.println(hitCount);
    Serial.print(" Time seconds: "); Serial.println(seconds);
    Serial.print(" Time minutes: "); Serial.println(minutes);
    Serial.print(" CPM: "); Serial.println(CPM);
    Serial.print(" uSv/hr: "); Serial.println(uSv);

    Serial.println(" ");
  }

  attachInterrupt(interruptPin, pinChanged, RISING);

}

//////////////////////////////////////////////////////////////////
// function used to reset variables: used for data clearing    ///
// session reset                                               ///
//////////////////////////////////////////////////////////////////

void resetVariables()
{

  //reset varialbles
  hitCount = 0;
  startMillis = millis();
  seconds = 0.0;
  minutes = 0.0;
  CPM = 0.0;
  uSv = 0.0;
  minHolder = 0;
  minuteHits = 0;
  lastcSv = 0;
  //
}

//////////////////////////////////
//    main setup function      ///
//////////////////////////////////

void setup() {

  pinMode(interruptPin, INPUT);
  startMillis = millis();
  attachInterrupt(interruptPin, pinChanged, RISING); // start counting Geiger hits

  if (debugmode)
  {
    Serial.begin(115200);
    delay(10);
  }
  SPIFFS.begin();

  ///////////////////

  Dir dir = SPIFFS.openDir("/geiger");
  while (dir.next()) {
    fileCount ++;
    if (debugmode)
    {
      Serial.print(dir.fileName());
      File f = dir.openFile("r");
      Serial.println(f.size());
    }
  }

  //////////////////


  fName += fileCount;
  fName += ".txt";
  File f = SPIFFS.open(fName, "r");
  if (!f) {
    if (debugmode)
      Serial.println("Please wait 30 secs for SPIFFS to be formatted");

    SPIFFS.format();
    if (debugmode)
      Serial.println("Spiffs formatted");

    f = SPIFFS.open(fName, "w");

    if (debugmode)
      Serial.println("Data file created");
  }
  else
  {


    if (debugmode)
      Serial.println("Data file exists");

    while (f.available()) {

      //Lets read line by line from the file
      String str = f.readStringUntil('\n');

      lineCount++;

    }

    f.close();
  }


  // Connect to WiFi network
  if (debugmode)
  {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }

  WiFi.begin(ssid, password);

  // Set the ip address of the webserver
  // WiFi.config(WebServerIP, Gatway, Subnet)
  // or comment out the line below and DHCP will be used to obtain an IP address
  // which will be displayed via the serial console

  WiFi.config(IPAddress(192, 168, 1, 220), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));

  // connect to WiFi router
  int waitCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debugmode)
      Serial.print(".");

    waitCount++;

    if (waitCount > 30)
    {
      WiFiConnected = false;
      break;
    }
  }


  if (WiFiConnected)
  {

    if (debugmode)
    {
      Serial.println("");
      Serial.println("WiFi connected");
    }
    // Start the server
    server.begin();

    if (debugmode)
    {
      Serial.println("Server started");

      // Print the IP address
      Serial.print("Use this URL to connect: ");
      Serial.print("http://");
      Serial.print(WiFi.localIP());
      Serial.println("/");
    }

  } else
  {
    if (debugmode)
      Serial.println("WiFi NOT connected");
  }

}

////////////////////////////////////
//       main loop function      ///
////////////////////////////////////

void loop() {

  /////// create new file every x lines of data

  if (lineCount == MAXFILELINES)
  {
    fileCount++;
    fName = "/geiger/gdata";
    fName += fileCount;
    fName += ".txt";
    lineCount = 0;

  }
  //////
  if (fileCount <= MAXFILES)
  {
    if (minutes > minHolder + 1)
    {

      minHolder++; // every minute

      File f = SPIFFS.open(fName, "a");
      if (!f) {

        if (debugmode)
          Serial.println("file open for append failed");
      }
      else
      {

        lastcSv = minuteHits * 0.0057;
        if (debugmode)
          Serial.println("====== Writing to SPIFFS file =========");

        f.print(",['"); f.print( minHolder); f.print("'"); f.print(",");
        f.print(minuteHits); f.print(",");
        f.print(lastcSv);  f.println("]");

        // the following lines will use the session averages for data
        //  f.print(CPM); f.print(",");
        //  f.print(uSv);  f.println("]");

        lineCount++;

      }
      f.close();
      minuteHits = 0; //reset per minute counter

    }
  }

  ////////////////////////////

  // check to for any web server requests. ie - browser requesting a page from the webserver
  WiFiClient client = server.available();
  if (!client) {
    return;
  }


  // Wait until the client sends some data
  WiFiStrength = WiFi.RSSI();
  if (debugmode)
    Serial.println("new client");

  // Read the first line of the request
  String request = client.readStringUntil('\r');

  if (debugmode)
    Serial.println(request);

  if (request.indexOf("/REFRESH") != -1) {
    webRefresh = 60;
  }

  if (request.indexOf("/RESET") != -1) {  // Reset Session
    resetVariables();
    webRefresh = 0;
  }

  client.flush();

  /////////////////////////////// Return the response

  client.println("HTTP/1.1 200 OK");


  if (request.indexOf("/CLEAR") != -1) {  // Clear file contents
    //    detachInterrupt(interruptPin);
    webRefresh = 0;
    if (debugmode)
      Serial.println("Clearing Data");


    Dir dir = SPIFFS.openDir("/geiger");
    while (dir.next()) {

      SPIFFS.remove(dir.fileName());

    }

    client.println("Content-Type: text/html");
    client.println(""); //  do not forget this one
    client.println("<!DOCTYPE HTML>");

    client.println("<html>");
    client.println(" <head>");

    client.println("</head><body>");
    client.println("<h1>Data Files</h1>");
    client.println("<a href=\"/\">Back</a><br><br>");
    client.println("<br><br><div style\"color:red\">Files Cleared</div>");

    client.println("<br><a href=\"/CLEAR\"\"><button>Clear Data</button></a>");
    client.println(" WARNING! This will delete all data.");

    client.println("</body></html>");

    resetVariables();
    lineCount = 0;
    fileCount = 1;
    fName = "/geiger/gdata1.txt";

    return;

  }



  ////////////////// download file
  if (request.indexOf("/gdata") != -1) {

    String fileDownloadName = request.substring(request.indexOf("/geiger"), request.indexOf("HTTP") - 1) ;
  
    client.println("Content-Type: text/plain");

    client.print("Content-Disposition: attachment;filename=");
    client.println(fileDownloadName);

    client.println("");

    File fi = SPIFFS.open(fileDownloadName, "r");
    if (!fi) {
      client.println("file open for download failed ");
      client.println(fileDownloadName.substring(1));
    }
    else
    {
      if (debugmode)
        Serial.println("====== Reading from SPIFFS file =======");

      while (fi.available()) {

        //Lets read line by line from the file
        String line = fi.readStringUntil('\n');

        client.println(line);
      }

    }
    fi.close();

    return;
  }

  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");

  client.println("<html>");
  client.println(" <head>");

  if (request.indexOf("/DATA") != -1)
  {
    client.println("</head><body>");
    client.println("<h1>Data Files</h1>");
    client.println("<a href=\"/\">Back</a><br><br>");
    if (fileCount >= MAXFILES)
    {
      client.println("<div style=\"color:red\">File limit reached. Clear data to restart data logging.</div>");
    }
    client.println("<a href=\"/CLEAR\"\"><button>Clear Data</button></a>");
    client.println(" WARNING! This will delete all data files.<br><br>");

    Dir dir = SPIFFS.openDir("/geiger");
    while (dir.next()) {

      client.print("<a href=\"");
      client.print(dir.fileName());
      client.print("\">");
      client.print(dir.fileName());
      client.println("</a><br>");
      //  File f = dir.openFile("r");
      //  client.println(f.size());
      //  client.println("<br>");
    }


    client.println("</body></html>");
    return;
  }



  if (webRefresh != 0)
  {
    client.print("<meta http-equiv=\"refresh\" content=\"");
    client.print(webRefresh);
    client.println("\">");
  }

  double CPMholder = CPM;
  double uSvholder = uSv;
  int hitHolder = hitCount;
  double minutesHolder = minutes;

  client.println("<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script> ");
  client.println("   <script type=\"text/javascript\"> ");
  client.println("    google.charts.load('current', {'packages':['corechart','gauge']}); ");
  client.println("    google.charts.setOnLoadCallback(drawChart); ");
  client.println("    google.charts.setOnLoadCallback(drawChartG); ");
  client.println("    google.charts.setOnLoadCallback(drawChartGM); ");
  client.println("    google.charts.setOnLoadCallback(drawChartGH); ");
 
  /////////

  // draw low emission uSv guage
  client.println("   function drawChartG() {");

  client.println("      var data = google.visualization.arrayToDataTable([ ");
  client.println("        ['Label', 'Value'], ");
  client.print("        ['\xB5Sv/hr',  ");
  //
  if (lastcSv != 0)
    client.print(lastcSv); // minute by minute
  else
    client.print(uSvholder);  // shows the average

  client.println(" ], ");
  client.println("       ]); ");
  // setup the google chart options here
  client.println("    var options = {");
  client.println("      width: 250, height: 150,");
  client.println("      min: 0, max: 1,");
  client.println("      greenFrom: 0, greenTo: .25,");
  client.println("      yellowFrom: .25, yellowTo: .75,");
  client.println("      redFrom: .75, redTo: 1,");
  client.println("       minorTicks: 5");
  client.println("    };");
  client.println("   var chart = new google.visualization.Gauge(document.getElementById('chart_div'));");
  client.println("  chart.draw(data, options);");
  client.println("  }");

  //////////////

  // draw medium emission uSv guage
  client.println("   function drawChartGM() {");

  client.println("      var data = google.visualization.arrayToDataTable([ ");
  client.println("        ['Label', 'Value'], ");
  client.print("        ['\xB5Sv/hr',  ");
  //
  if (lastcSv != 0)
    client.print(lastcSv); // minute by minute
  else
    client.print(uSvholder);  // shows the average

  client.println(" ], ");
  client.println("       ]); ");
  // setup the google chart options here
  client.println("    var options = {");
  client.println("      width: 250, height: 150,");
  client.println("      min: 0, max: 10,");
  client.println("      greenFrom: 0, greenTo: 2.5,");
  client.println("      yellowFrom: 2.5, yellowTo: 7.5,");
  client.println("      redFrom: 7.5, redTo: 10,");
  client.println("       minorTicks: 5");
  client.println("    };");
  client.println("   var chart = new google.visualization.Gauge(document.getElementById('chart_divGM'));");
  client.println("  chart.draw(data, options);");
  client.println("  }");

  //////////////

  // draw high emission uSv guage
  client.println("   function drawChartGH() {");

  client.println("      var data = google.visualization.arrayToDataTable([ ");
  client.println("        ['Label', 'Value'], ");
  client.print("        ['\xB5Sv/hr',  ");
  //
  if (lastcSv != 0)
    client.print(lastcSv); // minute by minute
  else
    client.print(uSvholder);  // shows the average

  client.println(" ], ");
  client.println("       ]); ");
  // setup the google chart options here
  client.println("    var options = {");
  client.println("      width: 250, height: 150,");
  client.println("      min: 0, max: 100,");
  client.println("      greenFrom: 0, greenTo: 25,");
  client.println("      yellowFrom: 25, yellowTo: 75,");
  client.println("      redFrom: 75, redTo: 100,");
  client.println("       minorTicks: 5");
  client.println("    };");
  client.println("   var chart = new google.visualization.Gauge(document.getElementById('chart_divGH'));");
  client.println("  chart.draw(data, options);");
  client.println("  }");

 ///////////////////

  double analogValue = analogRead(A0); // read the analog signal

  // convert the analog signal to voltage
  // the ESP2866 A0 reads between 0 and ~3 volts, producing a corresponding value
  // between 0 and 1024. The equation below will convert the value to a voltage value.

  double analogVolts = (analogValue * 3.12) / 1024;

  ///////
  client.println("    function drawChart() { ");

  client.println("     var data = google.visualization.arrayToDataTable([ ");
  client.println("       ['Hit', 'CPM', '\xB5Sv / hr'] ");

  // open file for reading
  int fileSize = 0;

  if (lineCount > 0)
  {
    File fi = SPIFFS.open(fName, "r");
    if (!fi) {
      client.println("<br>file open for read failed");
    }
    else
    {

      if (debugmode)
        Serial.println("====== Reading from SPIFFS file =======");

      fileSize = fi.size();
      int loopCount = 1;
      while (fi.available()) {

        //Lets read line by line from the file
        String line = fi.readStringUntil('\n');

        if (loopCount >= (lineCount - MaxChartElements))
          client.println(line);

        loopCount++;

      }
    }
    fi.close();

  } else // no data lines
  {
    client.print(",['No Data - click Refresh in ");
    client.print(60 - seconds);
    client.println(" seconds',0,0]");
  }

  client.println("     ]); ");
  client.println("     var options = { ");
  client.println("        title: 'Geiger Activity', ");
  client.println("        curveType: 'function', ");

  client.println("  series: {");
  client.println("         0: {targetAxisIndex: 0},");
  client.println("         1: {targetAxisIndex: 1}");
  client.println("       },");

  client.println("  vAxes: { ");
  client.println("         // Adds titles to each axis. ");
  client.println("         0: {title: 'CPM'}, ");
  client.println("         1: {title: '\xB5Sv / hr'} ");
  client.println("       }, ");

  client.println("  hAxes: { ");
  client.println("         // Adds titles to each axis. ");
  client.println("         0: {title: 'time elapsed (minutes)'}, ");
  client.println("         1: {title: ''} ");
  client.println("       }, ");

  client.println("         legend: { position: 'bottom' } ");
  client.println("       }; ");

  client.println("       var chart = new google.visualization.LineChart(document.getElementById('curve_chart')); ");

  client.println("       chart.draw(data, options); ");
  client.println("      } ");
  client.println("    </script> ");



  client.println("  </head>");
  client.println("  <body>");
  client.println("  <h1>Geiger Counter IoT</h1>");
  if (webRefresh != 0) {
    client.print("<table><tr><td>This page will referesh every ");
    client.print(webRefresh);
    client.print(" seconds ");
  }
  else
  {
    client.print("<div><b style=\"color:red;\">REFRESH NOW</b> ");
  }
  client.println("<a href=\"/REFRESH\"\"><button>Refresh</button></a></td>");

  client.print("<td style=\"text-align:center; width:650px;\">");
  if (fileCount >= MAXFILES)
  {
    client.print("<div style=\"color:red\">File limit reached. Data logging stopped.<br>Click Data Files and Clear Data to restart data logging.</div>");
  }

  client.println("<a href=\"/DATA\"\">Data Files</a></td></tr></table>");

  client.println("<table><tr><td style=\"width:190px; vertical-align:top;\">");


  client.print("<b>Session Details</b>");


  client.println("<br>Hit Count: ");
  client.println(hitHolder);
  client.print("<br>Total Minutes: ");
  client.println(minutesHolder);
  client.print("<br>Avg CPM: <b>");
  client.println(CPMholder);
  client.print("</b><br>Avg &micro;Sv/hr: <b>");
  client.println(uSvholder);
  client.print("</b><br>Avg millirems/hr: <b>");
  client.println(uSvholder * .1);
  client.println("</b><br><a href=\"/RESET\"\"><button>Reset Session</button></a>");

  client.println("</td><td>");
  client.println("<div id=\"chart_div\" style=\"width: 250px;\"></div>");
  client.println("</td><td>");
  client.println("<div id=\"chart_divGM\" style=\"width: 250px;\"></div>");
  client.println("</td><td>");
  client.println("<div id=\"chart_divGH\" style=\"width: 250px;\"></div>");
  client.println("</td></tr></table>");

  client.println("<div id=\"curve_chart\" style=\"width: 1000px; height: 500px\"></div>");

  client.println("<br>WiFi Signal Strength: ");
  client.println(WiFiStrength);
  client.print("dBm <br>Geiger Power: "); client.print(analogVolts);  client.println("volts");


  /*
    // some usefull debugging information
    // check remaining data
    FSInfo fs_info;
    SPIFFS.info(fs_info);

    int fileTotalKB = (int)fs_info.totalBytes;
    int fileUsedKB = (int)fs_info.usedBytes;

    client.print("<div>fileTotalBytes: ");

    client.print(fileTotalKB);
    client.print("<br>fileUsedBytes: ");
    client.print(fileUsedKB);


    float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
    client.print("<br>chipSize: ");
    client.print(flashChipSize);
  */

  ///////////////////////////

  client.println("</body>");
  client.println("</html>");


  if (debugmode)
  {
    Serial.println("Client disonnected");
    Serial.println("");
  }


}

