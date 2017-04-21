#include <Wire.h>
//#include <Math.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <SimpleDHT.h>

//Pin setup
const int DHT_PIN = 2;
const int NEO_PIN = 6;
const int BUZZER_PIN = 9;
const int BTN_PIN = 4; //2.2kohm pullup

//Pushbutton
bool btn_down_short = false;
bool btn_down_long = false;
bool btn_ignore = false;
bool btn_was_down = false;
unsigned long btn_down_started = 0;

//Temp and Humidity Sensor
SimpleDHT11 dht11;
byte temperature = 0;
byte humidity = 0;

//Neopixel Ring (12 LEDs)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(12, 6, NEO_GRB + NEO_KHZ800);

//RTC (Real Time Clock Breakout)
RTC_DS3231 ds3231;

//Keep track of milliseconds between seconds
int last_sec_five = 0;
unsigned long last_sec_five_millis = 0;
int last_loop_millis = 0;

//Keep track of show date
int show_date_speed = 4000; //in milliseconds
unsigned long show_date_last_millis = 0;

//Needed to keep track of datetime editing
unsigned long edit_date_blink_millis = 0;
int edit_time_hour = 0;
int edit_time_min = 0;
int edit_weekday = 0;

//RTC time
int rtc[7]; //sec,min,hour,dow,day,month,year
unsigned long rtc_last_refreshed = 0;

//The state to keep track of navigation
String state = "";

//The default brightness level of the LEDs
int brightness = 255; //1 - 255
int edit_brightness_state = 1;

void setup()
{
    Serial.begin(9600);

    delay(3000);

    state = "setup_pins";
    setupPins();
    Serial.println("Pin setup done");
    
    state = "rtc_setup";
    setupRtc();
    Serial.println("RTC setup done.");

    setupNeo();
    Serial.print("LED brightness defaulted to: ");
    Serial.println(brightness);
    
    state = "led_test";
    ledTest();
    Serial.println("LED test done.");
    
    state = "show_time";
}

void loop()
{
    unsigned long current_millis = millis();
    
    checkButtonState();
    
    //Refresh RTC time buffer every second
    if(millis() - rtc_last_refreshed > 1000 || rtc_last_refreshed > millis())
    {
        rtc_last_refreshed = millis();
        getDateTime();
        getDhtData();
    }
    
    //Keep track of milliseconds between each multiples of 5 seconds
    if(rtc[0]/5 != last_sec_five)
    {
        last_sec_five = rtc[0]/5;
        last_sec_five_millis = millis();
    }
    
    if(state == "edit_time")
    {
        state = "edit_hour";
    }
    else if(state == "edit_hour")
    {
        if(btn_down_long)
        {
            state = "edit_minute";
        }
        else
        {
            editHour();
        }
    }
    else if(state == "edit_minute")
    {
        if(btn_down_long)
        {
            //User just finished editing the time, set the entered time in RTC
            setDateTime(0, edit_time_min, edit_time_hour, rtc[3], rtc[4], rtc[5], rtc[6]);
            state = "show_time";
        }
        else
        {
            editMinute();
        }
    }
    else if(state == "edit_dow")
    {
        if(btn_down_long)
        {
            setDateTime(0, rtc[1], rtc[2], edit_weekday, rtc[4], rtc[5], rtc[6]);
            state = "show_dow";
        }
        else
        {
            editDow();
        }
    }
    else if(state == "show_dow")
    {
        if(btn_down_short)
        {
            state = "show_temperature";
        }
        else if(btn_down_long)
        {
            state = "edit_dow";
        }
        else
        {
            showDow(rtc[3]);
        }
    }
    else if(state == "show_temperature")
    {
        if(btn_down_short)
        {
            state = "show_humidity";
        }
        else if(btn_down_long)
        {
            
        }
        else
        {
            showTemperature();
        }
    }
    else if(state == "show_humidity")
    {
        if(btn_down_short)
        {
            state = "show_brightness";
        }
        else if(btn_down_long)
        {
            
        }
        else
        {
            showHumidity();
        }
    }
    else if(state == "edit_brightness")
    {
        if(btn_down_long)
        {
            state = "show_brightness";
        }
        else
        {
            editBrightness();
        }
    }
    else if(state == "show_brightness")
    {
        if(btn_down_short)
        {
            state = "show_time";
        }
        if(btn_down_long)
        {
            state = "edit_brightness";
        }
        else
        {
            showBrightness();
        }
    }
    else
    {
        if(btn_down_short)
        {
            state = "show_dow";
        }
        else if(btn_down_long)
        {
            state = "edit_time";
        }
        else
        {
            showTime(rtc[2], rtc[1], rtc[0]);
        }
    }
    
    last_loop_millis = millis() - current_millis;
}

void setupPins()
{
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT);
}

void setupRtc()
{
    if(!ds3231.begin())
    {
        Serial.println("Couldn't find RTC");
        while (1);
    }
    
    if(ds3231.lostPower())
    {
        Serial.println("RTC lost power, lets set the time!");
        ds3231.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}

void setupNeo()
{
    strip.begin();
    strip.show();
    strip.setBrightness(brightness);
}

void ledTest()
{
    for(int p=0; p <= 11; p++)
    {
        clearNeo();
        strip.setPixelColor(p , 255, 0, 0);
        strip.show();
        delay(50);
    }
    
    for(int p=0; p <= 11; p++)
    {
        clearNeo();
        strip.setPixelColor(p , 0, 255, 0);
        strip.show();
        delay(50);
    }
    
    for(int p=0; p <= 11; p++)
    {
        clearNeo();
        strip.setPixelColor(p , 0, 0, 255);
        strip.show();
        delay(50);
    }
}

void getDateTime()
{
    DateTime now = ds3231.now();
    rtc[0] = now.second();
    rtc[1] = now.minute();
    rtc[2] = now.hour();
    rtc[3] = now.dayOfTheWeek();
    rtc[4] = now.day();
    rtc[5] = now.month();
    rtc[6] = now.year();
}

void setDateTime(int sec, int min, int hour, int dow, int day, int month, int year)
{
    ds3231.adjust(DateTime(year, month, day, hour, min, sec));
    
    getDateTime();
    
    edit_time_hour = 0;
    edit_time_min = 0;
    edit_weekday = 0;
}

void showTime(int show_hour, int show_minute, int show_second)
{
    int five_millis = millis() - last_sec_five_millis;
    five_millis = five_millis <= 5000 ? five_millis : 5000;
    int second_ratio = map(five_millis, 0, 5000, 0, 255);
    int second = (show_second/5)+1;
    
    int minute_ratio = map(((show_minute % 5)*10), 0, 50, 0, 255);
    int minute = (show_minute/5);
    
    int hour_ratio = map(show_minute, 0, 60, 0, 255);
    int hour = (show_hour > 12 ? show_hour-12 : show_hour);
    
    for(int x=0; x < 13; x++)
    {
        int r = 0;
        int g = 0;
        int b = 0;
        
        if(second-1 == x)
        {
            g = 255-second_ratio;
        }
        if(second == x || (second == 12 && x == 0))
        {
            g = second_ratio;
        }
        if(minute-1 == x)
        {
            b = 255-minute_ratio;
        }
        if(minute == x)
        {
            b = minute_ratio;
        }
        if(hour-1 == x)
        {
            r = 255-hour_ratio;
        }
        if(hour == x)
        {
            r = hour_ratio;
        }
        
        strip.setPixelColor(x, r, g, b);
    }
    
    strip.show();
    delay(40);
}

void editHour()
{
    if(edit_time_hour == 0)
    {
        edit_time_hour = rtc[2];
        edit_time_hour = (edit_time_hour > 12 ? edit_time_hour-12 : edit_time_hour);
    }
    
    if(btn_down_short)
    {
        edit_time_hour = (edit_time_hour > 11 ? edit_time_hour = 1 : edit_time_hour + 1);
    }
    
    clearNeo();
    strip.setPixelColor(edit_time_hour-1 , 255, 0, 0);
    strip.show();
    delay(40);
}

void editMinute()
{
    if(edit_time_min == 0)
    {
        edit_time_min = rtc[1];
    }
    
    if(btn_down_short)
    {
        edit_time_min = (edit_time_min >= 59 ? edit_time_min = 1 : edit_time_min + 1);
    }
    
    int minute_ratio = map(((edit_time_min % 5)*10), 0, 50, 0, 255);
    int minute = (edit_time_min/5);

    clearNeo();
    strip.setPixelColor(minute-1, 0, 0, 255-minute_ratio);
    strip.setPixelColor(minute , 0, 0, minute_ratio);
    strip.show();
    delay(40);
}

void showDow(int show_dow)
{
    clearNeo();
    strip.setPixelColor(0, 20, 20, 20);
    strip.setPixelColor(1, 20, 20, 20);
    strip.setPixelColor(2, 20, 20, 20);
    strip.setPixelColor(3, 20, 20, 20);
    strip.setPixelColor(4, 20, 20, 20);
    strip.setPixelColor(5, 20, 20, 20);
    strip.setPixelColor(6, 20, 20, 20);
    strip.setPixelColor(show_dow, 255, 255, 255);
    strip.show();
    delay(40);
}

void editDow()
{
    if(edit_weekday == 0)
    {
        edit_weekday = rtc[3];
    }
    
    if(btn_down_short)
    {
        edit_weekday = (edit_weekday >= 7 ? edit_weekday = 1 : edit_weekday + 1);
    }
    
    if(millis() - edit_date_blink_millis > 1000)
    {
        edit_date_blink_millis = millis();
    }
    
    int intensitya = (millis() - edit_date_blink_millis < 500 ? 255 : 100);
    
    clearNeo();
    strip.setPixelColor(0, 20, 20, 20);
    strip.setPixelColor(1, 20, 20, 20);
    strip.setPixelColor(2, 20, 20, 20);
    strip.setPixelColor(3, 20, 20, 20);
    strip.setPixelColor(4, 20, 20, 20);
    strip.setPixelColor(5, 20, 20, 20);
    strip.setPixelColor(6, 20, 20, 20);
    strip.setPixelColor(edit_weekday-1, intensitya, intensitya, intensitya);
    strip.show();
    delay(40);
}

void showBrightness()
{
    clearNeo();
    strip.setPixelColor(0, 255, 0, 0);
    strip.setPixelColor(1, 255, 0, 0);
    strip.setPixelColor(2, 255, 0, 0);
    strip.setPixelColor(3, 255, 0, 0);
    strip.setPixelColor(4, 0, 255, 0);
    strip.setPixelColor(5, 0, 255, 0);
    strip.setPixelColor(6, 0, 255, 0);
    strip.setPixelColor(7, 0, 255, 0);
    strip.setPixelColor(8, 0, 0, 255);
    strip.setPixelColor(9, 0, 0, 255);
    strip.setPixelColor(10, 0, 0, 255);
    strip.setPixelColor(11, 0, 0, 255);
    strip.show();
    delay(100);
}

void editBrightness()
{
    if(btn_down_short)
    {
        brightness = (brightness >= 236 ? brightness = 20 : brightness + 20);
        strip.setBrightness(brightness);
    }
    
    
    edit_brightness_state = (edit_brightness_state >= 12 ? edit_brightness_state = 1 : edit_brightness_state + 1);
    int p = edit_brightness_state;
    
    clearNeo();
    strip.setPixelColor(0, (p == 1 ? 255 : 50), 0, 0);
    strip.setPixelColor(1, (p == 2 ? 255 : 50), 0, 0);
    strip.setPixelColor(2, (p == 3 ? 255 : 50), 0, 0);
    strip.setPixelColor(3, (p == 4 ? 255 : 50), 0, 0);
    strip.setPixelColor(4, 0, (p == 5 ? 255 : 50), 0);
    strip.setPixelColor(5, 0, (p == 6 ? 255 : 50), 0);
    strip.setPixelColor(6, 0, (p == 7 ? 255 : 50), 0);
    strip.setPixelColor(7, 0, (p == 8 ? 255 : 50), 0);
    strip.setPixelColor(8, 0, 0, (p == 9 ? 255 : 50));
    strip.setPixelColor(9, 0, 0, (p == 10 ? 255 : 50));
    strip.setPixelColor(10, 0, 0, (p == 11 ? 255 : 50));
    strip.setPixelColor(11, 0, 0, (p == 12 ? 255 : 50));
    strip.show();
    delay(50);
}

void showTemperature()
{
    Serial.print((int)temperature); Serial.print(" *C, "); 
    
    clearNeo();
    strip.setPixelColor(0, 20, 20, 20);
    strip.setPixelColor(1, 20, 20, 20);
    strip.setPixelColor(2, 20, 20, 20);
    strip.setPixelColor(3, 20, 20, 20);
    strip.setPixelColor(4, 20, 20, 20);
    strip.setPixelColor(5, 20, 20, 20);
    strip.setPixelColor(6, 20, 20, 20);
    strip.setPixelColor(temperature, 255, 255, 255);
    strip.show();
    delay(40);
}

void showHumidity()
{
    Serial.print((int)humidity); Serial.println(" %");
}

int nthDigit(int x, int n)
{
    if(x <= 0 || x >= 100)
    {
        return 0;
    }
    if(x > 0 && x < 10)
    {
        return x;
    }
    if(n == 2)
    {
        return x-floor(x/10)*10;
    }
    if(n == 1)
    {
        return floor(x/10);
    }
}

void clearNeo()
{
    strip.setPixelColor(0, 0, 0, 0);
    strip.setPixelColor(1, 0, 0, 0);
    strip.setPixelColor(2, 0, 0, 0);
    strip.setPixelColor(3, 0, 0, 0);
    strip.setPixelColor(4, 0, 0, 0);
    strip.setPixelColor(5, 0, 0, 0);
    strip.setPixelColor(6, 0, 0, 0);
    strip.setPixelColor(7, 0, 0, 0);
    strip.setPixelColor(8, 0, 0, 0);
    strip.setPixelColor(9, 0, 0, 0);
    strip.setPixelColor(10, 0, 0, 0);
    strip.setPixelColor(11, 0, 0, 0);
}

void checkButtonState()
{
    //Is the button being press right now
    bool btn_is_pressed = digitalRead(BTN_PIN) == LOW ? false : true;

    Serial.println(btn_is_pressed);

    //If the button long press was registered in the last loop, stop it in this loop
    if(btn_down_long)
    {
        btn_down_long = false;
    }
    
    //If a long press was registered in the last loop, and user is still pushing, ignore it
    if(!btn_ignore)
    {
        //If the button is being pressed now and was not being pressed before, start counting press time
        if(btn_is_pressed && !btn_was_down)
        {
            btn_down_started = millis();
            tone(BUZZER_PIN, 2000); delay(200); noTone(BUZZER_PIN);
        }

        //If button is not pressed now and was pressed before, register short press
        if(!btn_is_pressed && btn_was_down)
        {
            btn_down_short = true;
        }
        else
        {
            btn_down_short = false;
        }
      
        //If button is pressed now and has been pressed for more than 2 sec, register long press
        if(btn_is_pressed && btn_was_down && (millis() - btn_down_started) > 2000)
        {
            btn_down_long = true;
            
            //Ignore the button press until the user lets go
            btn_ignore = true;
            tone(BUZZER_PIN, 2000); delay(200); noTone(BUZZER_PIN);
            tone(BUZZER_PIN, 2000); delay(200); noTone(BUZZER_PIN);
        }
        else
        {
            btn_down_long = false;
        }
    }
    
    //If the user last did a long press and now they have let go, do not ignore button press anymore
    if(btn_was_down && !btn_is_pressed)
    {
        btn_ignore = false;
    }
    
    //Let this loops button state be known in the next loop
    btn_was_down = btn_is_pressed;
}

void getDhtData()
{
    dht11.read(DHT_PIN, &temperature, &humidity, NULL);
}
