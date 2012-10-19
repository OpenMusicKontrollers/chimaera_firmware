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

	uint16_t thresh0 [2];
};

extern uint16_t f_thresh1;
extern fix_0_16_t m_thresh1;

extern ADC_Range adc_range[MUX_MAX][ADC_LENGTH];
