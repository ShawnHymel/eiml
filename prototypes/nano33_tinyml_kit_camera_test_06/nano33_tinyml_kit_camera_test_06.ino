#include <TinyMLShield.h>
#include "base64.h"             // Used to convert data to Base64 encoding

// Preprocessor settings
#define BAUD_RATE         230400          // Must match receiver application
#define SEND_IMG          1               // Transmit raw RGB888 image over serial

// Camera settings: https://github.com/tinyMLx/arduino-library/blob/main/src/OV767X_TinyMLx.h
static const int cam_type = OV7675;       // OV7675
static const int cam_resolution = QCIF;   // QQVGA, QCIF, QVGA, CIF, VGA
static const int cam_format = RGB565;     // YUV422, RGB444, RGB565, GRAYSCALE
static const int cam_fps = 1;             // 1, 5, 10, 15, 30
static const int cam_width = 176;         // Width of resolution
static const int cam_height = 144;        // Height of resolution
static const int cam_bytes_per_pixel = 2;

// Other image settings
static const int scale_width = 176;
static const int scale_height = 64;
static const int crop_width = 64;
static const int crop_height = 64;
static const int rgb888_bytes_per_pixel = 3;

// EIML constants
static const char EIML_HEADER_SIZE = 12;
static const char EIML_SOF_SIZE = 3;
static const char EIML_SOF[] = {0xFF, 0xA0, 0xFF};

// EIML formats
typedef enum
{
  EIML_RESERVED = 0,
  EIML_GRAYSCALE = 1,
  EIML_RGB888 = 2
} EimlFormat;

// Return codes for image manipulation
typedef enum
{
  EIML_OK = 0,
  EIML_ERROR = 1
} EimlRet;

// Transmission header for raw image (must convert nicely to base64)
// |     SOF     |  format  |   width   |   height  |
// | xFF xA0 XFF | [1 byte] | [4 bytes] | [4 bytes] |
typedef struct EimlHeader
{
  uint8_t format;
  uint32_t width;
  uint32_t height;
} EimlHeader;

// Global variables
static int cam_bytes_per_frame;

/*******************************************************************************
 * Functions
 */

// Function: crop an image, store in another buffer
EimlRet eiml_crop_center(const unsigned char *in_pixels, 
                          unsigned int in_width, 
                          unsigned int in_height,
                          unsigned char *out_pixels,
                          unsigned int out_width,
                          unsigned int out_height,
                          unsigned int bytes_per_pixel)
{
  unsigned int in_x_offset;
  unsigned int in_y_offset;
  unsigned int out_x_offset;
  unsigned int out_y_offset;

  // Verify crop is smaller
  if ((in_width < out_width) || (in_height < out_height))
  {
    return EIML_ERROR;
  }

  // Calculate size of output image
  unsigned int out_buf_len = out_width * out_height;

  // Go through each row
  for (unsigned int y = 0; y < out_height; y++)
  {
    in_y_offset = bytes_per_pixel * in_width * 
                  ((in_height - out_height) / 2 + y);
    out_y_offset = bytes_per_pixel * out_width * y;

    // Go through each pixel in each row
    for (unsigned int x = 0; x < out_width; x++) 
    {
      in_x_offset = bytes_per_pixel * ((in_width - out_width) / 2 + x);
      out_x_offset = bytes_per_pixel * x;

      // go through each byte in each pixel
      for (unsigned int b = 0; b < bytes_per_pixel; b++)
      {
        out_pixels[out_y_offset + out_x_offset + b] = 
                                  in_pixels[in_y_offset + in_x_offset + b];
      }
    }
  }

  return EIML_OK;
}

// Function: scale image using nearest neighber
EimlRet eiml_scale( const unsigned char *in_pixels, 
                    unsigned int in_width, 
                    unsigned int in_height,
                    unsigned char *out_pixels,
                    unsigned int out_width,
                    unsigned int out_height,
                    unsigned int bytes_per_pixel)
{
  unsigned int in_x_offset;
  unsigned int in_y_offset;
  unsigned int out_x_offset;
  unsigned int out_y_offset;
  unsigned int src_x;
  unsigned int src_y;

  // Compute ratio between input and output widths/heights (fixed point)
  unsigned long ratio_x = (in_width << 16) / out_width;
  unsigned long ratio_y = (in_height << 16) / out_height;
  
  // Loop through each row
  for (unsigned int y = 0; y < out_height; y++)
  {
    // Find which pixel to sample from original image in y direction
    src_y = (y * ratio_y) >> 16;
    src_y = (src_y < in_height) ? src_y : in_height - 1;
  
    // Calculate buffer offsets for y
    in_y_offset = bytes_per_pixel * in_width * src_y;
    out_y_offset = bytes_per_pixel * out_width * y;

    // Go through each pixel in each row
    for (unsigned int x = 0; x < out_width; x++) 
    {
      // Find which pixel to sample from original image in x direction 
      src_x = int(x * ratio_x) >> 16;
      src_x = (src_x < in_width) ? src_x : in_width;
  
      // Calculate buffer offsets for x
      in_x_offset = bytes_per_pixel * src_x;
      out_x_offset = bytes_per_pixel * x;
        
      // Copy pixels from source image to destination
      for (unsigned int b = 0; b < bytes_per_pixel; b++)
      {
        out_pixels[out_y_offset + out_x_offset + b] = 
                  in_pixels[in_y_offset + in_x_offset + b];
      }
    }
  }

  return EIML_OK;
}

// Function: Convert RGB565 to RGB888
EimlRet eiml_rgb565_to_rgb888( const unsigned char *in_pixels,
                                unsigned char *out_pixels,
                                unsigned int num_pixels)
{
  unsigned char r;
  unsigned char g;
  unsigned char b;

  // Go through each pixel
  for (unsigned int i = 0; i < num_pixels; i++)
  {
    // Get RGB values
    r = in_pixels[2 * i] & 0xF8;
    g = (in_pixels[2 * i] << 5) | 
        ((in_pixels[(2 * i) + 1] & 0xE0) >> 3);
    b = in_pixels[(2 * i) + 1] << 3;

    // Copy RGB values to new buffer
    out_pixels[3 * i] = r;
    out_pixels[(3 * i) + 1] = g;
    out_pixels[(3 * i) + 2] = b;
  }

  return EIML_OK;
}

// Function: generate header
EimlRet eiml_generate_header(EimlHeader header, unsigned char *out_header)
{
  // Copy SOF
  for (int i = 0; i < EIML_SOF_SIZE; i++)
  {
    out_header[i] = EIML_SOF[i];
  }

 // Copy format
 out_header[EIML_SOF_SIZE] = header.format;

 // Copy width and height (keep little endianness)
 for (int i = 0; i < 4; i++)
 {
  out_header[EIML_SOF_SIZE + 1 + i] = (header.width >> (i * 8)) & 0xFF;
 }
 for (int i = 0; i < 4; i++)
 {
  out_header[EIML_SOF_SIZE + 5 + i] = (header.height >> (i * 8)) & 0xFF;
 }

 return EIML_OK;
}

/*******************************************************************************
 * Main
 */

void setup() {

  // Wait for serial to connect
  Serial.begin(BAUD_RATE);
  while (!Serial);

  // Initialize TinyML shield
  initializeShield();

  // Initialize the OV7675 camera
  if (!Camera.begin(cam_resolution, cam_format, cam_fps, cam_type)) {
    Serial.println("Failed to initialize camera");
    while (1);
  }
  cam_bytes_per_frame = Camera.width() * Camera.height() * Camera.bytesPerPixel();
}

void loop() {

  static EimlRet eiml_ret;

  // Crop the image as a square in the center of the frame
  static int scale_img_bytes = scale_width * scale_height * cam_bytes_per_pixel;
  static int crop_img_bytes = crop_width * crop_height * cam_bytes_per_pixel;
  static int rgb888_img_bytes = scale_width * scale_height * rgb888_bytes_per_pixel;

  // Create capture buffer
  uint8_t *cam_img;
  cam_img = (uint8_t *)malloc(cam_width * cam_height * cam_bytes_per_pixel * sizeof(char));
  if (!cam_img) {
    Serial.println("Could not allocate camera buffer");
    return;
  }

  // Create scale buffer
  uint8_t *scale_img;
  scale_img = (uint8_t *)malloc(scale_width * scale_height * cam_bytes_per_pixel * sizeof(char));
  if (!scale_img) {
    Serial.println("Could not allocate scale buffer");
    return;
  }

  // Capture frame
  Camera.readFrame(cam_img);

  // Scale image (skew to offset OV767X natural skew)
  eiml_ret = eiml_scale(cam_img,
                        cam_width,
                        cam_height,
                        scale_img,
                        scale_width,
                        scale_height,
                        cam_bytes_per_pixel);
  if (eiml_ret != EIML_OK) {
    Serial.println("Image scaling error");
    return;
  }

  // We're done with the raw frame buffer
  free(cam_img);

  // Create crop image buffer
  uint8_t *crop_img;
  crop_img = (uint8_t *)malloc(crop_width * crop_height * cam_bytes_per_pixel * sizeof(char));
  if (!crop_img) {
    Serial.println("Could not allocate crop buffer");
    return;
  }

  // Crop image to square
  eiml_ret = eiml_crop_center(scale_img,
                              scale_width,
                              scale_height,
                              crop_img,
                              crop_width,
                              crop_height,
                              cam_bytes_per_pixel);
  if (eiml_ret != EIML_OK) {
    Serial.println("Image cropping error");
    return;
  }

  // Free scale image buffer
  free(scale_img);

  // Create RGB888 image buffer
  uint8_t *rgb888_img;
  rgb888_img = (uint8_t *)malloc(crop_width * crop_width * rgb888_bytes_per_pixel * sizeof(char));
  if (!rgb888_img) {
    Serial.println("Could not allocate RGB888 buffer");
    return;
  }

  // Convert cropped image to RGB888
  eiml_ret = eiml_rgb565_to_rgb888(crop_img, rgb888_img, crop_width * crop_width);
  if (eiml_ret != EIML_OK) {
    Serial.println("Image conversion error");
    return;
  }

  // Free crop image buffer
  free(crop_img);

  // Create encoded message buffer
  uint32_t enc_len = (rgb888_img_bytes + 2) / 3 * 4;
  unsigned char *enc_buf;
  enc_buf = (unsigned char*)malloc((enc_len + 1) * sizeof(unsigned char));
  if (!enc_buf) {
    Serial.println("Could not allocate encoded message buffer");
    return;
  }

  // Convert scaled RGB888 image to base64
  unsigned int num = encode_base64(rgb888_img, rgb888_img_bytes, enc_buf);

  // Free RGB888 image buffer
  free(rgb888_img);
  
  // Send encoded image out over serial
#if SEND_IMG

  // Construct header
  EimlHeader header;
  header.format = EIML_RGB888;
  header.width = crop_width;
  header.height = crop_width;

  // Generate header
  unsigned char header_buf[EIML_HEADER_SIZE];
  eiml_ret = eiml_generate_header(header, header_buf);
  if (eiml_ret != EIML_OK) {
    Serial.println("Error generating header");
    return;
  }

  // Convert header to base64
  unsigned char enc_header[(EIML_HEADER_SIZE + 2) / 3 * 4];
  num = encode_base64(header_buf, EIML_HEADER_SIZE, enc_header);

  // Print header and image body
  Serial.print((char*)enc_header);
  Serial.println((char*)enc_buf);
#endif

  // Free buffers
  free(enc_buf);
}
