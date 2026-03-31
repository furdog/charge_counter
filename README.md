# **Charge Counter** ⚡️

Hardware-agnostic C89 library for tracking battery State of Charge (SoC) via Riemann sum integration of power (Coulomb counting variant).

## **Core Logic**
The library accumulates energy counts by integrating reported voltage and current over a defined time interval. 
* **Integration:** `_energy_accum += voltage * current` performed every `update_interval_ms`.
* **Recalibration:** Forced synchronization to 100% SoC occurs if `_max_energy_trigger` is high for $\ge 5000\text{ms}$.
* **Safety:** Hard clamping of `_energy_accum` between $0$ and `max_energy_wh`.
* **Data Integrity:** Updates halt if `report_timeout_ms` is exceeded without a fresh call to `chgc_set_voltage_V` or `chgc_set_current_A`.

## **API Reference**

### **Initialization & Configuration**
1.  **`chgc_init(struct chgc *self)`**: Zeroes all internal timers and accumulators.
2.  **`chgc_set_config(struct chgc *self, const struct chgc_config cfg)`**: Sets operational parameters. Only permitted once (fails if `max_energy_wh > 0`).

| Parameter | Type | Description |
| :--- | :--- | :--- |
| `update_interval_ms` | `uint32_t` | Frequency of the integration step. |
| `report_timeout_ms` | `uint32_t` | Max allowed age of sensor data before integration halts. |
| `max_energy_wh` | `uint32_t` | Battery capacity in Watt-hours. |
| `multiplier_V` | `uint8_t` | Scaling factor for voltage input (e.g., 2 for 0.5V units). |
| `multiplier_A` | `uint8_t` | Scaling factor for current input. |

### **Runtime Updates**
* **`chgc_set_voltage_V(self, val)`**: Ingests raw scaled voltage; resets report timer.
* **`chgc_set_current_A(self, val)`**: Ingests raw scaled current; resets report timer.
* **`chgc_update(self, delta_ms)`**: Increments internal timers. Executes integration if `update_interval_ms` is reached.

### **Data Retrieval**
* **`chgc_get_soc_pct(self)`**: Returns integer percentage (0–100).
* **`chgc_get_energy_wh(self)`**: Returns remaining energy in Watt-hours.

---

## **Implementation Example**

```c
/* Global */
struct chgc battery;
struct chgc_config cfg;

...

/* Setup */
	cfg.max_energy_wh      = 24000;
	cfg.update_interval_ms = 10u;
	cfg.multiplier_V       = 2u;
	cfg.multiplier_A       = 2u;
	cfg.report_timeout_ms  = 500u;

	chgc_init(&battery);
	chgc_set_config(&battery, cfg);

...

/* Loop (assumes 10ms delta (update interval)) */
	chgc_set_voltage_V(&battery, read_adc_v());
	chgc_set_current_A(&battery, read_adc_a());
	chgc_update(&battery, 10); 
```

Coverage report is available at: https://furdog.github.io/charge_counter/coverage/
