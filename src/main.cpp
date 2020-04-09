#include <Arduino.h>
//#include <heltec.h>
#include "F007TH.h"
#include "send2GS.h"
#include <U8x8lib.h>

#include "zeit.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "esp32-hal-cpu.h"

//#include "RFM69mbus.h"
#include "sx1276mbus.h"
SX1276MBUS sx1276mbus;

#define RxPin 38 //ESP32 input only
//#define RxPin 19

#define PinNSS 18 //for sx1276 on Heltec v2
//#define PinNSS 2
#define PinDIO0 26 //for sx1276 on Heltec v2

/* heltec esp32 lora v2
// GPIO5  -- SX1278's SCK
// GPIO19 -- SX1278's MISO
// GPIO27 -- SX1278's MOSI
// GPIO18 -- SX1278's CS
// GPIO14 -- SX1278's RESET
// so we use for the RFM69 same MISO MOSI SCK but different CS PinNSS
*/
//instance RFM69
//RFM69 rfm69;

int8_t PAind = 13;

U8X8_SSD1306_128X64_NONAME_HW_I2C OLED(/*OLED_RST*/ 16, /*OLED_SCL*/ 15, /*OLED_SDA*/ 4);
//U8X8_SSD1306_128X64_NONAME_SW_I2C OLED(/* clock=*/15, /* data=*/4, /* reset=*/16);
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2 (U8G2_R0, 16, 15, 4)

// 1.3" OLED
//U8X8_SH1106_128X64_NONAME_HW_I2C OLED(/* reset=*/U8X8_PIN_NONE);
char displaystr[20];

#include "msgdecoder.h"

unsigned long nextsend;
const unsigned long period = 300000;

unsigned int nextdraw = 0;

const char *NODEID = "F007TH";

char sendstr[100];

void setup()
{
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  //setCpuFrequencyMhz(80);

  //Heltec.begin(false /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);

  //Heltec.display->display();

  //esp_log_level_set("wifi", ESP_LOG_INFO);
  Serial.begin(115200);
  Serial.println(getCpuFrequencyMhz());
  Serial.println(NODEID);
  Serial.println(ESP.getFreeHeap());

  //ToDo wrong place
  Wire.begin(4, 15); // remapping of SPI for OLED
  //Wire.setClock(700000);

  OLED.begin();
  OLED.setFont(u8x8_font_chroma48medium8_r);
  OLED.drawString(0, 0, "F0007TH");

  //Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/, false /*PABOOST Enable*/, 868E6 /**/);
  //Heltec.display->flipScreenVertically();
  //Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  //Heltec.display->setFont(ArialMT_Plain_10);

  //Heltec.display->drawString(0, 0, "Hello world");
  //Heltec.display->display();

  // seems to be important to start that first
  // ISR seems to crash WLAN/IP stack
  // ToDo disable ISR during reconnect
  WiFiinit();

  RFinit(RxPin);
  nextsend = millis(); //update asap

  //if (!rfm69.initDevice(PinNSS, PinDIO0, CW, 868.95, GFSK, 100000, 40000, 5, PAind))
  if (!sx1276mbus.initDevice(PinNSS, PinDIO0)) //minRSSI
  {
    Serial.println("error initializing sx1276");
  }
  else
  {
    Serial.println("sx1276 ready");
  };
}

void checkcmd()
{
  String cmdstr;
  unsigned char reg, regval;
  char cmd;
  char *ptr;
  if (Serial.available() > 0)
  {
    cmdstr = Serial.readStringUntil('\n');
    //we assume "register, value" = 5 byte
    cmd = cmdstr.charAt(0);
    reg = strtoul(cmdstr.substring(1, 3).c_str(), &ptr, 16);

    if (cmd == 'w')
    {
      sx1276mbus.setModeStdby();
      regval = strtoul(cmdstr.substring(4, 6).c_str(), &ptr, 16);
      sx1276mbus.writeSPI(reg, regval);
      Serial.print("set ");
      Serial.print(reg, HEX);
      Serial.print(" to ");
    }
    if (cmd == 'r')
    {
      regval = sx1276mbus.readSPI(reg);
      Serial.print(reg, HEX);
      Serial.print(" is: ");
    }

    Serial.println(regval, HEX);
  }
}

void loop()
{
  /*
 byte idx =  check_RF_state(RxPin);
 if (idx >0 ) { 
   // ToDo decouple it object driffen approach
    OLED.clearLine(idx);
    snprintf(displaystr, 17, "%d:%sC %2d%% %4d", idx, tempstr[idx-1],chHum[idx-1], diff / 1000);
    OLED.drawString(0, idx, displaystr);
 }
 */

  if (sx1276mbus.receiveSizedFrame(FixPktSize, 170)) //minRSSI to reduce noice load
  {
    byte RSSI = sx1276mbus.getLastRSSI();
    if ( RSSI < 250) {
      
      printmsg(sx1276mbus._RxBuffer, sx1276mbus._RxBufferLen, RSSI );
      uint32_t myheatmeterserial = get_serial(mBusMsg);
     
      if (0x30585388 == myheatmeterserial) {
        char str[12];
        sprintf(str, "%ul", get_current(mBusMsg));
        OLED.drawString(10,0, str);
      }

    }
  }

  checkcmd();
  /*
  if ((millis() > nextsend) && (rxstate == 0))
  //rxstate 0 is in the beginning, waitung for the first edge, ISR disabled
  {
    snprintf(sendstr, 99, "nodeid=%s&values=%s;%d;%lu;%s;%d;%lu;%s;%d;%lu;%s;%d;%lu", NODEID,
             temp2str(0), chHum[0], chLastRecv[0],
             temp2str(1), chHum[1], chLastRecv[1],
             temp2str(2), chHum[2], chLastRecv[2],
             temp2str(3), chHum[3], chLastRecv[3]);
    Serial.println(sendstr);
    if (send2google(sendstr))
    {
      nextsend += period; //send ok, next period
    }
    else
    {
      nextsend += 30000; // send nok retry 30s later
    }
    
        Serial.println(ESP.getMinFreeHeap());
  }
  */
  /*
  if ((millis() > nextdraw) ) //&& (rxstate == 0))
  {
    unsigned long now1 = millis() / 1000UL;
    snprintf(displaystr, 17, "%2luT %2lu:%02lu:%02lu", elapsedDays(now1), numberOfHours(now1), numberOfMinutes(now1), numberOfSeconds(now1));
    OLED.drawString(0, 7, displaystr);
    nextdraw += 3000;
  }
  */

} // end of mainloop
