/*
  Zoom Virtual Assistant  
  This program allows you to use the 'Zoom' confrence via the Circuit Playground Express and Blynk App 
  This program must be used with the relative Integromat. The Integromat has two key parts 
  First, When there is no meeting The Integromat check for new valid 'Zoom' meeting in the next 15 minutes, if new meeting was found
  send the data to cpx
  Second, When we finish a certain meeting the data gathered during the meeting is being send to and stores it
  
  NOTE: Before starting to run this code you need to switch few params:
  1. ssid -> to your network name
  2. pass -> to the network's password if there is no password use ""
  3. auth -> to the BLYNK auth' code sent by the app.

  This Program is built as a state machine with the following states: 
  1. init state        - this state is for initing the program 
  2. base state        - the program didn't recieve any data from Integromat
  3. found state       - the program received new meeting data and starts timer
  4. start meet state  - the meeting is starting
  5. In meeting state  - this state support the meeting
  6. end meeting state - this state trigger the end of the meeting and sends data
  
  Inside state 5 there are 2 modes:
  - Semi host mode   - this mode is for user who wishes to share screen 
  - Participant mode - this mode is for regular user
  
  The APP:
  In order for you this code running you need to create 4 sliders and 3 button and connect them in order to the virtual pins mentioned
  - V0 - time until meeting start 
  - V1 - duration of meeting   
  - V2 - the link of the meeting
  - V3 - the subject of meeting
  - V4 - the webhook 
  - V5 - the text entered from Blynk 
  - V6 - the trigger of the message send from Blynk 
  - V8 - The date of the meeting (day of month only) 

  Both integromat scenarios , Blynk project and Video are in the instructable link 
  instructable link : 

  
  Created By :
    Tal Danai   
    Oded Helman 
*/

/* Note: we recommend the 'Zoom' will be full screen , and so is the browser, and the current language to be English
   and set up the initial program in a relative quiet location
*/
#include <Adafruit_CircuitPlayground.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <Wire.h>
#include <SPI.h>
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>

// state machine data //
# define STATE_START 0
# define STATE_NO_NEW_MEETING 10
# define STATE_NEW_MEETING_FOUND 20
# define TRIGGER_NEW_MEETING 30
# define IN_ZOOM_MEETING 40
# define LEAVE_MEETING 50
int state = 0;


// setting up BLYNK + WiFi  //
char ssid[] = "";                                            // enter your WIFI name (SSID) here
char pass[] = "";                                            // enter your WIFI password herechar auth[] = "";  
char auth[] = "";                                            // BLYNK Auth' code -> change it

#define EspSerial Serial1
#define ESP8266_BAUD 9600

ESP8266 wifi(&EspSerial);


// important global variables //

// Sound sensor assist // 
float soundValue, baseSound;

// Init checker //
bool isInit = false;

// Slide Switch //
bool slideSwitch = false;

// Microphone check //
bool leftButton, lastLeftButton = false;
long openMicTime; 

// Raise Hand //
int lightValue; 

// Video check // 
bool rightButton;

// Color data //
int colors[] = {0xFF00000, 0x00FF00, 0x0000FF, 0xFF00FF, 0xCCCC00};                  // microphone -> green + red

// Counter data //
long counter = 0, meetCounter = 0;
long assist = 0;
unsigned int numLeds = 0;
unsigned long sharedScreenTime = 0;
unsigned long microphoneOpen = 0; 
unsigned int numberOfComments = 0; 
bool sendNotification;
 
// Meeting Deteceted assist //
int timePass = 0, lastTempo = -2;
bool isOn = false;

// In meeting params //
bool shareScreen = false, videoOn = false;
float x, y;
unsigned long shareStart; 
bool fullScreen = false;

// Meeting params //
unsigned long starts, ends;
String link, topic, question;
uint8_t value;
int newDay, lastDay;

// Millis var - for timing //
unsigned long currentMillis;

// Change languge assist // 
boolean isEnglish = true;

// Default messages //
char *VIDEO_ON_TO_OFF_MESSAGE = "Sorry, I must grab a coffee";
char *VIDEO_OFF_TO_ON_MESSAGE = "I ran out of coffee. Hey Alexa , add coffee to shopping list";
char *LEAVE_MESSAGE = "Thank you for your time, see you next time around.";
char *SHARE_ON_MESSAGES[] = {"Host, please make sure the participants can share screen by enable them :)", "Please, look at the screen I am about to show to you"};
char *SHARE_OFF_MESSAGES[] = {"Thank you for viewing my screen.", "Lucky me nothing embarrasing was on."};
char *HELLO_MESSAGE = "Hello, I am here for meeting ! How are you today? let get the party started ! ;).";

#if !defined(ARDUINO_SAMD_CIRCUITPLAYGROUND_EXPRESS)
#error "Infrared support is only for the Circuit Playground Express, it doesn't work with other boards."
#endif


BLYNK_WRITE(V0) {
  starts = param.asLong(); // assigning incoming value from pin V0 to a variable
}
BLYNK_WRITE(V1) {
  ends = param.asLong(); // assigning incoming value from pin V1 to a variable
}
BLYNK_WRITE(V2) {
  link = param.asString(); // assigning incoming value from pin V2 to a variable
}
BLYNK_WRITE(V3) {
  topic = param.asString(); // assigning incoming value from pin V3 to a variable
}
// v4 -> data transfers
BLYNK_WRITE(V5) {
  question = param.asString(); // assigning incoming value from pin V5 to a variable
}
BLYNK_WRITE(V6) {
  value  = param.asInt(); // assigning incoming value from pin V6 to a variable
}
BLYNK_WRITE(V8) {
  newDay  = param.asInt(); // assigning incoming value from pin V8 to a variable
}

void setup() {
  //Setup everything
  CircuitPlayground.begin();
  Serial.begin(115200);
  EspSerial.begin(ESP8266_BAUD);
  delay(100);
  Blynk.begin(auth, wifi, ssid, pass);
}

void loop() {
  switch (state) {
    case STATE_START:
      state = handleStart();
      break;
    case STATE_NO_NEW_MEETING:
      state = handleBase();
      break;
     case STATE_NEW_MEETING_FOUND:
      state = newMeetingFound();
     break;
    case TRIGGER_NEW_MEETING:
      state = triggeredMeeting();
      break;
    case IN_ZOOM_MEETING:
      state = inZoom();
      break;
    case LEAVE_MEETING:
      state = leaveMeeting1();
      break;
  }
}
// State machine // 
// State 0 //
int handleStart() {
  init1();
  return STATE_NO_NEW_MEETING;
}
// Base State // 
int handleBase() {
  Blynk.run(); // run BLYNK
  if (link == "" || starts == 0 || ends == 0) {
    sendNotification = newDay!= lastDay; 
    lastDay = newDay; 
    return STATE_NO_NEW_MEETING;
  }
  return STATE_NEW_MEETING_FOUND;
}

// Pre meeting State // 
int newMeetingFound() {
  int temp;
    temp = lightHandler2(colors[2], colors[3]);
    delay(1000);
    counter+= 1000;
    return temp == -1 ? TRIGGER_NEW_MEETING: STATE_NEW_MEETING_FOUND;
}

// Trigger start of the meeting State // 
int triggeredMeeting() {
  flashingLights2Off();
  isOn = false;
  lastTempo = -1;
  flashingLights2QuickOnOff(colors[3]);
  Keyboard.press(KEY_LEFT_GUI);
  myDelay(100);
  Keyboard.press('r');
  Keyboard.releaseAll();
  pressControl('a');
  Keyboard.press(KEY_BACKSPACE);
  myDelay(100);
  Keyboard.releaseAll();
  enterWebsite(link);
  Keyboard.releaseAll();
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  myDelay(1000);
  Keyboard.press(KEY_LEFT_ARROW);
  myDelay(100);
  Keyboard.releaseAll();
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  return IN_ZOOM_MEETING;
}
// Inside the 'Zoom' meeting state // 
int inZoom() {
  // change all to millis()
  if(fullScreen == false){
    pressAlt('f');
    fullScreen = true;
  }
  lastLeftButton = leftButton;
  slideSwitch = CircuitPlayground.slideSwitch();
  soundValue = CircuitPlayground.mic.soundPressureLevel(10);
  leftButton = CircuitPlayground.leftButton();
  rightButton = CircuitPlayground.rightButton();
  lightValue = CircuitPlayground.lightSensor();
  if (slideSwitch) {
    bool temp = switchShareScreen();
    if (temp == true) {
      shareMyScreen();
    }
  } else {
    Keyboard.releaseAll();
    numLeds = map(ends - meetCounter, 0, ends, 0, 11);
    timeLeftInMeeting1(colors[4]);
    raiseHand();
    myDelay(1000);
    
  }
  openCloseMic();
  videoHandler();
  Blynk.run();
  if(question != "" && value == 1){
    enterQuestion();
    value = 0;
    question = "";
  }
  // add more functions here
  meetCounter += assist + 1;
  assist = 0;
  return meetCounter < ends ? IN_ZOOM_MEETING : LEAVE_MEETING;
}

// Finished Meeting, Exiting it and Sending data to SpreadSheet State  State // 
int leaveMeeting1() {
  enterMessage(LEAVE_MESSAGE);
  exitMeeting();
  ends = 0, starts = 0;
  counter = 0, meetCounter = 0;
  link = "";
  flashingLights2Off();
  updateDataToFile();
  return STATE_NO_NEW_MEETING;
}


/** This function is helping in initing the loop
*/
void init1() {
  Serial.println("I'm ready");
  baseSound = CircuitPlayground.mic.soundPressureLevel(10);
  isInit = true;
}

// lights assistance //
/** This function turns lights all the LEDS on within the CPX to the color given
   @param color - the color given
*/
void flashingLights2On(int color) {
  for (int i = 0; i < 10; i++) {
    CircuitPlayground.setPixelColor(i, color);
  }
}

/** This function turns lights all the LEDS off
*/
void flashingLights2Off() {
  for (int i = 0; i < 10; i++) {
    CircuitPlayground.setPixelColor(i, 0x000000);
  }
}

/** This function turn the lighting on and off fast 
 * @param col = the color of the light
 */
void flashingLights2QuickOnOff(int col) {
  for (int j = 0; j < 3; j++) {
    flashingLights2On(col);
    myDelay(100);
    flashingLights2Off();
    myDelay(100);
  }
}

// Meeting detected assistance //
/** This function checks how much time left from the start of the meeting then detemine how fast the tempo of lights
    As closer we get toward the meeting the light will be faster until they become constant
   @param count - counter to verify how much time left
   @return
*/
int tempoChecker(long count) {
  long minute = 1000 * 60;
  int tempo;
  if (starts - count > (minute * 10)) {
    tempo = 10 * 1000;
  } else if (starts - count > (minute * 5)) {
    tempo = 5 * 1000;
  } else if (starts - count > (minute * 2)) {
    tempo = 2 * 1000;
  } else if (starts - count > minute) {
    tempo = 1 * 1000;
  } else if (starts - count > 0){
    tempo = 0;
  }else {
    tempo = -1;
  }
  if (tempo != lastTempo) {
    if(tempo == 10000 & lastTempo == 0){
      tempo = -1;
    }
    lastTempo = tempo;
  }
  return tempo;
}

/** This function handles the light display from the moment a meeting is detected up until the meeting started

   @param col1 - color for the lighting before the meeting starts
   @param col2 - color for the lighting singaling the meeting starts now!
*/
int lightHandler2(int col1, int col2) {
  int tempo = tempoChecker(counter);
  if (tempo == -1){
    return -1;
  }
  if (tempo == 0) {
    isOn = true;
    timePass = 0;
    flashingLights2On(col1);
    return 0;
  }
  timePass += 1000;
  if (timePass % tempo == 0) {
    timePass = 0;
    if (isOn) {
      flashingLights2Off();
    } else {
      flashingLights2On(col1);
    }
    isOn = !isOn;
  }
  return tempo;
}

// complex presses on keyboard //
/** This function assist in pressing the shortcuts keyboard functions which uses the 'alt' key
    and another key in keyboard
   @param c - the other key in the key board
*/
void pressAlt(char c) {
  Keyboard.press(KEY_LEFT_ALT);
  myDelay(1000);
  Keyboard.press(c);
  Keyboard.releaseAll();
}

/** This function assist in pressing the shortcuts keyboard functions which uses the 'control' key (AKA 'Ctrl')
    and another key in keyboard
   @param c - the other key in the key board
*/
void pressControl(char c) {
  Keyboard.press(KEY_LEFT_CTRL);
  myDelay(1000);
  Keyboard.press(c);
  Keyboard.releaseAll();
}


// Shift keyboard handling //
/** This function assist in pressing the shortcuts keyboard functions which uses the 'shift' key
    and another key in keyboard
   @param c - the other key in the key board
*/
void pressShift(char c) {
  Keyboard.press(KEY_LEFT_SHIFT);
  myDelay(1000);
  Keyboard.press(c);
  Keyboard.releaseAll();
}

/** This function reads a char and determine whether the char requires to press shift during the typing or not
   @param c -> the char c
   @return true -> if it does require , otherwise reutrns false
*/
bool needShift(char c) {
  if (c >= 'A' && c <= 'Z') {
    return true;
  }
  char specials[] = "!@#$%^&*()+~{}:<>_?";
  bool check = false;
  for (int i = 0; i < sizeof(specials); i++) {
    if (c == specials[i]) {
      check = true;
      break;
    }
  }
  return check;
}

/** This function assist in reading a char being read and traslating into the relative key in the keyboard in order for it
    to be pressed (with the shift)
   @param c -> the char
   @return the translated char
*/
char shiftValue(char c) {
  if (c >= 'A' && c <= 'Z') {
    // switch to lower case
    char temp = (char) (c + 32);
    return temp;
  }
  // case of shift with digit
  char specials[] = ")!@#$%^&*(";
  for (int i = 0; i < sizeof(specials); i++) {
    if (c == specials[i]) {
      char temp = (char) ('0' + i);
      return temp;
    }
  }
  // case of other alternative
  char other[] = "+~{}:<>_?";
  char translate[] = "=`[];,.-/";
  for (int i = 0; i < sizeof(other); i++) {
    if (c == other[i]) {
      return translate[i];
    }
  }
  return '|';
}

// Type messages //
/** This function opens the chat and enters a single message
   @param string - the message we want to enter
*/
void enterMessage(char *string) {
  // start by clicking the screen once to assure the chat will open
  Mouse.press(MOUSE_LEFT);
  myDelay(500);
  Mouse.release(MOUSE_LEFT);
  pressAlt('h');
  for (int i = 0; i < strlen(string); i++) {
    x = CircuitPlayground.motionX();
    // types in wrong language..
    if ((x > 5.0 || x < -5.0)) {
      // delete all we typed
      pressControl('a');
      Keyboard.press(KEY_BACKSPACE);
      myDelay(100);
      Keyboard.releaseAll();
      // change language
      Keyboard.press(KEY_LEFT_SHIFT);
      Keyboard.press(KEY_LEFT_ALT);
      myDelay(100);
      Keyboard.releaseAll();
      if (!isEnglish) {
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_LEFT_ALT);
        myDelay(100);
        Keyboard.releaseAll();
      }
      isEnglish = !isEnglish;
      // reset;
      i = 0;
      myDelay(100);
    }
    //    if not ignored will probably cause more harm than good
    if (string[i] == '\\') {
      continue;
    }
    bool handledRelease = false;
    // handle all cases
    if (string[i] == 0x20) {
      // space case
      Keyboard.press(0x20);
    } else if (string[i] == '\n') {
      // if enter regularly it will send message .. alt+enter = \n
      pressAlt(KEY_RETURN);
    } else if (string[i] == '\t') {
      // since tab == space * 5
      for (int j = 0; j < 5; j++) {
        Keyboard.press(0x32);
        myDelay(100);
        Keyboard.release(0x32);
      }
      handledRelease = true;
    } else if (needShift(string[i])) {
      // chars who need shift will not be detected to translate relative to spot
      char c = shiftValue(string[i]);
      pressShift(c);
    } else {
      //valid key in keyboard
      Keyboard.press(string[i]);
    }
    if (!handledRelease) {
      myDelay(100);
      Keyboard.releaseAll();
    }
    //// start by clicking the middle of the screen
  }
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  Keyboard.press(KEY_ESC);
  myDelay(100); 
  Keyboard.releaseAll();
  numberOfComments += 1;
}

/** This function allows to enter multiple messages, in a row
   @param strings : the messages we want to enter
*/
void enterMessages(char **strings) {
  // start by clicking the screen once to assure the chat will open
  Mouse.press(MOUSE_LEFT);
  myDelay(100);
  Mouse.release(MOUSE_LEFT);
  pressAlt('h');
  for (int j = 0; j < sizeof(strings) / 2; j++) {
    enterMessagesHelper(strings[j]);
  }
  Keyboard.press(KEY_ESC);
  myDelay(100);
  Keyboard.releaseAll();
}

/** This function assist in entering multiple messages in a row
 * if the language is not right shake(gently) left and right. 
 */
void enterMessagesHelper(char *string) {
  for (int i = 0; i < strlen(string); i++) {
    x = CircuitPlayground.motionX();
    // types in wrong language..
    if ((x > 5.0 || x < -5.0)) {
      // delete all we typed
      pressControl('a');
      Keyboard.press(KEY_BACKSPACE);
      myDelay(100);
      Keyboard.releaseAll();
      // change language
      Keyboard.press(KEY_LEFT_SHIFT);
      Keyboard.press(KEY_LEFT_ALT);
      myDelay(100);
      Keyboard.releaseAll();
      if (!isEnglish) {
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_LEFT_ALT);
        myDelay(100);
        Keyboard.releaseAll();
      }
      isEnglish = !isEnglish;
      // reset;
      i = 0;
      myDelay(100);
    }
    Serial.println(string[i]);
    //    if not ignored will probably cause more harm than good
    if (string[i] == '\\') {
      continue;
    }
    bool handledRelease = false;
    // handle all cases
    if (string[i] == 0x20) {
      // space case
      Keyboard.press(0x20);
    } else if (string[i] == '\n') {
      // if enter regularly it will send message .. alt+enter = \n
      pressAlt(KEY_RETURN);
    } else if (string[i] == '\t') {
      // since tab == space * 5
      for (int j = 0; j < 5; j++) {
        Keyboard.press(0x32);
        myDelay(100);
        Keyboard.release(0x32);
      }
      handledRelease = true;
    } else if (needShift(string[i])) {
      // chars who need shift will not be detected to translate relative to spot
      char c = shiftValue(string[i]);
      pressShift(c);
    } else {
      //valid key in keyboard
      Keyboard.press(string[i]);
    }
    if (!handledRelease) {
      myDelay(100);
      Keyboard.releaseAll();
    }
  }
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  numberOfComments =+1 ;
}
/** This function and enters a link to zoom meeting 
 *  if the language is not right shake(gently) left and right.     
 *  @param string - the website we want to enter
*/
void enterWebsite(String string) {
  for (int i = 0; i < string.length(); i++) {
    x = CircuitPlayground.motionX();
    // types in wrong language..
    if ((x > 5.0 || x < -5.0)) {
      // delete all we typed
      pressControl('a');
      Keyboard.press(KEY_BACKSPACE);
      myDelay(100);
      Keyboard.releaseAll();
      // change language
      Keyboard.press(KEY_LEFT_SHIFT);
      Keyboard.press(KEY_LEFT_ALT);
      myDelay(100);
      Keyboard.releaseAll();
      if (!isEnglish) {
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_LEFT_ALT);
        myDelay(100);
        Keyboard.releaseAll();
      }
      isEnglish = !isEnglish;
      // reset;
      i = 0;
      myDelay(1000);
    }
    //    if not ignored will probably cause more harm than good
    if (string.charAt(i) == '\\') {
      continue;
    }
    bool handledRelease = false;
    // handle all cases
    if (string.charAt(i) == 0x20) {
      // space case
      Keyboard.press(0x20);
    } else if (string.charAt(i) == '\n') {
      // if enter regularly it will send message .. alt+enter = \n
      pressAlt(KEY_RETURN);
    } else if (string.charAt(i) == '\t') {
      // since tab == space * 5
      for (int j = 0; j < 5; j++) {
        Keyboard.press(0x32);
        myDelay(100);
        Keyboard.release(0x32);
      }
      handledRelease = true;
    } else if (needShift(string.charAt(i))) {
      // chars who need shift will not be detected to translate relative to spot
      char c = shiftValue(string.charAt(i));
      pressShift(c);
    } else {
      //valid key in keyboard
      Keyboard.press(string.charAt(i));
    }
    if (!handledRelease) {
      myDelay(100);
      Keyboard.releaseAll();
    }
  }
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
}

/** This function assists in taking the question submitted from BLYNK
 *  and typing it in Zoom chat 
 *  @param string - the question submitted
 */
void enterQuestionHelper(String string) {
  // start by clicking the screen once to assure the chat will open
  Mouse.press(MOUSE_LEFT);
  myDelay(500);
  Mouse.release(MOUSE_LEFT);
  pressAlt('h');
  for (int i = 0; i < string.length(); i++) {
    x = CircuitPlayground.motionX();
    // types in wrong language..
    if ((x > 5.0 || x < -5.0)) {
      // delete all we typed
      pressControl('a');
      Keyboard.press(KEY_BACKSPACE);
      myDelay(100);
      Keyboard.releaseAll();
      // change language
      Keyboard.press(KEY_LEFT_SHIFT);
      Keyboard.press(KEY_LEFT_ALT);
      myDelay(100);
      Keyboard.releaseAll();
      if (!isEnglish) {
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_LEFT_ALT);
        myDelay(100);
        Keyboard.releaseAll();
      }
      isEnglish = !isEnglish;
      // reset;
      i = 0;
      myDelay(100);
    }
    //    if not ignored will probably cause more harm than good
    if (string.charAt(i) == '\\') {
      continue;
    }
    bool handledRelease = false;
    // handle all cases
    if (string.charAt(i) == 0x20) {
      // space case
      Keyboard.press(0x20);
    } else if (string.charAt(i) == '\n') {
      // if enter regularly it will send message .. alt+enter = \n
      pressAlt(KEY_RETURN);
    } else if (string.charAt(i) == '\t') {
      // since tab == space * 5
      for (int j = 0; j < 5; j++) {
        Keyboard.press(0x32);
        myDelay(100);
        Keyboard.release(0x32);
      }
      handledRelease = true;
    } else if (needShift(string.charAt(i))) {
      // chars who need shift will not be detected to translate relative to spot
      char c = shiftValue(string.charAt(i));
      pressShift(c);
    } else {
      //valid key in keyboard
      Keyboard.press(string.charAt(i));
    }
    if (!handledRelease) {
      myDelay(100);
      Keyboard.releaseAll();
    }
  }
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  Keyboard.press(KEY_ESC);
  myDelay(100); 
  Keyboard.releaseAll();
  numberOfComments += 1;
}

/** This function handles the process of entering the question from BLYNK
 * 
 */
void enterQuestion(){
    String temp = question;
    Blynk.virtualWrite(V5,"Question was recieved");
    myDelay(10);
    enterQuestionHelper(temp);
    Blynk.virtualWrite(V5," ");
    Blynk.virtualWrite(V6,0);
}

// microphone // 
/** This fuction checks if the microphone is open or not and signaling it back to user
*   by flashing quickly the red and green if does open
/** This function allows to the user the control when his , microphone is open
 *  
 */
void openCloseMic() {
  if (soundValue > (baseSound + 11)) {
    flashingLights2QuickOnOff(colors[leftButton]);
  }
  if (leftButton) {
      Keyboard.press(0x20);
      if (leftButton && !lastLeftButton){
        openMicTime = millis();
      }
    } else {
      if (!leftButton && lastLeftButton) {
        microphoneOpen += millis() - openMicTime;
        Keyboard.release(0x20);
        
      }
    }
}
/** This function handles the video screen
 * 
 */
void videoHandler(){
  if (rightButton) {
      if (videoOn) {
        enterMessage(VIDEO_ON_TO_OFF_MESSAGE);
        pressAlt('v');
        videoOn = !videoOn;
      } else {
        videoOn = !videoOn;
        pressAlt('v');
        enterMessage(VIDEO_OFF_TO_ON_MESSAGE);
      }
    }
}
// raise hand //
/** this function checks the light value and if the light is low (relativly dark)
 *  raise hand function will start 
 */
 void raiseHand(){
  if(lightValue <= 10){
      pressAlt('y');
    }
 }
/** This function checks if the CPX is tilted up and down if it does will share or unshare the screen
*  @return true if state need to change or not otherwise false
*/
bool switchShareScreen() {
  int signs[2];
  // to share screen tilt the cpx up and down (or down and up)
  for (int i = 0; i < 2; i++) {
    y = CircuitPlayground.motionY();
    if (-1 < y && y < 1) {
      return false;
    }
    if (y < 0) {
      signs[i] = -1;
    } else {
      signs[i] = 1;
    }
    myDelay(1000);
  }
  // if was tilted correctly the signes will be opposite
  return (signs[0] / signs[1]) == -1;
}
/** This function shares and unshare the screen within the zoom meeting
*
*/
void shareMyScreen() {
  shareScreen = !shareScreen;
  if (shareScreen == true) {
    enterMessages(SHARE_ON_MESSAGES);
    shareStart = millis();
  }
  pressAlt('s');
  myDelay(100);
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
  if (shareScreen == false) {
    sharedScreenTime += millis() - shareStart;
    enterMessages(SHARE_OFF_MESSAGES);
  }
}

/** This function shows the relative time left until the end of the meeting by lighting up leds
    in the color given
   @param col - the color of LED
*/
void timeLeftInMeeting1(int col) {
  for (int i = 0; i < 10; i++) {
    if (i < numLeds) {
      CircuitPlayground.setPixelColor(i, col);
    } else {
      CircuitPlayground.setPixelColor(i, 0x000000);
    }
  }
}

/** This function help us exit the meeting when the time of the meeting is up
*/
void exitMeeting() {
  pressAlt('q');
  Keyboard.press(KEY_RETURN);
  myDelay(100);
  Keyboard.releaseAll();
}

/** This fuction updates the data to Spreadsheet and reset the base data 
 * 
 */
void updateDataToFile(){
  Blynk.virtualWrite(V4,microphoneOpen,sharedScreenTime,numberOfComments,topic,newDay-lastDay);
  microphoneOpen = 0;
  topic = "";
  numberOfComments = 0;
  sharedScreenTime = 0;
  Serial.println("Done");
}

/** this function assist on managing all the times we did a myDelay within our code in a single iteration
    then sums up to a global variable which help us keep track how much time left to the meeting
   @param myDelayTime - the time we delay
*/
void myDelay(unsigned int myDelayTime) {
  currentMillis = millis();
  while (millis() < currentMillis + myDelayTime); 
  assist += myDelayTime;
}
