typedef enum
{
  EIML_OK = 0,
  EIML_ERROR = 1
} eiml_ret;

/*******************************************************************************
 * Functions
 */

// Function: Convert RGB565 to RGB888
eiml_ret eiml_rgb565_to_rgb888( const unsigned char *in_pixels,
                                unsigned char *out_pixels,
                                unsigned int num_pixels)
{
  unsigned char r;
  unsigned char g;
  unsigned char b;
  
  for (unsigned int i = 0; i < num_pixels; i++)
  {
    r = in_pixels[2 * i] & 0xF8;
    g = (in_pixels[2 * i] << 5) | (in_pixels[(2 * i) + 1] & 0xE0);
    b = in_pixels[(2 * i) + 1] << 3;
//    Serial.print("0x");
//    Serial.print(in_pixels[2 * i], HEX);
//    Serial.print(in_pixels[(2 * i) + 1], HEX);
//    Serial.print(" -> 0x");
//    Serial.print(r, HEX);
//    Serial.print(g, HEX);
//    Serial.println(b, HEX);
    out_pixels[3 * i] = r;
    out_pixels[(3 * i) + 1] = g;
    out_pixels[(3 * i) + 2] = b;
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
  int in_b_per_pix = 2;
  unsigned char a[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
                        0x12, 0x13, 0x14, 0x15, 0x16, 0x17
  };

  int out_w = 3;
  int out_h = 4;
  int out_b_per_pix = 3;
  unsigned char b[out_w * out_h * out_b_per_pix];

  eiml_rgb565_to_rgb888(a, b, in_w * in_h);

  for (int i = 0; i < in_w * in_h * in_b_per_pix; i++) {
    Serial.print("0x");
    Serial.print(a[i], HEX);
    Serial.print(", ");
  }
  Serial.println();


  for (int i = 0; i < out_w * out_h * out_b_per_pix; i++) {
    Serial.print("0x");
    Serial.print(b[i], HEX);
    Serial.print(", ");
  }
  Serial.println();

  delay(1000);
}
