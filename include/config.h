#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct _Config Config;

struct _Config {
	struct _comm {
		uint8_t mac [6];
		uint8_t ip [4];
		uint8_t gateway [4];
		uint8_t subnet [4];

		uint8_t tuio_sock;
		uint16_t tuio_port;

		uint8_t config_sock;
		uint16_t config_port;

		uint8_t remote_ip [4];
	} comm;

	struct _cmc {
		uint16_t diff;
		uint16_t thresh0;
		uint16_t thresh1;
	} cmc;
};

void config_init (Config *config);

#endif // CONFIG_hH
