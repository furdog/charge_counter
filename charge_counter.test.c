#include "charge_counter.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

bool cmp_floats_with_epsilon(float a, float b, float e)
{
	return fabs(a - b) <= e;
}

#define CHGC_MULTIPLIER 2
#define CHGC_MULTIPLIER_TOTAL (CHGC_MULTIPLIER * CHGC_MULTIPLIER)

void chgc_test()
{
	float capacity = 7.0f;
	uint32_t update_interval_ms = 10U;
	uint32_t counts_per_hour = (1000U / update_interval_ms) * 60U * 60U;
	uint32_t ms_per_hour = 1000 * 60U * 60U;

	/* Calculate capacity counts */
	int64_t capacity_counts = (int64_t)(capacity * 1000.f) *
				       counts_per_hour * CHGC_MULTIPLIER_TOTAL;

	uint64_t i;
	
	struct chgc chgc;

	chgc_init(&chgc);
	chgc_set_multiplier         (&chgc, CHGC_MULTIPLIER);
	chgc_set_update_interval_ms (&chgc, 10U);
	chgc_set_full_cap_kwh       (&chgc, capacity);
	chgc_set_full_cap_voltage_V (&chgc, 400.0f);
	chgc_set_initial_cap_kwh    (&chgc, 0.0f);

	assert(cmp_floats_with_epsilon(
		chgc_get_remain_cap_kwh(&chgc), 0.0, 0.001f));

	/* Put 350 volts and 5 amps during 1 hour */
	for (i = 0; i < ms_per_hour; i++) {
		chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
		chgc_set_current_A(&chgc, 5   * CHGC_MULTIPLIER);
		chgc_update(&chgc, 1);
	}

	/* printf("chgc._cap_counts: %lli\n", chgc._cap_counts);
	 */
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));
	assert(cmp_floats_with_epsilon(
		chgc_get_remain_cap_kwh(&chgc), 1.750, 0.1));

	printf("chgc_get_soc_pct(&chgc): %f\n", chgc_get_soc_pct(&chgc));
	assert(cmp_floats_with_epsilon(chgc_get_soc_pct(&chgc), 25.0, 0.1));

	/* Put 350 volts and 20 amps during 1 hour (minus one count) */
	chgc_set_initial_cap_kwh(&chgc, 0.0f);
	for (i = 0; i < ms_per_hour - update_interval_ms; i++) {
		chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
		chgc_set_current_A(&chgc, 20  * CHGC_MULTIPLIER);
		chgc_update(&chgc, 1);
	}
	/* printf("chgc._cap_counts: %lli\n", chgc._cap_counts);
	 */
	/* Check edge case. Must be less than capacity */
	assert(chgc._cap_counts < capacity_counts);
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));

	/* Add one more count and check edge case 
	 * Must be equal to capacity */	
	chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
	chgc_set_current_A(&chgc, 20  * CHGC_MULTIPLIER);
	chgc_update(&chgc, update_interval_ms);
	assert(chgc._cap_counts == capacity_counts);
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));

	/* Add one more count and check edge case 
	 * Should not exceed capacity */
	chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
	chgc_set_current_A(&chgc, 20  * CHGC_MULTIPLIER);
	chgc_update(&chgc, update_interval_ms);
	assert(chgc._cap_counts == capacity_counts);
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));

	printf("chgc_get_soc_pct(&chgc): %f\n", chgc_get_soc_pct(&chgc));
	assert(cmp_floats_with_epsilon(chgc_get_soc_pct(&chgc), 100.0, 0.1));

	/* Put 350 volts and -20 amps during 1 hour (minus one count) */
	for (i = 0; i < ms_per_hour - update_interval_ms; i++) {
		chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
		chgc_set_current_A(&chgc, -20 * CHGC_MULTIPLIER);
		chgc_update(&chgc, 1);
	}
	/* printf("chgc._cap_counts: %lli\n", chgc._cap_counts); */
	/* Check edge case. Must be more than 0.0 kwh */
	assert(chgc._cap_counts > 0.0);
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));

	/* Add one more count and check edge case 
	 * Must be equal to 0.0kwh */	
	chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
	chgc_set_current_A(&chgc, -20 * CHGC_MULTIPLIER);
	chgc_update(&chgc, update_interval_ms);
	assert(chgc._cap_counts == 0.0);
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));

	/* Add one more count and check edge case 
	 * Should not go below 0.0kwh */
	chgc_set_voltage_V(&chgc, 350 * CHGC_MULTIPLIER);
	chgc_set_current_A(&chgc, -20 * CHGC_MULTIPLIER);
	chgc_update(&chgc, update_interval_ms);
	assert(chgc._cap_counts == 0.0);
	printf("chgc_get_remain_cap_kwh(&chgc): %f\n",
		chgc_get_remain_cap_kwh(&chgc));

	printf("chgc_get_soc_pct(&chgc): %f\n", chgc_get_soc_pct(&chgc));
	assert(cmp_floats_with_epsilon(chgc_get_soc_pct(&chgc), 0.0, 0.1));

	/* Check if 100% voltage sets energy too capacity */
	chgc_set_voltage_V(&chgc, 400 * CHGC_MULTIPLIER);
	chgc_set_current_A(&chgc, 1   * CHGC_MULTIPLIER);
	chgc_update(&chgc, update_interval_ms);
	/* LEGACY: assert(chgc._cap_counts == capacity_counts);
	 * Now we have to wait at least 5 seconds for get to 100% */
	for (i = 0; i < 4999 - update_interval_ms; i++) {
		chgc_update(&chgc, 1);
	}
	assert(chgc._cap_counts != capacity_counts);
	chgc_update(&chgc, 1);
	assert(chgc._cap_counts == capacity_counts);

	printf("chgc_get_soc_pct(&chgc): %f\n", chgc_get_soc_pct(&chgc));
	assert(cmp_floats_with_epsilon(chgc_get_soc_pct(&chgc), 100.0, 0.1));
}

int main()
{
	chgc_test();
	return 0;
}
