#ifndef _CUST_TMP103_COOLER_
#define _CUST_TMP103_COOLER_

#define MIN_CPU_POWER (594)
#define MAX_CPU_POWER (4600)
#define MAX_BRIGHTNESS (255)
#define MIN_BRIGHTNESS (10)
#define MAX_CHARGING_LIMIT (CHARGE_CURRENT_MAX)
#define MIN_CHARGING_LIMIT (0)

enum tmp103_cooler_type {
	TMP103_COOLER_CPU = 0,
	TMP103_COOLER_BL,
	TMP103_COOLER_BC,
};

struct tmp103_cooler_pdev {
	enum tmp103_cooler_type ctype;
	int action;
	int clear;
	char *name;
};

struct tmp103_cooler_pdata {
	int count;
	struct tmp103_cooler_pdev *list;
};

#endif /* _CUST_TMP103_COOLER_ */
