/*
 * ArduinoGotchi - A real Tamagotchi emulator for Arduino UNO
 *
 * Copyright (C) 2022 Gary Kwok
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

#include <ESP_EEPROM.h>
#include "tamalib.h"
#include "hw.h"
#include "bitmaps.h"
#include "hardcoded_state.h"

ESPboyInit myESPboy;
TFT_eSprite spr = TFT_eSprite(&myESPboy.tft);

uint32_t foregrnd = TFT_YELLOW;
uint32_t backgrnd = TFT_BROWN;

/***** Tama Setting and Features *****/
#define START_EEPROM_WRITE 10
#define TAMA_DISPLAY_FRAMERATE 5   // 3 is optimal for Arduino UNO
#define AUTO_SAVE_MINUTES 100
//#define ENABLE_LOAD_HARCODED_STATE_WHEN_START
#define TFTSHIFT 20
#define ICONSHIFT TFTSHIFT+70
#define GAME_ID 0xFA

/**** TamaLib Specific Variables ****/
static uint16_t current_freq = 0; 
static bool_t matrix_buffer[LCD_WIDTH][LCD_HEIGHT] = {{0}};
static byte runOnceBool = 0;
static bool_t icon_buffer[ICON_NUM] = {0};
static cpu_state_t cpuState;
static uint32_t lastSaveTimestamp = 0;


static void hal_halt(void) {
  //Serial.println("HALT GAME"); 
}


static void hal_log(log_level_t level, char *buff, ...) {
  //Serial.println(buff); 
}


static void hal_sleep_until(timestamp_t ts) {
  while (ts > hal_get_timestamp()) delay(100);
}


static timestamp_t hal_get_timestamp(void) {
  return millis() * 1000;
}


static void hal_update_screen(void) {
  displayTama();
} 


static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val) {
  matrix_buffer[x][y] = val;
}


static void hal_set_lcd_icon(u8_t icon, bool_t val) {
  icon_buffer[icon] = val;
}


static void hal_set_frequency(u32_t freq) {
  //Serial.println("SET FREQ"); 
  current_freq = freq;
}


static void hal_play_frequency(bool_t en) {
  if(en) myESPboy.playTone(current_freq, 100); 
  else   myESPboy.noPlayTone();  
}



static int hal_handler(void) {
  uint8_t getkeys = myESPboy.getKeys();
  
  if (getkeys&PAD_UP) hw_set_button(BTN_LEFT, BTN_STATE_PRESSED );
  else hw_set_button(BTN_LEFT, BTN_STATE_RELEASED );
  
  if (getkeys&PAD_ACT) hw_set_button(BTN_MIDDLE, BTN_STATE_PRESSED );
  else hw_set_button(BTN_MIDDLE, BTN_STATE_RELEASED );
  
  if (getkeys&PAD_ESC) hw_set_button(BTN_RIGHT, BTN_STATE_PRESSED );
  else hw_set_button(BTN_RIGHT, BTN_STATE_RELEASED );

  if (getkeys&PAD_LFT || getkeys&PAD_RGT) {
    saveStateToEEPROM();
    while (myESPboy.getKeys()) delay(100);
    delay(500);
  }
  
  return 0;
}



void displayTama() {
  //myESPboy.tft.fillScreen(TFT_BLACK);
  spr.fillScreen(TFT_BLACK);
  for (uint8_t i=0; i<LCD_HEIGHT; i++)
    for (uint8_t j=0; j<LCD_WIDTH; j++)
       if(matrix_buffer[j][i])
          spr.fillRect(j*4, i*4, 3, 3, 1);

  spr.pushSprite(0, TFTSHIFT); 
  
  for(uint8_t i=0; i<ICON_NUM; i++) {
    if(!icon_buffer[i]) myESPboy.tft.drawXBitmap(i*14+10, ICONSHIFT, bitmaps+i*18, 9, 9, foregrnd, backgrnd);
    else myESPboy.tft.drawXBitmap(i*14+10, ICONSHIFT, bitmaps+i*18, 9, 9, backgrnd, foregrnd);
  }

  for(uint8_t i=0; i<ICON_NUM; i++) 
    if(icon_buffer[i]) myESPboy.tft.drawRect(i*14+10-2, ICONSHIFT-2, 13, 13, foregrnd);
    else myESPboy.tft.drawRect(i*14+10-2, ICONSHIFT-2, 13, 13, backgrnd);
}



void loadHardcodedState() {
  //Serial.println("LOAD HARDCODED STATE"); 
  
  cpu_get_state(&cpuState);
  u4_t *memTemp = cpuState.memory;
  uint8_t *cpuS = (uint8_t *)&cpuState;
  
  for(uint16_t i=0; i<sizeof(cpu_state_t); i++)
    cpuS[i]=pgm_read_byte(hardcodedState+i);
  
  for(uint16_t i=0; i<MEMORY_SIZE; i++)
    memTemp[i]=pgm_read_byte(hardcodedState+ sizeof(cpu_state_t) + i);
    
  cpuState.memory = memTemp;
  cpu_set_state(&cpuState);
}



void saveStateToEEPROM() {
  //Serial.println("SAVE STATE"); 

  myESPboy.playTone(current_freq, 0);
  myESPboy.playTone(current_freq, 50);
  myESPboy.tft.setTextColor(foregrnd);                    
  myESPboy.tft.drawString("SAVE STATE...", 2, 2);
  
  if (EEPROM.read(START_EEPROM_WRITE)!=GAME_ID)
    for (uint16_t i = 0 ; i <  1 + sizeof(cpu_state_t) + MEMORY_SIZE; i++){
      EEPROM.write(START_EEPROM_WRITE+i, 0);
      delay(1);
    }
    
  EEPROM.write(START_EEPROM_WRITE, GAME_ID);
  cpu_get_state(&cpuState);
  EEPROM.put(START_EEPROM_WRITE+1, cpuState); 
  
  for(uint16_t i=0;i<MEMORY_SIZE;i++){
    EEPROM.write(START_EEPROM_WRITE + 1 + sizeof(cpu_state_t) + i, cpuState.memory[i]);
    delay(1);}
  EEPROM.commit();

  myESPboy.tft.fillScreen(TFT_BLACK);
  displayTama();
}


void loadStateFromEEPROM() {
  //Serial.println("LOAD STATE"); 
  
  cpu_get_state(&cpuState);
  u4_t *memTemp = cpuState.memory;
  EEPROM.get(START_EEPROM_WRITE+1, cpuState);
  cpu_set_state(&cpuState);
  
  for(uint16_t i=0;i<MEMORY_SIZE;i++){
    memTemp[i] = EEPROM.read(START_EEPROM_WRITE + 1 + sizeof(cpu_state_t) + i);
    delay(1);}
}


static hal_t hal = {
  .halt = &hal_halt,
  .log = &hal_log,
  .sleep_until = &hal_sleep_until,
  .get_timestamp = &hal_get_timestamp,
  .update_screen = &hal_update_screen,
  .set_lcd_matrix = &hal_set_lcd_matrix,
  .set_lcd_icon = &hal_set_lcd_icon,
  .set_frequency = &hal_set_frequency,
  .play_frequency = &hal_play_frequency,
  .handler = &hal_handler,
};


void setup() {
  //Serial.begin(115200);
  EEPROM.begin(START_EEPROM_WRITE + 1 + sizeof(cpu_state_t) + MEMORY_SIZE);

  //Serial.println();
  //Serial.println("START GAME"); 
  //Serial.println(START_EEPROM_WRITE + 1 + sizeof(cpu_state_t) + MEMORY_SIZE);
  
  myESPboy.begin("Tamagotchi P1");
  spr.setColorDepth(1);
  spr.createSprite(128, 64);
  spr.setBitmapColor(foregrnd, backgrnd);
  
  tamalib_register_hal(&hal);
  tamalib_set_framerate(TAMA_DISPLAY_FRAMERATE);
  tamalib_init(1000000);

  if (EEPROM.read(START_EEPROM_WRITE) == GAME_ID) {
    loadStateFromEEPROM();
  }

#ifdef ENABLE_LOAD_HARCODED_STATE_WHEN_START
  loadHardcodedState();
#endif
}



void loop() {
  tamalib_mainloop_step_by_step();   
  if ((millis() - lastSaveTimestamp) > (AUTO_SAVE_MINUTES * 60 * 1000)) {
    lastSaveTimestamp = millis();
    saveStateToEEPROM();
  }
}
