#include <chimaera.h>

uint8_t buf_o_ptr = 0;
uint8_t buf_o[2][CHIMAERA_BUFSIZE] __attribute__((aligned(4))); // general purpose output buffer //TODO how big?
uint8_t buf_i[2][CHIMAERA_BUFSIZE] __attribute__((aligned(4))); // general purpose input buffer //TODO how big?
// the buffers should be aligned to 32bit, as most we write to it is a multiple of 32bit (OSC, SNTP, DHCP, ARP, etc.)

uint8_t calibrating = 0;
