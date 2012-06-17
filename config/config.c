#include <config.h>

#include <string.h>

const uint8_t  _comm_mac [] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
const uint8_t  _comm_ip [] = {192, 168, 1, 177};
const uint8_t  _comm_gateway [] = {192, 168, 1, 1};
const uint8_t  _comm_subnet [] = {255, 255, 255, 0};

const uint8_t  _comm_tuio_sock = 0;
const uint16_t _comm_tuio_port = 3333;

const uint8_t  _comm_config_sock = 1;
const uint16_t _comm_config_port = 4444;

const uint8_t  _comm_remote_ip [] = {192, 168, 1, 255}; // send via local broadcast

const uint16_t _cmc_diff = 0;
const uint16_t _cmc_thresh0 = 60;
const uint16_t _cmc_thresh1 = 120;

void 
config_init (Config *config)
{
	memcpy (config->comm.mac, _comm_mac, 6);
	memcpy (config->comm.ip, _comm_ip, 4);
	memcpy (config->comm.gateway, _comm_gateway, 4);
	memcpy (config->comm.subnet, _comm_subnet, 4);

	memcpy (&config->comm.tuio_sock, &_comm_tuio_sock, 1);
	memcpy (&config->comm.tuio_port, &_comm_tuio_port, 2);

	memcpy (&config->comm.config_sock, &_comm_config_sock, 1);
	memcpy (&config->comm.config_port, &_comm_config_port, 2);

	memcpy (config->comm.remote_ip, _comm_remote_ip, 4);

	memcpy (&config->cmc.diff, &_cmc_diff, 2);
	memcpy (&config->cmc.thresh0, &_cmc_thresh0, 2);
	memcpy (&config->cmc.thresh1, &_cmc_thresh1, 2);
}
