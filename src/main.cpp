/* Game Controller */
/*INCLUDE LIBRARY*/
#include <mbed.h>
#include <EthernetInterface.h>
#include <rtos.h>
#include <mbed_events.h>
#include <FXOS8700Q.h>
#include <C12832.h>

/* display */
/*DECLARING LCD SCREEN VARIABLES*/
C12832 lcd(D11, D13, D12, D7, D10);

/*DECLARING LED LIGHT VARIABLES*/
DigitalOut red(PTB22);
DigitalOut green(PTE26);
DigitalOut blue(PTB21);

/*DECALRING SPEAKER*/
PwmOut speaker(D6);

/* event queue and thread support */
Thread dispatch;
EventQueue periodic;

/* Accelerometer */
I2C i2c(PTE25, PTE24);
FXOS8700QAccelerometer acc(i2c, FXOS8700CQ_SLAVE_ADDR1);

/* Input from Potentiometer */
AnalogIn  left(A0);

/*FUNCTION FOR BUTTON IFPRESSED*/
enum { Btn1, Btn2 };
DigitalIn buttons[] = {
  DigitalIn(SW2), DigitalIn(SW3)
};

bool ispressed(int b) {
  return !(buttons[b].read());
}

/* User input states */
/*TODO define variables to hold the users desired actions.
    (roll rate and throttle setting)*/
float rollRate;
float throttle = 0.0f;

/* Task for polling sensors */
void user_input(void){
    motion_data_units_t a;
    acc.getAxis(a);

    /*TODO decide on what roll rate -1..+1 to ask for */
    /*TODO decide on what throttle setting 0..100 to ask for */

    float magnitude = sqrt( a.x*a.x + a.x*a.x + a.x*a.x );
    a.x = a.x/magnitude;
    rollRate = asin(a.x);
    if(rollRate < 0.1f && rollRate > -0.1f){
      rollRate = 0.0f;
    }
    if(ispressed(1) == true){
      throttle = 0.0f;
    }
    else if (ispressed(0) == true){
      rollRate = 0.0f;
    }
    else if (left >= 0.0f){
      throttle = left;
    }

}

/* States from Lander */
/*TODO Variables to hold the state of the lander as returned to
    the MBED board, including altitude,fuel,isflying,iscrashed */
float altitude = 0.0, fuel= 100.00;
int velocity, Vx, Vy;
bool isFlying, isCrashed;

/*TODO YOU will have to hardwire the IP address in here */
SocketAddress lander("192.168.80.6",65200);
SocketAddress dash("192.168.80.6",65250);

EthernetInterface eth;
UDPSocket udp;

/* Task for synchronous UDP communications with lander */
void communications(void){
    SocketAddress source;

/*TODO Create and format the message to send to the Lander */
    char text[80];
    char buffer[512];
    sprintf(text,"command:!\nthrottle:%f\nroll:%f\n", throttle * 100.0f, rollRate * -1.0f);
    strcat(buffer,text);

/*TODO Send and recieve messages */
    udp.sendto( lander, buffer, strlen(buffer));
    nsapi_size_or_error_t  n =
     udp.recvfrom(&source, buffer, sizeof(buffer));
    buffer[n] = '\0';

/* Unpack incomming message */
/*TODO split message into lines*/
    char *nextline, *line;
    /*TODO for each line */
    for(
        line = strtok_r(buffer, "\r\n", &nextline);
        line != NULL;
        line = (strtok_r(NULL, "\r\n", &nextline))
        /*TODO split into key value pairs */
        /*TODO convert value strings into state variables */
    ){
      char *key, *value;
      key = strtok(line, ":");
      value = strtok(NULL, ":");
      if(strcmp(key,"altitude")==0 ) {
        altitude = atof(value);
      }
      else if (strcmp(key,"fuel")==0){
        fuel = atof(value);
      }
      else if (strcmp(key, "flying") == 0){
        if (atoi(value) == 1){
          isFlying = true;
        }
        if (atoi(value) == 0){
          isFlying = false;
        }
      }
      else if (strcmp(key,"crashed")==0){
        if (atoi(value) == 1){
          isCrashed = true;
        }
        else if(atoi(value) == 0){
          isCrashed = false;
        }
      }
      else if(strcmp(key,"velocity")==0){
          velocity = atoi(value);
      }
      else if(strcmp(key,"Vx")==0){
          Vx = atoi(value);
      }
      else if(strcmp(key,"Vy")==0){
        Vy = atoi(value);
      }
    }
}

/* Task for asynchronous UDP communications with dashboard */
void dashboard(void){
    /*TODO convert value strings into state variables */
    char buffer[512];
    sprintf(buffer, "altitude:%f\nfuel:%f\nflying:%d\noVx:%d\noVy:%d", altitude, fuel, isFlying,Vx,Vy);
    /*TODO send the message to the dashboard:*/
    udp.sendto( dash, buffer, strlen(buffer));
}

int main() {
    acc.enable();
    throttle = 0.00;
    isFlying = false;
    isCrashed = false;

    /* ethernet connection : usually takes a few seconds */
    printf("conecting \n");
    eth.connect();
    /* write obtained IP address to serial monitor */
    const char *ip = eth.get_ip_address();
    printf("IP address is: %s\n", ip ? ip : "No IP");

    /* open udp for communications on the ethernet */
    udp.open( &eth);

    printf("lander is on %s/%d\n",lander.get_ip_address(),lander.get_port() );
    printf("dash   is on %s/%d\n",dash.get_ip_address(),dash.get_port() );
    dispatch.start( callback(&periodic, &EventQueue::dispatch_forever) );

    /* periodic tasks */
    /*TODO call periodic tasks;
        communications, user_input, dashboard
        at desired rates.
        periodic.call_every(<time in ms>, <function to call>);*/
    periodic.call_every(50, communications);
    periodic.call_every(50, user_input);
    periodic.call_every(50, dashboard);

    /* start event dispatching thread */
    dispatch.start( callback(&periodic, &EventQueue::dispatch_forever) );

    while(1) {
        /* update display at whatever rate is possible */
        /*TODO show user information on the LCD */
        /*TODO set LEDs as appropriate to show boolean states */

        /*ALWAYS PRINT INSTRUCTION*/
        lcd.locate(0,0);
        lcd.printf("Tilt to roll");
        lcd.locate(0,10);
        lcd.printf("Left P for throttle");
        /*SET ALL LEDS TO OFF*/
        red.write(1);
        green.write(1);
        blue.write(1);

        /*WHILE LANDER IS IN FLIGHT*/
        if(isFlying){
          /*THREE IF STATEMENTS CREATE PROXIMITY WARNING USING LED*/
          /*IF ALT ABOVE 350 SHOW GREEN LIGHT ON*/
          if (altitude > 350.00f){
            green.write(0);
            blue.write(1);
            red.write(1);
          }
          /*IF ALT BELOW 350 SHOW BLUE LIGHT ON*/
          if (altitude < 350.00f){
            green.write(1);
            blue.write(0);
            red.write(1);
          }
          /*IF ALT BELOW 250 FLASH RED LIGHT*/
          if (altitude < 250.00f){
            green.write(1);
            blue.write(1);
            wait(0.1);
            red.write(0);
            wait(0.1);
          }

          /*FUEL WARNING SHOWN ON LCD*/
          if (fuel > 90.00){
            lcd.locate(0,20);
            lcd.printf("FULL FUEL");
            //lcd.cls();
          }
          else if (fuel < 90.00){
            lcd.locate(0,20);
            lcd.printf("HIGH FUEL");
            //lcd.cls();
          }
          else if (fuel < 50.00){
            lcd.locate(0,20);
            lcd.printf("HALF FUEL");
            //lcd.cls();
          }
        }


        /*LCD DISPLAY AND SPEAKER SOUND FOR CRASHING LANDER*/
        if(isCrashed){
          lcd.cls();
          lcd.locate(0,10);
          lcd.printf("CRASH LANDING!\n");
          green.write(1);
          blue.write(1);
          red.write(0);
          for (int f=20.0; f<100; f+=10) {
            speaker.period(1.0/f);
            speaker.write(0.5);
            wait(0.1);
          }
          lcd.cls();
        }
        wait(0.1);
        /*TODO you may want to change this time
                    to get a responsive display. TRY 0.1 AGAIN*/
    }
}
