typedef enum
{
  EIML_OK = 0,
  EIML_ERROR = 1
} eiml_ret;

/*******************************************************************************
 * Functions
 */

// Function: crop an image, store in another buffer
eiml_ret eiml_crop_center(const unsigned char *in_pixels, 
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

  // Copy pixels (in center of input image) to new buffer
  unsigned int out_buf_len = out_width * out_height;
  for (unsigned int y = 0; y < out_height; y++)
  {
    in_y_offset = bytes_per_pixel * in_width * 
                  ((in_height - out_height) / 2 + y);
    out_y_offset = bytes_per_pixel * out_width * y;
    for (unsigned int x = 0; x < out_width; x++) 
    {
      in_x_offset = bytes_per_pixel * ((in_width - out_width) / 2 + x);
      out_x_offset = bytes_per_pixel * x;
      for (unsigned int b = 0; b < bytes_per_pixel; b++)
      {
//        Serial.print("in_x: ");
//        Serial.println(in_x);
//        Serial.print("in_y: ");
//        Serial.println(in_y);
//        Serial.print("out_x: ");
//        Serial.println(x);
//        Serial.print("out_y: ");
//        Serial.println(y);
//        Serial.print("b: ");
//        Serial.println(b);
//        Serial.print("pix: 0x");
//        Serial.println(in_pixels[(bytes_per_pixel * in_width * in_y) + (bytes_per_pixel * in_x) + b], HEX);
        out_pixels[out_y_offset + out_x_offset + b] = 
                                  in_pixels[in_y_offset + in_x_offset + b];
      }
    }
  }

  return EIML_OK;
}

void setup() {
  // Wait for serial to connect
  Serial.begin(115200);
  while (!Serial);
}

void loop() {

  int in_w = 3;
  int in_h = 4;
  int b_per_pix = 2;
  unsigned char a[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
                        0x12, 0x13, 0x14, 0x15, 0x16, 0x17
  };

  int out_w = 2;
  int out_h = 2;
  unsigned char b[out_w * out_h * b_per_pix];

  eiml_crop_center(a, in_w, in_h, b, out_w, out_h, b_per_pix);

  for (int i = 0; i < out_w * out_h * b_per_pix; i++) {
    Serial.print("0x");
    Serial.print(b[i]);
    Serial.print(", ");
  }
  Serial.println();

  delay(1000);
}
