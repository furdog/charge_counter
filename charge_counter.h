/**
 * @file charge_counter.h
 * @brief Charge counter (Hardware-Agnostic)
 *
 * This file contains the software implementation of the charge counter logic.
 * The design is hardware-agnostic, requiring an external adaptation layer
 * for hardware interaction.
 *
 * **Conventions:**
 * C89, Linux kernel style, MISRA, rule of 10, No hardware specific code,
 * only generic C and some binding layer. Be extra specific about types.
 *
 * Scientific units where posible at end of the names, for example:
 * - timer_10s (timer_10s has a resolution of 10s per bit)
 * - power_150w (power 150W per bit or 0.15kw per bit)
 *
 * Keep variables without units if they're unknown or not specified or hard
 * to define with short notation.
 *
 * ```LICENSE
 * Copyright (c) 2025 furdog <https://github.com/furdog>
 *
 * SPDX-License-Identifier: 0BSD
 * ```
 *
 * Be free, be wise and take care of yourself!
 * With best wishes and respect, furdog
 */

#ifndef CHARGE_COUNTER_HEADER_GUARD
#define CHARGE_COUNTER_HEADER_GUARD

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/******************************************************************************
 * CHARGE COUNTER
 *****************************************************************************/
/** Config is set after init, before any other operations */
struct chgc_config {
	/** How often voltage/current reports are expected? */
	uint32_t update_interval_ms;

	/** How long we allowed to wait voltage/current report? */
	uint32_t report_timeout_ms;

	uint32_t max_energy_wh; /**< Max energy (watt/hours) */

	uint8_t multiplier_V; /**< Reported voltage multiplier */
	uint8_t multiplier_A; /**< Reported current multiplier */
};

/** Main instance */
struct chgc {
	struct chgc_config _config;

	/** Time integral of power (Riemann Sum) */
	int64_t _energy_accum;

	uint32_t _report_timer_ms; /**< Used to calculate report timeout */
	uint32_t _update_timer_ms; /**< Used to hold update integral */

	/** If _max_energy_trigger is true, this timer will increase.
	 *  It is compared with debounce time */
	uint32_t _max_energy_trigger_timer_ms;

	uint16_t _voltage_V; /**< Reported voltage */
	int16_t	 _current_A; /**< Reported current */

	/** If true for certain period of time (debounce)
	 *  _energy_accum will be set to its maximum value */
	bool _max_energy_trigger;
};

/*--------------------------------------------------------------- PUBLIC API */
/** Initializes charge counter structure with default values */
void chgc_init(struct chgc *self);

/** Set config after initialization */
bool chgc_set_config(struct chgc *self, const struct chgc_config cfg);

/** Get config */
struct chgc_config chgc_get_config(const struct chgc *self);

/** Set remaining energy */
void chgc_set_energy_wh(struct chgc *self, const uint32_t val);

/** Get remaining energy */
uint32_t chgc_get_energy_wh(const struct chgc *self);

/** Get max energy, that can be stored */
uint32_t chgc_get_max_energy_wh(const struct chgc *self);

/** Returns State of Charge in range 0 - 100 */
uint8_t chgc_get_soc_pct(const struct chgc *self);

/** Updates voltage reading */
void chgc_set_voltage_V(struct chgc *self, const uint32_t val_V);

/** Updates current reading */
void chgc_set_current_A(struct chgc *self, const int32_t val_A);

/** If set to true for long enough, remaining energy may be dropped to max
 * energy */
void chgc_trigger_max_energy(struct chgc *self, const bool val);

/** Main update loop for the charge counter */
void chgc_update(struct chgc *self, const uint32_t delta_time_ms);
/*---------------------------------------------------------------------------*/

#ifndef CHARGE_COUNTER_IMPLEMENTATION
static int64_t _chgc_get_multiplier_total(const struct chgc *self)
{
	return (self->_config.multiplier_V * self->_config.multiplier_A);
}

static int64_t _chgc_get_counts_per_hour(const struct chgc *self)
{
	uint32_t update_interval = self->_config.update_interval_ms;

	if (update_interval == 0u) {
		/* Default to 1ms to prevent division by zero */
		update_interval = 1u;
	}

	return ((1000u / update_interval) * 60u * 60u);
}

static int64_t _chgc_conv_wh_to_counts(struct chgc *self, const int64_t val)
{
	return val * _chgc_get_counts_per_hour(self) *
	       _chgc_get_multiplier_total(self);
}

void _chgc_recalc_energy(struct chgc *self)
{
	/* Calculate capacity counts */
	int64_t full_energy_accum =
	    _chgc_conv_wh_to_counts(self, self->_config.max_energy_wh);

	self->_energy_accum +=
	    (int64_t)self->_voltage_V * (int64_t)self->_current_A;

	/* If trigger is active - count its duration */
	if (self->_max_energy_trigger) {
		self->_max_energy_trigger_timer_ms +=
		    self->_config.update_interval_ms;
	} else {
		self->_max_energy_trigger_timer_ms = 0u;
	}

	/* If trigger is active for 5 seconds - set capacity too 100% */
	if (self->_max_energy_trigger_timer_ms >= 5000u) {
		self->_max_energy_trigger_timer_ms = 0u;

		self->_energy_accum = full_energy_accum;
	}

	/* Accumulated capacity should not exceed battery capacity
	 * nor go below negative capacity */
	if (self->_energy_accum > full_energy_accum) {
		self->_energy_accum = full_energy_accum;
	} else if (self->_energy_accum < 0) {
		self->_energy_accum = 0;
	} else {
	}
}

void chgc_init(struct chgc *self)
{
	assert(self);

	/* Config */
	self->_config.update_interval_ms = 0u;

	self->_config.report_timeout_ms = 0u;

	self->_config.max_energy_wh = 0u;

	self->_config.multiplier_V = 1u;
	self->_config.multiplier_A = 1u;

	/* Runtime */
	self->_energy_accum = 0u;

	self->_report_timer_ms = 0u;
	self->_update_timer_ms = 0u;

	self->_max_energy_trigger_timer_ms = 0u;

	self->_voltage_V = 0u;
	self->_current_A = 0u;

	self->_max_energy_trigger = 0u;
}

bool chgc_set_config(struct chgc *self, const struct chgc_config cfg)
{
	bool success = false;

	assert(self);

	/* If max energy > 0u, the config is considered to already be set */
	if (self->_config.max_energy_wh == 0u) {
		self->_config = cfg;
		success	      = true;
	}

	return success;
}

struct chgc_config chgc_get_config(const struct chgc *self)
{
	assert(self);

	return self->_config;
}

void chgc_set_energy_wh(struct chgc *self, const uint32_t val)
{
	assert(self);

	self->_energy_accum = (int64_t)val * _chgc_get_counts_per_hour(self) *
			      _chgc_get_multiplier_total(self);
}

uint32_t chgc_get_energy_wh(const struct chgc *self)
{
	int64_t counts_per_h = _chgc_get_counts_per_hour(self);
	int64_t mul_total    = _chgc_get_multiplier_total(self);

	int64_t result = 0;

	assert(self);

	/* Division by zero */
	if ((counts_per_h == 0) || (mul_total == 0)) {
		result = 0;
	} else {
		/* Divide accumulated energy to update intervals per hour
		 * Also divide by squared multiplier, since _energy_accum is a
		 * product of both scaled voltage and current */
		result = (self->_energy_accum / counts_per_h) / mul_total;
	}

	return result;
}

uint32_t chgc_get_max_energy_wh(const struct chgc *self)
{
	assert(self);

	return self->_config.max_energy_wh;
}

uint8_t chgc_get_soc_pct(const struct chgc *self)
{
	uint8_t result = 0u;

	assert(self);

	if (self->_config.max_energy_wh > 0u) {
		result = chgc_get_energy_wh(self) * 100u /
			 chgc_get_max_energy_wh(self);
	}

	return result;
}

void chgc_set_voltage_V(struct chgc *self, const uint32_t val)
{
	assert(self);

	self->_voltage_V       = val;
	self->_report_timer_ms = 0u;
}

void chgc_set_current_A(struct chgc *self, const int32_t val)
{
	assert(self);

	self->_current_A       = val;
	self->_report_timer_ms = 0u;
}

void chgc_trigger_max_energy(struct chgc *self, const bool val)
{
	assert(self);

	self->_max_energy_trigger = val;
}

void chgc_update(struct chgc *self, const uint32_t delta_time_ms)
{
	assert(self);

	self->_update_timer_ms += delta_time_ms;

	/* Update charge counter with strictly specified interval
	 * Regardless of chgc_update(...) rate */
	if (self->_update_timer_ms >= self->_config.update_interval_ms) {
		self->_update_timer_ms -= self->_config.update_interval_ms;

		self->_report_timer_ms += self->_config.update_interval_ms;

		/* Only recalculate capacity if values were reported in time */
		if (self->_report_timer_ms < self->_config.report_timeout_ms) {
			_chgc_recalc_energy(self);
		}
	}
}
#endif /* CHARGE_COUNTER_IMPLEMENTATION */

#endif /* CHARGE_COUNTER_HEADER_GUARD */
