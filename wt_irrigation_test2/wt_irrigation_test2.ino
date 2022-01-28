/*
    Output the temperature readings to all pixels to be read by a Processing visualizer
*/
// Include the required libraries.
#include <TFT_eSPI.h>
#include <Wire.h>

#define USE_MLX90640

#ifndef USE_MLX90641
#include "MLX90640_API.h"
#else
#include "MLX90641_API.h"
#endif

#include "MLX9064X_I2C_Driver.h"

#if defined(ARDUINO_ARCH_AVR)
#define debug Serial
#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_SAM)
#define debug Serial
#else
#define debug Serial
#endif

#ifdef USE_MLX90641
const byte MLX90641_address = 0x33; // Default 7-bit unshifted address of the MLX90641
#define TA_SHIFT 8                  // Default shift for MLX90641 in open air
uint16_t eeMLX90641[832];
float MLX90641To[192];
uint16_t MLX90641Frame[242];
paramsMLX90641 MLX90641;
int errorno = 0;

#else
// Define the MLX90640 Thermal Imaging Camera settings:
const byte MLX90640_address = 0x33; // Default 7-bit unshifted address of the MLX90640
#define TA_SHIFT 8 // Default shift for MLX90640 in open air
uint16_t eeMLX90640[832];
static float mlx90640To[768];
paramsMLX90640 mlx90640;

#endif

// Define the TFT screen:
TFT_eSPI tft;

//  Define the maximum and minimum temperature values:
uint16_t MinTemp = 21;
uint16_t MaxTemp = 45;

// Define the data holders:
// String MLX90640_data = "";
byte red, green, blue;
float a, b, c, d;

void setup()
{
  // Initialize Serial Monitor
  debug.begin(115200); // Fast debug as possible

  // Initialize the I2C clock for the MLX90641 thermal imaging camera.
  Wire.begin();
  Wire.setClock(400000); // Increase I2C clock speed to 400kHz

  // Initialize the TFT screen
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

#ifndef USE_MLX90641 // MLX90640 is using

  // Print Debug Information on TFT
  tft.drawString("MLX90640 IR Array Example", 5, 10);
  if (isConnected() == false)
  {
    debug.println("MLX9064x not detected at default I2C address. Please check wiring. Freezing.");
    // Check the connection status between the MLX90640 & Wio Terminal.
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("MLX90640 not detected at default I2C address!", 5, 20);
    tft.drawString("Please check wiring. Freezing.", 5, 30);
    while (1)
      ;
  }
  else
  {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("MLX90640 online!", 5, 20);
  }

  // Get device parameters - We only have to do this once
  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Failed to load system parameters!", 5, 30);
    debug.println("Failed to load system parameters");
    while (1)
      ;
  }

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Parameter extraction failed", 5, 30);
    debug.println("Parameter extraction failed");
  }

  // Once params are extracted, we can release eeMLX90640 array

  // MLX90640_SetRefreshRate(MLX90640_address, 0x02); //Set rate to 2Hz
  MLX90640_SetRefreshRate(MLX90640_address, 0x03); // Set rate to 4Hz
  // MLX90640_SetRefreshRate(MLX90640_address, 0x07); //Set rate to 64H

#else

  if (isConnected() == false)
  {
    debug.println("MLX90641 not detected at default I2C address. Please check wiring. Freezing.");
    while (1)
      ;
  }
  // Get device parameters - We only have to do this once
  int status;
  status = MLX90641_DumpEE(MLX90641_address, eeMLX90641);
  errorno = status; // MLX90641_CheckEEPROMValid(eeMLX90641);//eeMLX90641[10] & 0x0040;//

  if (status != 0)
  {
    debug.println("Failed to load system parameters");
    while (1)
      ;
  }

  status = MLX90641_ExtractParameters(eeMLX90641, &MLX90641);
  // errorno = status;
  if (status != 0)
  {
    debug.println("Parameter extraction failed");
    while (1)
      ;
  }

  // Once params are extracted, we can release eeMLX90641 array

  // MLX90641_SetRefreshRate(MLX90641_address, 0x02); //Set rate to 2Hz
  MLX90641_SetRefreshRate(MLX90641_address, 0x03); // Set rate to 4Hz
  // MLX90641_SetRefreshRate(MLX90641_address, 0x07); //Set rate to 64Hz

#endif

  // Get the cutoff points:
  Getabcd();

  // Menu setup:
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Waiting to setup menu.....", 5, 40);
  delay(3000);
  tft.fillScreen(TFT_BLACK); // Clear screen
}

void loop()
{

  // Draw the options menu:
  int start_x = 64;
  int start_y = 20;
  int w = 6;
  int h = 6;
  draw_menu(start_x, start_y, w, h); // 64, 20, 6, 6

  // Elicit the 32x24 pixel IR thermal imaging array generated by the MLX90640 Thermal Imaging Camera:
#ifndef USE_MLX90641

  long startTime = millis();
  for (byte x = 0; x < 2; x++)
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT; // Reflected temperature based on the sensor ambient temperature
    float emissivity = 0.95;
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
  }
  long stopTime = millis();

  int x = start_x;            // start_x = 64
  int y = start_y + (h * 23); // start_y = 20 h = 6
  uint32_t c = TFT_BLUE;

  for (int i = 0; i < 768; i++)
  {
    // if(x % 8 == 0) debug.println();
    debug.print(mlx90640To[i], 2);
    debug.print(",");
    // Display a simple image version of the collected data (array) on the screen:
    // Define the color palette:
    c = GetColor(mlx90640To[i]);
    // Draw image pixels (rectangles):
    tft.fillRect(x, y, 6, 6, c);
    x = x + 6;
    // start a new row
    int l = i++;
    if (l % 32 == 0)
    {
      x = start_x;
      y = y - h;
    }

  }
  debug.println("");
//  while (1);
#else

  long startTime = millis();
  for (byte x = 0; x < 2; x++)
  {
    int status = MLX90641_GetFrameData(MLX90641_address, MLX90641Frame);
    float vdd = MLX90641_GetVdd(MLX90641Frame, &MLX90641);
    float Ta = MLX90641_GetTa(MLX90641Frame, &MLX90641);
    float tr = Ta - TA_SHIFT; // Reflected temperature based on the sensor ambient temperature
    float emissivity = 0.95;
    MLX90641_CalculateTo(MLX90641Frame, &MLX90641, emissivity, tr, MLX90641To);
  }
  long stopTime = millis();
  /*
    debug.print("vdd=");
    debug.print(vdd,2);
    debug.print(",Ta=");
    debug.print(Ta,2);
    debug.print(",errorno=");
    debug.print(errorno,DEC);
    for (int x = 0 ; x < 64 ; x++) {
       debug.print(MLX90641Frame[x], HEX);
       debug.print(",");
    }
    delay(1000);
  */
  for (int x = 0; x < 192; x++)
  {
    debug.print(MLX90641To[x], 2);
    debug.print(",");
  }
  debug.println("");

#endif
}

boolean isConnected()
{
  // Returns true if the MLX90640 is detected on the I2C bus
#ifndef USE_MLX90641
  Wire.beginTransmission((uint8_t)MLX90640_address);
#else
  Wire.beginTransmission((uint8_t)MLX90641_address);
#endif
  if (Wire.endTransmission() != 0)
  {
    return (false); // Sensor did not ACK
  }
  return (true);
}

void Getabcd()
{
  // Get the cutoff points based on the given maximum and minimum temperature values.
  a = MinTemp + (MaxTemp - MinTemp) * 0.2121;
  b = MinTemp + (MaxTemp - MinTemp) * 0.3182;
  c = MinTemp + (MaxTemp - MinTemp) * 0.4242;
  d = MinTemp + (MaxTemp - MinTemp) * 0.8182;
}

void draw_menu(int start_x, int start_y, int w, int h)
{
  // Draw the border:
  int offset = 10;
  tft.drawRoundRect(start_x - offset, start_y - offset, (2 * offset) + (w * 32), (2 * offset) + (h * 24), 10, TFT_WHITE);
  // Draw options:
  int x_c = 52;
  int x_s = x_c - 7;
  int y_c = 210;
  int y_s = y_c - 11;
  int sp = 72;
  tft.setTextSize(1);
  /////////////////////////////////////
  tft.fillCircle(x_c, y_c, 20, TFT_WHITE);
  tft.drawChar(x_s, y_s, 'U', TFT_BLACK, TFT_WHITE, 3);
  tft.drawString("Excessive", x_c - 25, y_c - 33);
  /////////////////////////////////////
  tft.fillCircle(x_c + sp, y_c, 20, TFT_WHITE);
  tft.drawChar(x_s + sp, y_s, 'L', TFT_BLACK, TFT_WHITE, 3);
  tft.drawString("Sufficient", x_c + sp - 28, y_c - 33);
  /////////////////////////////////////
  tft.fillCircle(x_c + (2 * sp), y_c, 20, TFT_WHITE);
  tft.drawChar(x_s + (2 * sp), y_s, 'R', TFT_BLACK, TFT_WHITE, 3);
  tft.drawString("Moderate", x_s + (2 * sp) - 16, y_c - 33);
  /////////////////////////////////////
  tft.fillCircle(x_c + (3 * sp), y_c, 20, TFT_WHITE);
  tft.drawChar(x_s + (3 * sp), y_s, 'D', TFT_BLACK, TFT_WHITE, 3);
  tft.drawString("Dry", x_c + (3 * sp) - 8, y_c - 33);
}

uint16_t GetColor(float val)
{
  /*
    equations based on
    http://web-tech.ga-usa.com/2012/05/creating-a-custom-hot-to-cold-temperature-color-gradient-for-use-with-rrdtool/index.html
  */

  // Assign colors to the given temperature readings:
  // R:
  red = constrain(255.0 / (c - b) * val - ((b * 255.0) / (c - b)), 0, 255);
  // G:
  if ((val > MinTemp) & (val < a))
  {
    green = constrain(255.0 / (a - MinTemp) * val - (255.0 * MinTemp) / (a - MinTemp), 0, 255);
  }
  else if ((val >= a) & (val <= c))
  {
    green = 255;
  }
  else if (val > c)
  {
    green = constrain(255.0 / (c - d) * val - (d * 255.0) / (c - d), 0, 255);
  }
  else if ((val > d) | (val < a))
  {
    green = 0;
  }
  // B:
  if (val <= b)
  {
    blue = constrain(255.0 / (a - b) * val - (255.0 * b) / (a - b), 0, 255);
  }
  else if ((val > b) & (val <= d))
  {
    blue = 0;
  }
  else if (val > d)
  {
    blue = constrain(240.0 / (MaxTemp - d) * val - (d * 240.0) / (MaxTemp - d), 0, 240);
  }

  // Utilize the built-in color mapping function to get a 5-6-5 color palette (R=5 bits, G=6 bits, B-5 bits):
  return tft.color565(red, green, blue);
}
