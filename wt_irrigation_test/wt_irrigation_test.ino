/*
  The camera can present dynamic thermal images and detect the surrounding temperature from -40℃~300℃
  The camera with narrow-angle/wide-angle has an FOV(Field of View) of 55°x35°/110°x75°
  MCU RAM > 20KB to drive the camera
*/
// Include the required libraries.
#include <TFT_eSPI.h>
#include <Wire.h>
#include "MLX90640_API.h"
#include "MLX9064X_I2C_Driver.h"

// Define the TFT screen:
TFT_eSPI tft;

// Define the MLX90640 Thermal Imaging Camera settings:
const byte MLX90640_address = 0x33; // Default 7-bit unshifted address of the MLX90640
#define TA_SHIFT 8                  // Default shift for MLX90640 in open air
uint16_t eeMLX90640[832];
static float mlx90640To[768]; // 32x24 array of thermal sensors
paramsMLX90640 mlx90640;

uint16_t mlx90640Frame[834];

// int errorno = 0
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
  Serial.begin(115200);

  // Initialize the I2C clock for the MLX90641 thermal imaging camera.
  Wire.begin();
  Wire.setClock(2000000); // Increase the I2C clock speed to 2M. (400kHz: 400000)
  // Initialize the TFT screen
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  // Print Debug Information on TFT
  tft.drawString("MLX90640 IR Array Example", 5, 10);
  if (isConnected() == false)
  { // Check the connection status between the MLX90640 & Wio Terminal.
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

  // Get the MLX90640 Thermal Imaging Camera parameters: we only have to do this once
  int status;
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Failed to load system parameters!", 5, 30);
    while (1)
      ;
  }

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Parameter extraction failed", 5, 30);
  }

  // Once params are extracted, we can release eeMLX90640 array
  MLX90640_SetRefreshRate(MLX90640_address, 0x07); // Set rate to 64Hz.

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
  get_and_display_data_from_MLX90640(64, 20, 12, 12);
}

void draw_menu(int start_x, int start_y, int w, int h)
{
  // Draw the border:
  int offset = 10;
  tft.drawRoundRect(start_x - offset, start_y - offset, (2 * offset) + (w * 16), (2 * offset) + (h * 12), 10, TFT_WHITE);
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

void get_and_display_data_from_MLX90640(int start_x, int start_y, int w, int h)
{
  // Draw the options menu:
  draw_menu(start_x, start_y, w, h); // 64, 20, 6, 6

  // Elicit the 32x24 pixel IR thermal imaging array generated by the MLX90640 Thermal Imaging Camera:
  long startTime = millis();
  for (byte x = 0; x < 2; x++)
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);

    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

    float tr = Ta - TA_SHIFT; //Reflected temperature based on the sensor ambient temperature
    float emissivity = 0.95;

    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
  }
  long stopTime = millis();

  int x = start_x; // start_x = 64
  int y = start_y + (h * 11); // start_y = 20 h = 6
  uint32_t c = TFT_BLUE;

  for (int i = 0; i < 768; i++)
  {
    // Print arrary values
    Serial.print(mlx90640To[x], 2);
    Serial.print(",");
    
    // Convert the 16x12 pixel imaging array to string (MLX90640_data) so as to transfer it to the web application:
    // MLX90640_data += String(mlx90640To[i]);
    // if (i != 727)
    //     MLX90640_data += ",";

    // Display a simple image version of the collected data (array) on the screen:
    // Define the color palette:
    c = GetColor(mlx90640To[i]);
    // Draw image pixels (rectangles):
    tft.fillRect(x, y, 6, 6, c);
    x = x + 6;
    // Start a new row: 32x24
//    int l = i + 1;
//    if (l % 32 == 0)
//    {
//      x = start_x;
//      y = y - h;
//    }
  }
  Serial.println();
  delay(5000);
}

boolean isConnected()
{
  // Return true if the MLX90640 Thermal Imaging Camera is detected on the I2C bus:
  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0)
  {
    return (false); // Sensor did not ACK
  }
  return (true);
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

void Getabcd()
{
  // Get the cutoff points based on the given maximum and minimum temperature values.
  a = MinTemp + (MaxTemp - MinTemp) * 0.2121;
  b = MinTemp + (MaxTemp - MinTemp) * 0.3182;
  c = MinTemp + (MaxTemp - MinTemp) * 0.4242;
  d = MinTemp + (MaxTemp - MinTemp) * 0.8182;
}
