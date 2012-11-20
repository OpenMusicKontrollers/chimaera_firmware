#include <chimaera.h>
#include <config.h>
#include <armfix.h>

typedef struct _ADC_Range ADC_Range;
typedef union _ADC_Union ADC_Union;

union _ADC_Union {
	uint16_t uint;
	fix_s7_8_t fix;
};

struct _ADC_Range {
	uint16_t mean;

	ADC_Union A[2]; // apart from A, here we store b0 while calibrarting
	ADC_Union B[2]; // apart from B, here we store b1 while calibrating
	ADC_Union C[2]; // apart from C, here we store b2 while calibrating

	uint16_t thresh [2]; // apart from thresh, here we store the maximal sensor values while calibrating
};

extern ADC_Range adc_range[MUX_MAX][ADC_LENGTH];
