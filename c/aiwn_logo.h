#pragma once

struct CLogo {
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char pixel_data[128 * 117 * 4 + 1];
};

extern const struct CLogo aiwnios_logo;
