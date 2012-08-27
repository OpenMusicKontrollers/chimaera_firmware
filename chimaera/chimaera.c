#include <chimaera.h>

uint8_t buf_o_ptr = 0;
uint8_t buf_o[2][CHIMAERA_BUFSIZE]; // general purpose output buffer //TODO how big?
uint8_t buf_i[2][CHIMAERA_BUFSIZE]; // general purpose input buffer //TODO how big?

uint8_t calibrating = 0;
