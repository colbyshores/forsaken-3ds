#pragma once

/* rtlight.h -- level real-time lights (.RTL): fixed, pulsing, flickering, spot
 *
 * rtlight.cpp loads rt_light[] per level (LoadRTLights / ReleaseRTLights) and
 * reserves one XLights[] slot per light at load; ProcessRTLights runs once a
 * frame and writes that slot's colour and visibility. xlight is -1 when the
 * pool was full, and that light is skipped. Trigger-generated lights start off
 * and are switched by EnableRTLight / DisableRTLight from triggers.cpp.
 *
 * Units after load: r/g/b are 0..255 (the file stores 0..1); all times and
 * delays are framelag ticks (file seconds x ANIM_SECOND); pos and range are
 * world units; spot cone is in degrees, rotation_speed in radians per tick.
 */

enum LIGHTTYPE {
	LIGHT_FIXED,
	LIGHT_PULSING,
	LIGHT_FLICKERING,
	LIGHT_SPOT,
};

enum GENTYPE {
	GENTYPE_Initialised,		// Initialised immediatly
	GENTYPE_Time,				// Initialised after time
	GENTYPE_Trigger,			// Initialised by trigger
};

enum LIGHTSTATE {
	STATE_OFF,
	STATE_TURNING_ON,
	STATE_ON,
	STATE_TURNING_OFF,
};

enum PULSETYPE {
	PULSE_BLINK,
	PULSE_RAMP,
	PULSE_HALFWAVE,
	PULSE_WAVE,
};

struct RT_FIXED_LIGHT {
	PULSETYPE on_type;
	PULSETYPE off_type;
	float on_time;
	float off_time;
};

struct RT_PULSING_LIGHT {
	PULSETYPE type;
	float on_time;
	float stay_on_time;
	float off_time;
	float stay_off_time;
	float stay_on_point;
	float off_point;
	float total_time;
};

struct RT_FLICKERING_LIGHT {
	float stay_on_chance;
	float stay_off_chance;
	float stay_on_time;
	float stay_off_time;
};

struct RT_SPOT_LIGHT {
	VECTOR dir;
	VECTOR up;
	float cone;
	float rotation_period;
	float rotation_speed;
	float angle;
};

struct RT_LIGHT
{
	LIGHTTYPE type;
	u_int16_t group;
	VECTOR pos;
	float range;
	float r;
	float g;
	float b;
	GENTYPE generation_type;
	float generation_delay;
	union
	{
		RT_FIXED_LIGHT fixed;
		RT_PULSING_LIGHT pulse;
		RT_FLICKERING_LIGHT flicker;
		RT_SPOT_LIGHT spot;
	};
	u_int16_t xlight;
	bool enabled;
	LIGHTSTATE state;
	float now_time;
	float delay;
	float intensity;
};


extern bool LoadRTLights( void );

extern bool ReleaseRTLights( void );

extern void ProcessRTLights( void );

extern void EnableRTLight( u_int16_t *data );

extern void DisableRTLight( u_int16_t *data );

FILE * SaveRealTimeLights( FILE * fp );
FILE * LoadRealTimeLights( FILE * fp );
