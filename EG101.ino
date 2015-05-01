/*  EG101 Companion
    James Higgs 2014
    
    Provides extra front panel functionality for Roland EG101 Synth
*/

/*
  Software serial MIDI
 
  The circuit:
 * digital 4 connected to MIDI jack pin 5
 * MIDI jack pin 2 connected to ground
 * MIDI jack pin 4 connected to +5V through 220-ohm resistor

  Sotware serial drives the MIDI thru digital port 3 (in) and 4 (out)
 */
 
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <SPI.h>
#include <GD2.h>
#include <avr/pgmspace.h>
#include "patches.h"

#define midiRxPin  3
#define midiTxPin  4
SoftwareSerial midiSerial(midiRxPin, midiTxPin); // RX, TX

// Patch info for all 400 odd patches is stored in PROGMEM, only the current
// "page" of patches is stored in SRAM
PATCHINFO currentPagePatchInfo[20];

#define MAIN_PAGE      0
#define PATCAT_PAGE    1
#define REVERB_PAGE    2
#define CHORUS_PAGE    3
#define BENDMOD_PAGE   4
#define SYNTH_PAGE     5
#define PATCHES_PAGE0  101

#define BACKBUTTON_TAG  200

byte prevPressedTag = 0;
byte gCurrentPage = MAIN_PAGE;
byte mono = 0;

void setup()
{
  //GD.begin();
  
  // Special GD.begin() code to handle calibration of rotated screen
  GD.begin(~(GD_STORAGE | GD_CALIBRATE));
  GD.wr(REG_ROTATE, 0);
  if (EEPROM.read(0) != 0x7c)
    {
    GD.self_calibrate();
    for (int i = 0; i < 24; i++)
      EEPROM.write(1 + i, GD.rd(REG_TOUCH_TRANSFORM_A + i));
    EEPROM.write(0, 0x7c);  // is written!
    }
  else
    {
    for (int i = 0; i < 24; i++)
      GD.wr(REG_TOUCH_TRANSFORM_A + i, EEPROM.read(1 + i));
    }

  // define pin modes for MIDI in/out (rx, tx):
  pinMode(midiRxPin, INPUT);
  pinMode(midiTxPin, OUTPUT);
  // set the data rate for the SoftwareSerial port (MIDI rate)
  midiSerial.begin(31250);
  
  midiNoteOn(3, 60, 70);
  delay(200);
  midiNoteOn(3, 60, 0);
}

void loop()
{ 
  // Get input
  GD.get_inputs();
  byte pressedTag = GD.inputs.tag;

  GD.ClearColorRGB(0 == pressedTag ? 0x103000 : 0x106000);
  GD.Clear();
  GD.cmd_text(0, 0, 30, 0, "GrooveBuddy");

  if (MAIN_PAGE == gCurrentPage)
    DoMainPage(pressedTag);
  else if (PATCAT_PAGE == gCurrentPage)
    DoPatchCategoryPage(pressedTag);
  else if (REVERB_PAGE == gCurrentPage)
    DoReverbPage(pressedTag);
  else if (CHORUS_PAGE == gCurrentPage)
    DoChorusPage(pressedTag);
  else if (BENDMOD_PAGE == gCurrentPage)
    DoBendModPage(pressedTag);
  else if (SYNTH_PAGE == gCurrentPage)
    DoSynthPage(pressedTag);
  else if (gCurrentPage >= PATCHES_PAGE0 && gCurrentPage <= (PATCHES_PAGE0 + 21))
    DoPatchesPage(pressedTag);
  
  prevPressedTag = pressedTag;
}

// Show and process input for "Main" page
void DoMainPage(byte pressedTag)
{
  GD.Tag(PATCAT_PAGE);
  GD.cmd_button(20, 60, 160, 48, 28, PATCAT_PAGE == pressedTag ? OPT_FLAT : 0, "Patches");
  GD.Tag(REVERB_PAGE);
  GD.cmd_button(20, 120, 160, 48, 28, REVERB_PAGE == pressedTag ? OPT_FLAT : 0, "Reverb");
  GD.Tag(CHORUS_PAGE);
  GD.cmd_button(20, 180, 160, 48, 28, CHORUS_PAGE == pressedTag ? OPT_FLAT : 0, "Chorus");

  GD.Tag(BENDMOD_PAGE);
  GD.cmd_button(260, 60, 160, 48, 28, BENDMOD_PAGE == pressedTag ? OPT_FLAT : 0, "Bend + Mod");
  GD.Tag(SYNTH_PAGE);
  GD.cmd_button(260, 120, 160, 48, 28, SYNTH_PAGE == pressedTag ? OPT_FLAT : 0, "Synth");
  GD.Tag(20);
  GD.cmd_button(260, 180, 100, 44, 28, 20 == pressedTag ? OPT_FLAT : 0, (1 == mono) ? "Mono" : "Poly");

  GD.swap();    // swap buffers
  
  if (pressedTag >= PATCAT_PAGE && pressedTag <= SYNTH_PAGE)
    {
    gCurrentPage = pressedTag;
    prevPressedTag = 0;
    delay(100);
    }
  else if (20 == pressedTag)
    {
    // Toggle mono/poly button
    if (0 == mono)
      {
      // Send "Mono" CC 
      midiControlChange(3, 126, 1);
      mono = 1;
      }
    else
      {
      // Send "Poly" CC 
      midiControlChange(3, 127, 0);
      mono = 0;
      }
    delay(100);
    }
  
}

// Show and process input for "Patch Category" page
void DoPatchCategoryPage(byte pressedTag)
{
  // Generate display of patch category buttons
  for (int i = 0; i < 22; i++)
    {
    GD.Tag(i + 1);
    int x = (i / 5) * 96;
    int y = 48 + (i % 5) * 45;
    GD.cmd_button(x, y, 92, 42, 28, (i + 1) == pressedTag ? OPT_FLAT : 0, patchCategories[i]);
    }   

  GD.Tag(BACKBUTTON_TAG);
  GD.cmd_button(400, 0, 80, 42, 28, BACKBUTTON_TAG == pressedTag ? OPT_FLAT : 0, "Back");

  GD.swap();    // swap buffers

  if (BACKBUTTON_TAG == pressedTag)
    {
    gCurrentPage = 0;
    delay(100);
    return;
    }

   if (pressedTag >= 1 && pressedTag <= 22)
     {
     // Copy the relevant page of patch info to the SRAM copy
     int pageNumber = pressedTag - 1;
     memcpy_P(&currentPagePatchInfo, &patchInfo[20 * pageNumber], sizeof(PATCHINFO) * 20);
     // Set current page to a "patch page"     
     gCurrentPage = PATCHES_PAGE0 + pageNumber;
     prevPressedTag = 0;
     }

}

// Show and process input for "Synth Patches" page
void DoPatchesPage(byte pressedTag)
{
  GD.cmd_text(240, 0, 28, 0, patchCategories[gCurrentPage - PATCHES_PAGE0]);
  
  // Uses the copy of the "current page patch info" in local SRAM
  for (int i = 0; i < 20; i++)
    {
    GD.Tag(i + 1);
    int x = (i / 5) * 120;
    int y = 48 + (i % 5) * 45;
    GD.cmd_button(x, y, 116, 42, 27, (i + 1) == pressedTag ? OPT_FLAT : 0, currentPagePatchInfo[i].name);
    }   

  GD.Tag(BACKBUTTON_TAG);
  GD.cmd_button(400, 0, 80, 42, 28, BACKBUTTON_TAG == pressedTag ? OPT_FLAT : 0, "Back");

  GD.swap();    // swap buffers

  if (BACKBUTTON_TAG == pressedTag)
    {
    gCurrentPage = PATCAT_PAGE;
    delay(100);
    }
  else if (pressedTag >= 1 && pressedTag <= 20 && pressedTag != prevPressedTag)
    {
    // User pressed a patch button, so select the relevant patch
    int index = pressedTag - 1;
    if (currentPagePatchInfo[index].bank > 0)
      {
      midiPatchSelect(currentPagePatchInfo[index].bank, currentPagePatchInfo[index].pc - 1);
      midiNoteOn(3, 60, 70);
      delay(500);
      midiNoteOn(3, 60, 0);
      }
    }

}

// Show and process input for "Reverb" page
void DoReverbPage(byte pressedTag)
{
  GD.Tag(1);
  GD.cmd_button(10, 48, 100, 44, 28, 1 == pressedTag ? OPT_FLAT : 0, "Room 1");
  GD.Tag(2);
  GD.cmd_button(10, 96, 100, 44, 28, 2 == pressedTag ? OPT_FLAT : 0, "Room 2");
  GD.Tag(3);
  GD.cmd_button(10, 144, 100, 44, 28, 3 == pressedTag ? OPT_FLAT : 0, "Room 3");
  GD.Tag(6);
  GD.cmd_button(10, 192, 100, 44, 28, 6 == pressedTag ? OPT_FLAT : 0, "Plate");
  GD.Tag(4);
  GD.cmd_button(120, 48, 100, 44, 28, 4 == pressedTag ? OPT_FLAT : 0, "Hall 1");
  GD.Tag(5);
  GD.cmd_button(120, 96, 100, 44, 28, 5 == pressedTag ? OPT_FLAT : 0, "Hall 2");
  GD.Tag(7);
  GD.cmd_button(120, 144, 100, 44, 28, 7 == pressedTag ? OPT_FLAT : 0, "Delay");
  GD.Tag(8);
  GD.cmd_button(120, 192, 100, 44, 28, 8 == pressedTag ? OPT_FLAT : 0, "Pan Delay");


  GD.Tag(BACKBUTTON_TAG);
  GD.cmd_button(400, 0, 80, 44, 28, BACKBUTTON_TAG == pressedTag ? OPT_FLAT : 0, "Back");

  GD.swap();    // swap buffers

  if (BACKBUTTON_TAG == pressedTag)
    {
    gCurrentPage = MAIN_PAGE;
    delay(100);
    }
  else if (pressedTag >= 1 && pressedTag <= 8 && pressedTag != prevPressedTag)
    {
/*
    byte data[20] = { 0x41, 0x10, 0x00, 0x18, 0x12, 0x40, 0x01, 0x30, 0x00, 0x00 };
    data[8] = pressedTag - 1;
    byte checksum = GetRolandChecksum(data+5, 4);
    data[9] = checksum;
    midiSysex(data, 10);
*/
    midiSysEx(0x01, 0x30, pressedTag - 1);    
    
    midiNoteOn(3, 60, 70);
    delay(500);
    midiNoteOn(3, 60, 0);
    }

}

// Show and process input for "Chorus" page
void DoChorusPage(byte pressedTag)
{
  static uint16_t value = 0;
  if (20 == GD.inputs.track_tag & 0xFF)
    value = GD.inputs.track_val;
  
  GD.Tag(11);
  GD.cmd_button(10, 48, 100, 44, 28, 11 == pressedTag ? OPT_FLAT : 0, "Chorus 1");
  GD.Tag(12);
  GD.cmd_button(10, 96, 100, 44, 28, 12 == pressedTag ? OPT_FLAT : 0, "Chorus 2");
  GD.Tag(13);
  GD.cmd_button(10, 144, 100, 44, 28, 13 == pressedTag ? OPT_FLAT : 0, "Chorus 3");
  GD.Tag(14);
  GD.cmd_button(10, 192, 100, 44, 28, 14 == pressedTag ? OPT_FLAT : 0, "Chorus 4");
  GD.Tag(15);
  GD.cmd_button(120, 48, 100, 44, 28, 15 == pressedTag ? OPT_FLAT : 0, "FB Chorus");
  GD.Tag(16);
  GD.cmd_button(120, 96, 100, 44, 28, 16 == pressedTag ? OPT_FLAT : 0, "Flanger");
  GD.Tag(17);
  GD.cmd_button(120, 144, 100, 44, 28, 17 == pressedTag ? OPT_FLAT : 0, "Sht Delay");
  GD.Tag(18);
  GD.cmd_button(120, 192, 100, 44, 28, 18 == pressedTag ? OPT_FLAT : 0, "S Delay FB");

  // "Depth" slider
  GD.Tag(20);
  GD.cmd_slider(300, 48, 20, 200, 0, value, 65535);
  GD.cmd_track(300, 48, 20, 200, 20);

  // Back button
  GD.Tag(BACKBUTTON_TAG);
  GD.cmd_button(400, 0, 80, 44, 28, BACKBUTTON_TAG == pressedTag ? OPT_FLAT : 0, "Back");

  GD.swap();    // swap buffers

  if (BACKBUTTON_TAG == pressedTag)
    {
    gCurrentPage = MAIN_PAGE;
    delay(100);
    }
  else if (pressedTag >= 11 && pressedTag <= 18 && pressedTag != prevPressedTag)
    {
    midiSysEx(0x01, 0x38, pressedTag - 11);

    midiNoteOn(3, 60, 70);
    delay(500);
    midiNoteOn(3, 60, 0);
    }
  else if (20 == GD.inputs.track_tag & 0xFF)
    {
    // Chorus send level (0 to 127)
    midiControlChange(3, 93, 127 - (value / 512));
    delay(100);
    }
  
}

// Show and process input for "Bend+Mod" page
void DoBendModPage(byte pressedTag)
{
  static uint16_t bendRange = 32768;   // 
  static uint16_t bendCutoff = 32768;  // 50%
  static uint16_t modRate = 32768;    // 50%
  static uint16_t modPitch = 2560;    // 10 / 127
  static uint16_t modCutoff = 0;
  static uint16_t modVol = 0;

  GD.cmd_text(20, 40, 28, 0, "Bend");
  GD.cmd_text(240, 40, 28, 0, "Mod");

  //GD.cmd_text(20, 248, 27, 0, "Pitch");
  //GD.cmd_text(84, 248, 27, 0, "Cutoff");
  char s[20];
  sprintf(s, "%u", (65535 - bendRange) / 2730);
  GD.cmd_text(20, 248, 27, 0, s);
  sprintf(s, "%u", (65535 - bendCutoff) / 512);
  GD.cmd_text(84, 248, 27, 0, s);

  GD.cmd_text(228, 248, 27, 0, "Rate");
  GD.cmd_text(288, 248, 27, 0, "Pitch");
  GD.cmd_text(348, 248, 27, 0, "Cutoff");
  GD.cmd_text(408, 248, 27, 0, "Volume");

  // "Bend Pitch Range" slider
  GD.Tag(20);
  GD.cmd_slider(32, 80, 20, 160, 0, bendRange, 65535);
  GD.cmd_track(32, 80, 20, 160, 20);

  // "Bend Cutoff Range" slider
  GD.Tag(21);
  GD.cmd_slider(96, 80, 20, 160, 0, bendCutoff, 65535);
  GD.cmd_track(96, 80, 20, 160, 21);

  // "Mod Rate" slider
  GD.Tag(22);
  GD.cmd_slider(240, 80, 20, 160, 0, modRate, 65535);
  GD.cmd_track(240, 80, 20, 160, 22);

  // "Mod Pitch" slider
  GD.Tag(23);
  GD.cmd_slider(300, 80, 20, 160, 0, modPitch, 65535);
  GD.cmd_track(300, 80, 20, 160, 23);

  // "Mod Cutoff" slider
  GD.Tag(24);
  GD.cmd_slider(360, 80, 20, 160, 0, modCutoff, 65535);
  GD.cmd_track(360, 80, 20, 160, 24);

  // "Mod Vol" slider
  GD.Tag(25);
  GD.cmd_slider(420, 80, 20, 160, 0, modVol, 65535);
  GD.cmd_track(420, 80, 20, 160, 25);


  // Back button
  GD.Tag(BACKBUTTON_TAG);
  GD.cmd_button(400, 0, 80, 44, 28, BACKBUTTON_TAG == pressedTag ? OPT_FLAT : 0, "Back");

  GD.swap();    // swap buffers

  if (BACKBUTTON_TAG == pressedTag)
    {
    gCurrentPage = MAIN_PAGE;
    delay(100);
    }
  else
    {
    byte tt = (GD.inputs.track_tag & 0xFF);
    if (20 == tt)
      {
      // Bend pitch range (40 to 87, default = 64)
      bendRange = GD.inputs.track_val;
      midiRPN(3, 0, 0, ((65535 - bendRange) / 2730));    // 1365 = 65536 / 48
      delay(50);
      }
    else if (21 == tt)
      {
      // Bend filter cutoff (0 to 127, default = 64)
      bendCutoff = GD.inputs.track_val;
      midiSysEx(0x24, 0x11, (byte)((65535 - bendCutoff) / 512));
      }
    else if (22 == tt)
      {
      // Mod rate (0 to 127, default = 64)
      modRate = GD.inputs.track_val;
      midiSysEx(0x24, 0x03, (byte)((65535 - modRate) / 512));
      }
    else if (23 == tt)
      {
      // Mod pitch (0 to 127, default = 10)
      modPitch = GD.inputs.track_val;
      midiSysEx(0x24, 0x04, (byte)((65535 - modPitch) / 512));
      }
    else if (24 == tt)
      {
      // Mod cutoff (0 to 127, default = 0)
      modCutoff = GD.inputs.track_val;
      midiSysEx(0x24, 0x05, (byte)((65535 - modCutoff) / 512));
      }
    else if (25 == tt)
      {
      // Mod vol (0 to 127, default = 0)
      modVol = GD.inputs.track_val;
      midiSysEx(0x24, 0x06, (byte)((65535 - modVol) / 512));
      }
    }
  
}

// Show and process input for "Synth" page
void DoSynthPage(byte pressedTag)
{
  static uint16_t vibRate = 32767;
  static uint16_t vibDepth = 32767;
  static uint16_t vibDelay = 32767;
  static uint16_t envAttack = 32767; 
  static uint16_t envDecay = 32767;
  static uint16_t envRelease = 32767;

  GD.cmd_text(20, 40, 28, 0, "Vibrato");
  GD.cmd_text(240, 40, 28, 0, "Envelope");

  GD.cmd_text(20, 248, 27, 0, "Rate");
  GD.cmd_text(84, 248, 27, 0, "Depth");
  GD.cmd_text(148, 248, 27, 0, "Delay");

  GD.cmd_text(228, 248, 27, 0, "Attack");
  GD.cmd_text(288, 248, 27, 0, "Decay");
  GD.cmd_text(348, 248, 27, 0, "Release");

  // "Vibrato rate" slider
  GD.Tag(20);
  GD.cmd_slider(32, 80, 20, 160, 0, vibRate, 65535);
  GD.cmd_track(32, 80, 20, 160, 20);

  // "Vibrato depth" slider
  GD.Tag(21);
  GD.cmd_slider(96, 80, 20, 160, 0, vibDepth, 65535);
  GD.cmd_track(96, 80, 20, 160, 21);

  // "Vibrato delay" slider
  GD.Tag(22);
  GD.cmd_slider(160, 80, 20, 160, 0, vibDelay, 65535);
  GD.cmd_track(160, 80, 20, 160, 22);


  // "Env Attack" slider
  GD.Tag(23);
  GD.cmd_slider(240, 80, 20, 160, 0, envAttack, 65535);
  GD.cmd_track(240, 80, 20, 160, 23);

  // "Env Decay" slider
  GD.Tag(24);
  GD.cmd_slider(300, 80, 20, 160, 0, envDecay, 65535);
  GD.cmd_track(300, 80, 20, 160, 24);

  // "Env Release" slider
  GD.Tag(25);
  GD.cmd_slider(360, 80, 20, 160, 0, envRelease, 65535);
  GD.cmd_track(360, 80, 20, 160, 25);



  // Back button
  GD.Tag(BACKBUTTON_TAG);
  GD.cmd_button(400, 0, 80, 44, 28, BACKBUTTON_TAG == pressedTag ? OPT_FLAT : 0, "Back");

  GD.swap();    // swap buffers

  if (BACKBUTTON_TAG == pressedTag)
    {
    gCurrentPage = MAIN_PAGE;
    delay(100);
    }
  else
    {
    byte tt = (GD.inputs.track_tag & 0xFF);
    if (20 == tt)
      {
      // Vibrato rate (0 to 127, def = 64)
      vibRate = GD.inputs.track_val;
      midiNRPN(3, 0x01, 0x08, ((65535 - vibRate) / 512));
      delay(50);
      }
    else if (21 == tt)
      {
      // Vibrato depth (0 to 127, def = 64)
      vibDepth = GD.inputs.track_val;
      midiNRPN(3, 0x01, 0x09, ((65535 - vibDepth) / 512));
      }
    else if (22 == tt)
      {
      // Vibrato delay (0 to 127, def = 64)
      vibDelay = GD.inputs.track_val;
      midiNRPN(3, 0x01, 0x0A, ((65535 - vibDelay) / 512));
      }
    else if (23 == tt)
      {
      // Envelope attack time (0 to 127, def = 64)
      envAttack = GD.inputs.track_val;
      midiNRPN(3, 0x01, 0x63, ((65535 - envAttack) / 512));
      }
    else if (24 == tt)
      {
      // Envelope decay time (0 to 127, def = 64)
      envDecay = GD.inputs.track_val;
      midiNRPN(3, 0x01, 0x64, ((65535 - envDecay) / 512));
      }
    else if (25 == tt)
      {
      // Envelope release time (0 to 127, def = 64)
      envRelease = GD.inputs.track_val;
      midiNRPN(3, 0x01, 0x66, ((65535 - envRelease) / 512));
      }
    }
  
}

//  plays a MIDI note.  Doesn't check to see that
//  cmd is greater than 127, or that data values are  less than 127:
void midiNoteOn(byte channel, byte pitch, byte velocity)
{
//  Serial.write(cmd);
//  Serial.write(pitch);
//  Serial.write(velocity);
  midiSerial.write(0x90 | channel);
  midiSerial.write(pitch);
  midiSerial.write(velocity);
}

//  Sends a MIDI control change
void midiControlChange(byte channel, byte ccNum, byte value)
{
//  Serial.write(cmd);
//  Serial.write(pitch);
//  Serial.write(velocity);
  midiSerial.write(0xB0 | channel);
  midiSerial.write(ccNum);
  midiSerial.write(value);
  delay(20);
}

//  Sends a MIDI RPN change
void midiRPN(byte channel, byte cc100, byte cc101, byte value)
{
  midiSerial.write(0xB0 | channel);
  midiSerial.write(0x64);
  midiSerial.write(cc100);
  //midiSerial.write(0xB0 | channel);
  midiSerial.write(0x65);
  midiSerial.write(cc101);
  //midiSerial.write(0xB0 | channel);
  midiSerial.write(0x06);
  midiSerial.write(value);
  // Set RPN to null so that further spuriuous controller messages are ignored
  midiSerial.write(0x64);
  midiSerial.write(0x7F);
  midiSerial.write(0x65);
  midiSerial.write(0x7F);
  delay(50);
}

//  Sends a MIDI RPN change
void midiNRPN(byte channel, byte msb, byte lsb, byte value)
{
  midiSerial.write(0xB0 | channel);
  midiSerial.write(0x63);
  midiSerial.write(msb);
  //midiSerial.write(0xB0 | channel);
  midiSerial.write(0x62);
  midiSerial.write(lsb);
  //midiSerial.write(0xB0 | channel);
  midiSerial.write(0x06);
  midiSerial.write(value);
  // Set RPN to null so that further spuriuous controller messages are ignored
  midiSerial.write(0x62);
  midiSerial.write(0x7F);
  midiSerial.write(0x63);
  midiSerial.write(0x7F);
  delay(50);
}

//  Sends a MIDI control change
void midiPatchSelect(byte bank, byte patch)
{
//  Serial.write(cmd);
//  Serial.write(pitch);
//  Serial.write(velocity);
  // Write bank in CC00 and CC32
  midiSerial.write(0xB3);
  midiSerial.write(byte(0));
  midiSerial.write(bank);
  delay(5);
  midiSerial.write(0xB3);
  midiSerial.write(32);
  midiSerial.write(byte(0));
  delay(5);
  // Write PC
  midiSerial.write(0xC3);
  midiSerial.write(patch);
  delay(40);
}

/*
// Send sysex data
void midiSysex(byte *data, int length)
{
  // Sysex starts wth 0xF0, ends with 0xF7
  midiSerial.write(0xF0);
  for (int i = 0; i < length; i++)
    midiSerial.write(data[i]);
  
  midiSerial.write(0xF7);
  
  delay(50);
}
*/

// Send sysex data
void midiSysEx(byte addr1, byte addr2, byte value)
{
  byte data[20] = { 0x41, 0x10, 0x00, 0x18, 0x12, 0x40, 0x00, 0x00, 0x00, 0x00 };
  data[6] = addr1;
  data[7] = addr2;
  data[8] = value;
  byte checksum = GetRolandChecksum(data+5, 4);
  data[9] = checksum;

  // Sysex starts wth 0xF0, ends with 0xF7
  midiSerial.write(0xF0);
  for (int i = 0; i < 10; i++)
    midiSerial.write(data[i]);
  
  midiSerial.write(0xF7);
  
  delay(50);
}

// calculate Roland sysex checksum
byte GetRolandChecksum(byte* data, int length)
{
  // Roland sysex checksum = 128 - (data sum % 128)
  int sum = 0;
  for (int i = 0; i < length; i++)
    sum += data[i];
  
  int rem = sum % 128;
  int checksum = (128 - rem);
  //int check = sum + checksum;
  return byte(checksum);
}

/*
  Notes:
  1. Reverb depth = CC 91  (eg: 0xB3 91 value)
  2. Chorsu depth = CC 93  (eg: 0xB3 93 value)
*/





