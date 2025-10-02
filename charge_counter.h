#ifndef   CHARGE_COUNTER_H
#define   CHARGE_COUNTER_H

#include <stdint.h>
#include <assert.h>

/******************************************************************************
 * CLASS
 *****************************************************************************/
struct chgc
{
	/* Config */
	uint32_t _full_cap_wh;
	uint16_t _full_cap_voltage_V;
	uint32_t _update_interval_ms;
	uint8_t  _multiplier;
	
	/* Runtime */
	uint16_t _voltage_V;
	int16_t  _current_A;

	/* Capacity counter (accumulator) */
	int64_t _cap_counts;
	
	int32_t  _update_timer_ms;
	uint32_t _full_cap_voltage_debounce_ms;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
int64_t _chgc_get_multiplier_total(struct chgc *self)
{
	return (self->_multiplier * self->_multiplier);
}

int64_t _chgc_get_counts_per_hour(struct chgc *self)
{
	return ((1000U / self->_update_interval_ms) * 60U * 60U);
}

int64_t _chgc_conv_wh_to_counts(struct chgc *self, int64_t val)
{
	return val * _chgc_get_counts_per_hour(self) *
		_chgc_get_multiplier_total(self);
}

/******************************************************************************
 * INIT
 *****************************************************************************/
void chgc_init(struct chgc *self)
{
	/* Config */
	self->_full_cap_wh        = 0U;
	self->_full_cap_voltage_V = 0U;
	self->_update_interval_ms = 0U;
	self->_multiplier         = 1U; /* 1x is default multiplier */

	/* Runtime */
	self->_voltage_V  = 0U;
	self->_current_A  = 0U;
	self->_cap_counts = 0;
	
	self->_update_timer_ms              = 0;
	self->_full_cap_voltage_debounce_ms = 0;
}

/******************************************************************************
 * CONFIG
 *****************************************************************************/
void chgc_set_full_cap_wh(struct chgc *self, uint32_t val)
{
	self->_full_cap_wh = val;
}

void chgc_set_full_cap_kwh(struct chgc *self, float val)
{
	chgc_set_full_cap_wh(self, val * 1000.0f);
}

uint32_t chgc_get_full_cap_wh(struct chgc *self)
{
	return self->_full_cap_wh;
}

float chgc_get_full_cap_kwh(struct chgc *self)
{
	return chgc_get_full_cap_wh(self) / 1000.0f;
}

/* Multiplier must be set before call */
void chgc_set_initial_cap_kwh(struct chgc *self, float val)
{
	int64_t e = (int64_t)(val * 1000.0f); /* convert to watts */

	/* Setting initial capacity without update interval
	 * is undefined behaviour (TODO define) */
	assert(self->_update_interval_ms != 0U);

	e = e * _chgc_get_counts_per_hour(self);

	self->_cap_counts = e * _chgc_get_multiplier_total(self);
}

void chgc_set_full_cap_voltage_V(struct chgc *self, uint16_t val)
{
	self->_full_cap_voltage_V = val;
}

void chgc_set_update_interval_ms(struct chgc *self, uint32_t val)
{
	/* Changing interval is undefined behaviour (TODO define) */
	assert(self->_cap_counts == 0);

	self->_update_interval_ms = val;
	self->_update_timer_ms    = val;
}

/* Input current and voltage will be divided (without precission loss)
 * by this value */
void chgc_set_multiplier(struct chgc *self, uint8_t val)
{
	self->_multiplier = val;
}

/******************************************************************************
 * RUNTIME
 *****************************************************************************/
/* (1V/multiplier)/bit precision */
void chgc_set_voltage_V(struct chgc *self, int16_t val)
{
	/* TODO set to 0, if not called for too long after certain timeout */
	self->_voltage_V = val;
}

/* (1A/multiplier)/bit precision */
void chgc_set_current_A(struct chgc *self, int16_t val)
{
	/* TODO set to 0, if not called for too long after certain timeout */
	self->_current_A = val;
}

/* 1W/bit precision */
uint32_t chgc_get_remain_cap_wh(struct chgc *self)
{
	/* Divide accumulated energy to update intervals per hour
	 * Also divide by squared multiplier, since _cap_counts is a
	 * product of both scaled voltage and current */
	return (self->_cap_counts / _chgc_get_counts_per_hour(self)) /
		_chgc_get_multiplier_total(self);
}

/* 1W/bit precision */
float chgc_get_remain_cap_kwh(struct chgc *self)
{
	return chgc_get_remain_cap_wh(self) / 1000.0f;
}

float chgc_get_soc_pct(struct chgc *self)
{
	float result = 0.0f;

	if (self->_full_cap_wh > 0U) {
		result = (chgc_get_remain_cap_kwh(self) /
			  chgc_get_full_cap_kwh(self)) * 100.0f;
	}

	return result;
}

void chgc_recalc_cap(struct chgc *self)
{
	/* Calculate capacity counts */
	int64_t full_cap_counts =
		_chgc_conv_wh_to_counts(self, self->_full_cap_wh);

	self->_cap_counts += (int64_t)self->_voltage_V *
			     (int64_t)self->_current_A;

	/* If voltage is higher than full capacity voltage - increment timer */
	if (self->_voltage_V >=
	    (self->_full_cap_voltage_V * self->_multiplier)) {
		self->_full_cap_voltage_debounce_ms +=
						     self->_update_interval_ms;
	} else {
		self->_full_cap_voltage_debounce_ms = 0U;
	}

	/* If voltage was higher for 5 seconds - set capacity too 100% */
	if (self->_full_cap_voltage_debounce_ms >= 5000U) {
		self->_full_cap_voltage_debounce_ms = 0U;
		self->_cap_counts = full_cap_counts;
	}

	/* Accumulated capacity should not exceed battery capacity 
	 * nor go below negative capacity */
	if (self->_cap_counts > full_cap_counts) {
		self->_cap_counts = full_cap_counts;
	} else if (self->_cap_counts < 0) {
		self->_cap_counts = 0;
	} else {}
}

void chgc_update(struct chgc *self, uint32_t delta_time_ms)
{

	self->_update_timer_ms += (int32_t)delta_time_ms;

	if (self->_update_timer_ms >= (int32_t)self->_update_interval_ms) {
		self->_update_timer_ms -= (int32_t)self->_update_interval_ms;

		chgc_recalc_cap(self);
	}
}

#endif /* CHARGE_COUNTER_H */
