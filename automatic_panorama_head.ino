#include <EEPROM.h>
#include <SoftwareSerial.h>
SoftwareSerial Bluetooth(A4, A5);


#define TILT_STEP_PIN 2    //2
#define TILT_DIR_PIN 5     //5
#define PAN_STEP_PIN 3     //3
#define PAN_DIR_PIN 6      //6
#define EN_PIN    8
#define TILT_CS_PIN    10    //10
#define PAN_CS_PIN    9     //9

#define SHUTTER_PIN 7
#define DONE_STATUS 4
#define MAX_HORIZONTAL_STEPS 9600
#define MAX_VERTICAL_STEPS 16480

// EEPROM POSZĄTEK

#define E_INIT  1023
#define ACCELERATION 0
#define MAX_SPEED 1
#define VERTICAL 2
#define HORIZONTAL 3
#define EXP_AMOUNT 4
#define DELAY_TIME 5
#define MAX_EXP_TIME 6
//koniec definicji EEPROM



#include <AccelStepper.h>

AccelStepper tilt_stepper = AccelStepper(tilt_stepper.DRIVER, TILT_DIR_PIN, TILT_STEP_PIN);
AccelStepper pan_stepper = AccelStepper(pan_stepper.DRIVER, PAN_DIR_PIN, PAN_STEP_PIN);

#include <TMC2130Stepper.h>
TMC2130Stepper driver = TMC2130Stepper(EN_PIN, TILT_DIR_PIN, TILT_STEP_PIN, TILT_CS_PIN);
TMC2130Stepper driver2 = TMC2130Stepper(EN_PIN, PAN_DIR_PIN, PAN_STEP_PIN, PAN_CS_PIN);


//definicje funkcji
void testPan();
void testTilt();
void update_settings_EEPROM();
void restore_settings_EEPROM();
void shutter();
void photoshoot();
void done();
void stop_pause_check();

unsigned int settings[7];
String sdata = ""; // Initialised to nothing.
byte horizontal_set, vertical_set, cancel_flag;
int tmp_acceleration;
int tmp_maxspeed;



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup () {

  restore_settings_EEPROM();  //wczytanie danych z EEPROM
  Bluetooth.begin(9600);
  Bluetooth.println("HELLO PanoHead here!");

  SPI.begin();
  digitalWrite(TILT_CS_PIN, HIGH);
  driver.begin();             // Initiate pins and registeries
  driver.rms_current(900);    // Set stepper current to 600mA. The command is the same as command TMC2130.setCurrent(600, 0.11, 0.5);
  driver.stealthChop(1);      // Enable extremely quiet stepping
  driver.interpolate(1);
  driver.microsteps(2);
  driver.high_speed_mode(0);

  digitalWrite(TILT_CS_PIN, LOW);
  digitalWrite(PAN_CS_PIN, HIGH);
  driver2.begin();             // Initiate pins and registeries
  driver2.rms_current(900);    // Set stepper current to 600mA. The command is the same as command TMC2130.setCurrent(600, 0.11, 0.5);
  driver2.stealthChop(1);      // Enable extremely quiet stepping
  driver2.interpolate(1);
  driver2.microsteps(8);
  digitalWrite(PAN_CS_PIN, LOW);

  pinMode(EN_PIN, OUTPUT);
  pinMode(SHUTTER_PIN, OUTPUT);
  pinMode(DONE_STATUS, OUTPUT);
  digitalWrite(EN_PIN, LOW);
  digitalWrite(SHUTTER_PIN, LOW);
  digitalWrite(DONE_STATUS, HIGH);


  tmp_maxspeed = settings[MAX_SPEED] * 100;
  tilt_stepper.setMaxSpeed(tmp_maxspeed);
  pan_stepper.setMaxSpeed(tmp_maxspeed);
  tmp_acceleration = settings[ACCELERATION] * 100;
  tilt_stepper.setAcceleration(tmp_acceleration);
  pan_stepper.setAcceleration(tmp_acceleration);
  pan_stepper.setCurrentPosition(0);
  tilt_stepper.setCurrentPosition(MAX_VERTICAL_STEPS / 2);





}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {

  int row_photo[240];
  int column_photo[480];
  
  cancel_flag = 0;
  byte ch;
  String valStr;
  int step_size = 20;


  if (Bluetooth.available()) {
    ch = Bluetooth.read();
    sdata += (char)ch;


    if (ch == '\r') { // Command received and ready.
      sdata.trim();

      // Process command in sdata.
      switch ( sdata.charAt(0) ) {
        case 's':
          Bluetooth.println("Start Process");
          photoshoot();
          done();
          break;

        case '$':
          update_settings_EEPROM();
          Bluetooth.println("settings saved");
          break;

        case '#':
          EEPROM.write(E_INIT, 'X');
          restore_settings_EEPROM();
          Bluetooth.println("Default settings DONE");
          break;

        case '?':
          Bluetooth.println("s - start");
          Bluetooth.println("+,- - adjust vertical axe (topDown)");
          Bluetooth.println("p - position topDown SET");
          Bluetooth.print("h - horizontal photos [num] "); Bluetooth.println(settings[HORIZONTAL]);
          Bluetooth.print("v - vertical photos [num] "); Bluetooth.println(settings[VERTICAL]);
          Bluetooth.print("e - sxposure amount [num] "); Bluetooth.println(settings[EXP_AMOUNT]);
          Bluetooth.print("a - acceleration [num*100]  "); Bluetooth.println(settings[ACCELERATION]);
          Bluetooth.print("q - speed [num*100]  "); Bluetooth.println(settings[MAX_SPEED]);
          Bluetooth.println("$ - save settings");
          Bluetooth.println("# - reset settings to default");
          Bluetooth.print("d - delay befote photo [num*10ms]  ");  Bluetooth.println(settings[DELAY_TIME]);
          Bluetooth.print("t - max exposure time [num*10ms]  ");   Bluetooth.println(settings[MAX_EXP_TIME]);
          break;

        case '+':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            step_size = valStr.toInt();
          }
          tilt_stepper.move(step_size);
          while (tilt_stepper.distanceToGo()) tilt_stepper.run();
          Bluetooth.print("moved");
          Bluetooth.println(step_size);

          break;

        case '-':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            step_size = valStr.toInt();
          }
          tilt_stepper.move(-step_size);
          while (tilt_stepper.distanceToGo()) tilt_stepper.run();
          Bluetooth.print("moved");
          Bluetooth.println(step_size);

          break;;

        case 'p':

          tilt_stepper.setCurrentPosition(0);
          Bluetooth.println("TopDown position set succesfully");
          tilt_stepper.runToNewPosition(MAX_VERTICAL_STEPS / 2);

          break;


        case 'd':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[DELAY_TIME] = valStr.toInt();
          }
          Bluetooth.print("delay befote photo [num*10ms]  ");
          Bluetooth.println(settings[DELAY_TIME]);
          break;

        case 't':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[MAX_EXP_TIME] = valStr.toInt();
          }
          Bluetooth.print("max exposure time [num*10ms]  ");
          Bluetooth.println(settings[MAX_EXP_TIME]);
          break;

        case 'h':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[HORIZONTAL] = valStr.toInt();
          }
          horizontal_set = 1;
          Bluetooth.print("Horizontal ");
          Bluetooth.println(settings[HORIZONTAL]);
          Bluetooth.println("Horizontal motor testing...");
          testPan();
          Bluetooth.println("Horizontal motor testing DONE");
          break;


        case 'v':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[VERTICAL] = valStr.toInt();
          }
          vertical_set = 1;
          Bluetooth.print("Vertical ");
          Bluetooth.println(settings[VERTICAL]);
          Bluetooth.println("Vertical motor testing...");
          testTilt();
          Bluetooth.println("Vertical motor testing DONE");
          break;


        case 'e':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[EXP_AMOUNT] = valStr.toInt();
          }
          Bluetooth.print("Amount of exposures  ");
          Bluetooth.println(settings[EXP_AMOUNT]);
          break;

        case 'a':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[ACCELERATION] = valStr.toInt();
          }
          Bluetooth.print("Acceleration(1-100)  ");
          Bluetooth.println(settings[ACCELERATION]);
          tmp_acceleration = settings[ACCELERATION] * 100;
          tilt_stepper.setAcceleration(tmp_acceleration);
          pan_stepper.setAcceleration(tmp_acceleration);
          break;


        case 'q':
          if (sdata.length() > 1) {
            valStr = sdata.substring(1);
            settings[MAX_SPEED] = valStr.toInt();
          }
          Bluetooth.print("Max spped(1-50)  ");
          Bluetooth.println(settings[MAX_SPEED]);
          tmp_maxspeed = settings[MAX_SPEED] * 100;
          tilt_stepper.setMaxSpeed(tmp_maxspeed);
          pan_stepper.setMaxSpeed(tmp_maxspeed);
          break;


        default: Bluetooth.println(sdata);
      } // switch

      sdata = ""; // Clear the string ready for the next command.
    } // if \r
  }  // available
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testPan() {
  pan_stepper.runToNewPosition(-200);
  pan_stepper.runToNewPosition(+200);
  pan_stepper.runToNewPosition(0);
}
void testTilt() {
  tilt_stepper.runToNewPosition((MAX_VERTICAL_STEPS / 2) - 200);
  tilt_stepper.runToNewPosition((MAX_VERTICAL_STEPS / 2) + 200);
  tilt_stepper.runToNewPosition(MAX_VERTICAL_STEPS / 2);
}

void update_settings_EEPROM() {
  EEPROM.write(ACCELERATION, settings[ACCELERATION]);
  EEPROM.write(MAX_SPEED, settings[MAX_SPEED]);
  EEPROM.write(VERTICAL, settings[VERTICAL]);
  EEPROM.write(HORIZONTAL, settings[HORIZONTAL]);
  EEPROM.write(EXP_AMOUNT, settings[EXP_AMOUNT]);
  EEPROM.write(DELAY_TIME, settings[DELAY_TIME]);
  EEPROM.write(MAX_EXP_TIME, settings[MAX_EXP_TIME]);
  EEPROM.write(E_INIT, 'T');
}


void restore_settings_EEPROM() {
  if (EEPROM.read(E_INIT) == 'T') {              //load EEPROM data
    settings[ACCELERATION] = EEPROM.read(ACCELERATION);
    settings[MAX_SPEED] = EEPROM.read(MAX_SPEED);
    settings[VERTICAL] = EEPROM.read(VERTICAL);
    settings[HORIZONTAL] = EEPROM.read(HORIZONTAL);
    settings[EXP_AMOUNT] = EEPROM.read(EXP_AMOUNT);
    settings[DELAY_TIME] = EEPROM.read(DELAY_TIME);
    settings[MAX_EXP_TIME] = EEPROM.read(MAX_EXP_TIME);
  }
  else {                                      //first inicjalization
    EEPROM.write(ACCELERATION, 5);
    settings[ACCELERATION] = 5;
    EEPROM.write(MAX_SPEED, 5);
    settings[MAX_SPEED] = 5;
    EEPROM.write(VERTICAL, 3);
    settings[VERTICAL] = 3;
    EEPROM.write(HORIZONTAL, 6);
    settings[HORIZONTAL] = 6;
    EEPROM.write(EXP_AMOUNT, 1);
    settings[EXP_AMOUNT] = 1;
    EEPROM.write(DELAY_TIME, 3);
    settings[DELAY_TIME] = 3;
    EEPROM.write(MAX_EXP_TIME, 2);
    settings[MAX_EXP_TIME] = 2;
    EEPROM.write(E_INIT, 'T');
  }
}
void shutter() {
  stop_pause_check();
  for (int i = 0; i < settings[EXP_AMOUNT]; i++) {
    delay((settings[DELAY_TIME]) * 10);
    digitalWrite(SHUTTER_PIN, HIGH);
    delay((settings[MAX_EXP_TIME]) * 10);
    digitalWrite(SHUTTER_PIN, LOW);
  }
}


void done() {
  for (int i = 0; i < 3; i++) {
    delay(600);
    digitalWrite(DONE_STATUS, LOW);
    delay(600);
    digitalWrite(DONE_STATUS, HIGH);
  }
}


void photoshoot() {
  Bluetooth.println("type 1 to PAUSE or 2 to CANCEL");
  unsigned long increment_row = (MAX_VERTICAL_STEPS * 100000) / (settings[VERTICAL] + 1);
  unsigned long increment_column = (MAX_HORIZONTAL_STEPS * 100000) / settings[HORIZONTAL];
  int total_photo = settings[HORIZONTAL] * (settings[VERTICAL]);
  int progress_photo = 0;



  for (int z = 1; z < (settings[VERTICAL] + 1); z++) {
    int newRowPosition = (z * increment_row)/100000;
    tilt_stepper.runToNewPosition(newRowPosition);
    if (z % 2 == 1) {                                                               //parity check
      for (int k = 0; k < settings[HORIZONTAL]; k++) {                         //horizontal if no parity
        if (cancel_flag == 1) break;
        int newColumnPosition = (k * increment_column)/100000;
        pan_stepper.runToNewPosition(newColumnPosition);
        shutter();
        progress_photo++;
        Bluetooth.print(progress_photo);
        Bluetooth.print(" of ");
        Bluetooth.println(total_photo);
      }
    }
    else {
      for (int x = settings[HORIZONTAL] - 1; x > -1; x--) {                       //horizontal if parity
        if (cancel_flag == 1) break;
        int newColumnPosition = (x * increment_column)/100000;
        pan_stepper.runToNewPosition(newColumnPosition);
//        Bluetooth.println(newColumnPosition);
        shutter();
        progress_photo++;
        Bluetooth.print(progress_photo);
        Bluetooth.print(" of ");
        Bluetooth.println(total_photo);

      }
    }

  }
  tilt_stepper.runToNewPosition(MAX_VERTICAL_STEPS / 2);
  pan_stepper.runToNewPosition(0);
}



void stop_pause_check() {
  byte ch;
  String valStr;
  if (Bluetooth.available() > 0) {
    int pause = Bluetooth.read();


    if (pause == '1') {
      Bluetooth.println("press 0 to continue");
      while (pause) {
        pause = Bluetooth.read();
        if (pause == '0')break;
      }
    }
    else if (pause == '2') {
      cancel_flag = 1;
    }
  }
}
