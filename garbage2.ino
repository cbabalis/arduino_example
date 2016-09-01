// Test code for Adafruit GPS modules using MTK3329/MTK3339 driver
//
// This code shows how to listen to the GPS module in an interrupt
// which allows the program to have more 'freedom' - just parse
// when a new NMEA sentence is available! Then access data when
// desired.
//
// Tested and works great with the Adafruit Ultimate GPS module
// using MTK33x9 chipset
//    ------> http://www.adafruit.com/products/746
// Pick one up today at the Adafruit electronics shop 
// and help support open source hardware & software! -ada

#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>

// If you're using a GPS module:
// Connect the GPS Power pin to 5V
// Connect the GPS Ground pin to ground
// If using software serial (sketch example default):
//   Connect the GPS TX (transmit) pin to Digital 3
//   Connect the GPS RX (receive) pin to Digital 2
// If using hardware serial (e.g. Arduino Mega):
//   Connect the GPS TX (transmit) pin to Arduino RX1, RX2 or RX3
//   Connect the GPS RX (receive) pin to matching TX1, TX2 or TX3

// If you're using the Adafruit GPS shield, change 
// SoftwareSerial mySerial(3, 2); -> SoftwareSerial mySerial(8, 7);
// and make sure the switch is set to SoftSerial

// If using software serial, keep these lines enabled
// (you can change the pin numbers to match your wiring):
SoftwareSerial mySerial(3, 2);


Adafruit_GPS GPS(&mySerial);
// If using hardware serial (e.g. Arduino Mega), comment
// out the above six lines and enable this line instead:
//Adafruit_GPS GPS(&Serial1);


// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences. 
#define GPSECHO  true

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy


const int YELLOW_BUTTON = 4;
const int GREEN_BUTTON  = 7; 
const int BLUE_BUTTON   = 8; // the input pin where the
// pushbutton is connected

int yellow_val  = 0;
int green_val   = 0;
int blue_val    = 0;
/* delta-x and delta-y of movement (direction) */
int dir_x       = 0;
int old_dir_x   = 0;
int dir_y       = 0;
int old_dir_y   = 0;
/* previously saved latitude and longtitude respectively */
float old_latitude = 0;
float old_longitude = 0;


void setup()  
{    
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);
  Serial.println("blaxeep basic test!");

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!
  useInterrupt(true);

  delay(1000);
  // Ask for firmware version
  mySerial.println(PMTK_Q_RELEASE);
}


// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

uint32_t timer = millis();
void loop()                     // run over and over again
{
  // in case you are not using the interrupt above, you'll
  // need to 'hand query' the GPS, not suggested :(
  if (! usingInterrupt) {
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    if (GPSECHO)
      if (c) Serial.print(c);
  }
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }
  
  yellow_val = digitalRead(YELLOW_BUTTON); // read input value and store it
  green_val = digitalRead(GREEN_BUTTON);
  blue_val = digitalRead(BLUE_BUTTON);
  
  /* x and y direction computations respectively */
  dir_x = compute_direction( GPS.latitude, old_latitude );
  dir_y = compute_direction( GPS.longitude, old_longitude );
  
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 30000 || yellow_val == HIGH || green_val == HIGH || blue_val == HIGH ||
      has_direction_changed(dir_x, old_dir_x) || has_direction_changed(dir_y, old_dir_y)) { 
    
    timer = millis(); // reset the timer
    print_stuff();

     if (yellow_val == HIGH) {
       delay(500);
       Serial.print("\nCODE 1 \n ");
       print_location();
     }
    if (green_val == HIGH) {
      delay(500);
      Serial.print("\nCODE 2 \n ");
      print_location();
    }
    if (blue_val == HIGH) {
      delay(500);
      Serial.print("\nCODE 3 \n ");
      print_location();
    }
    
    if (dir_x - old_dir_x != 0) {
      Serial.print("\nLatitude direction changed!\n");
      Serial.print("dir x is ");Serial.print(dir_x);
      Serial.print("\nold dir x is ");Serial.print(old_dir_x);
      print_location();
      // assign recently changed direction to the new one
      old_dir_x = dir_x;
    }
  
    if (dir_y - old_dir_y != 0) {
      Serial.print("\nLongitude direction changed!\n");
      print_location();
      old_dir_y = dir_y;
    }
  }
}

void print_location() {
    if (GPS.fix) {  
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4);
      Serial.print(", "); 
      Serial.print(GPS.longitude, 4);
      Serial.print("\n\n");
    } else {
      Serial.print("No fix found yet!\n");
    }
}

float compute_coordinate(String coord) {
  /* drop the last character (it's either (N or S), or (E or W). */
  coord = coord.substring(0, coord.length() - 1);
  /* convert String variable to float. */
  char carray[coord.length() + 1];  // determine size of array
  coord.toCharArray(carray, sizeof(carray));  // put readString into an array
  float float_coord = atof( carray );
  return float_coord;
}
  

int compute_direction(float current_value, float old_value) {
  float dir = 0;
  /* first time running case (where old_value is still 0) */
  if ( old_value == 0 ) {
    /* assign current value to od_value and continue. */
    old_value = current_value;
  }
  /* compute direction by */
  dir = current_value - old_value;
  if ( dir < 0 ) {
    /* if direction has been changed, then assign current direction
     * to old direction, so as to not lose another direction change
     * in the future.
     * Then return -1.
     */
     old_value = current_value;
    return -1;
  }
  else if ( dir >= 0 )
    return 1;
  else
    return 0;
}

int has_direction_changed(int dir, int old_dir) {
  /* if dir * old_dir have the same sign, then nothing has been changed.
   * else, the direction has been changed.
   */
   if ( dir * old_dir == 1 ) {
     return false;
   }
   else {
     old_dir = dir;
     return true;
   }
}

void print_stuff() {
      Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality); 
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", "); 
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
    }
}
