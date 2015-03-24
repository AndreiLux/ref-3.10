#ifndef __DYNAMIC_BOOST_H__
#define __DYNAMIC_BOOST_H__

enum {
	PRIO_TWO_LITTLES,
	PRIO_TWO_LITTLES_MAX_FREQ,
	PRIO_MAX_CORES,
	PRIO_MAX_CORES_MAX_FREQ,
	PRIO_RESET,
	/* Define the max priority for priority limit */
	PRIO_DEFAULT
};

int set_dynamic_boost(int duration, int prio_mode);

#endif	/* __DYNAMIC_BOOST_H__ */
