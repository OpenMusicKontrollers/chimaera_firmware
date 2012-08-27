#include <chimaera.h>
#include <config.h>
#include <armfix.h>

typedef struct _ADC_Range ADC_Range;

struct _ADC_Range {
	uint16_t south;
	uint16_t mean;
	uint16_t north;
	fix_8_8_t m_south;
	fix_8_8_t m_north;
};

extern ADC_Range adc_range[MUX_MAX][ADC_LENGTH];
