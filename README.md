# **Charge Counter** ⚡️

This C code provides a simple **charge counter** for tracking the **State of Charge (SoC)** and remaining capacity of a battery. It's designed to be lightweight and suitable for embedded systems.

-----

## **How it Works**

The library integrates **voltage** and **current** measurements over time to calculate the energy consumed or replenished in a battery. It uses an internal accumulator, `_cap_counts`, to track the energy change.

A key feature is its ability to automatically recalibrate to **100% SoC** when the battery voltage stays above a specified `_full_cap_voltage_V` for more than five seconds, ensuring accuracy and preventing drift.

-----

## **Configuration**

To use the charge counter, you must initialize the `chgc` struct and configure the following parameters:

  * `chgc_set_multiplier()`: Use this to scale down your input voltage and current values if you're using fractional or high-precision values. The internal calculations handle this to maintain accuracy. **MANDATORY** (Before everything else)
  * `chgc_set_update_interval_ms()`: Define how often the accumulator is updated, in milliseconds. **MANDATORY** (Before everything else).
  * `chgc_set_full_cap_wh()`: Set the battery's full capacity in **watt-hours (Wh)**.
  * `chgc_set_full_cap_voltage_V()`: Specify the voltage that indicates a **full charge**.
  * `chgc_set_initial_cap_kwh()`: Set initial, remaining capacity. Good for retrieving values saved in flash memory.

-----

## **Usage Example**

```c
#include "charge_counter.h"
#include <stdio.h>

int main() {
	struct chgc my_battery;

	/* Initialize the struct */
	chgc_init(&my_battery);

	/* Set battery specifications */
	chgc_set_multiplier(&my_battery, 2); /* 2x multiplier for precision */
	chgc_set_update_interval_ms(&my_battery, 100); /* Update every 100ms */
	chgc_set_full_cap_kwh(&my_battery, 2.5f); /* 2.5 kWh capacity */
	chgc_set_full_cap_voltage_V(&my_battery, 26); /* 13V (with 2x multiplier) */
	chgc_set_initial_cap_kwh(&my_battery, 0.0f); /* Initial capacity set to 0 */

	/* Main loop */
	while (1) {
		/* Read raw sensor values (e.g., from an ADC) */
		int16_t current_reading = read_current_sensor(); /* in A * 2 */
		int16_t voltage_reading = read_voltage_sensor(); /* in V * 2 */

		/* Set the runtime values */
		chgc_set_current_A(&my_battery, current_reading);
		chgc_set_voltage_V(&my_battery, voltage_reading);
        
		/* Update the counter (use system timer delta) */
		chgc_update(&my_battery, sys_timer_get_delta());

		/* Get the current state */
		float soc = chgc_get_soc_pct(&my_battery);
		float remain_cap = chgc_get_remain_cap_kwh(&my_battery);

		printf("SoC: %.2f%%, Remaining Capacity: %.2f kWh\n", soc, remain_cap);
	}
    
	return 0;
}

```
