
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <asm/mach-types.h>
#include <mach/mt_board.h>
#include <mach/board.h>
#include <mach/battery_custom_data.h>
#include <mach/charging.h>
#include <linux/i2c.h>

#define BAT_NTC_10 0
#define BAT_NTC_47 0
#define BAT_NTC_100 1

/* ============================================================ */
/* <DOD, Battery_Voltage> Table */
/* ============================================================ */
#if (BAT_NTC_10 == 1)
BATT_TEMPERATURE Batt_Temperature_Table[] = {
	{-20, 68237}
	,
	{-15, 53650}
	,
	{-10, 42506}
	,
	{-5, 33892}
	,
	{0, 27219}
	,
	{5, 22021}
	,
	{10, 17926}
	,
	{15, 14674}
	,
	{20, 12081}
	,
	{25, 10000}
	,
	{30, 8315}
	,
	{35, 6948}
	,
	{40, 5834}
	,
	{45, 4917}
	,
	{50, 4161}
	,
	{55, 3535}
	,
	{60, 3014}
};
#endif

#if (BAT_NTC_47 == 1)
BATT_TEMPERATURE Batt_Temperature_Table[] = {
	{-20, 483954}
	,
	{-15, 360850}
	,
	{-10, 271697}
	,
	{-5, 206463}
	,
	{0, 158214}
	,
	{5, 122259}
	,
	{10, 95227}
	,
	{15, 74730}
	,
	{20, 59065}
	,
	{25, 47000}
	,
	{30, 37643}
	,
	{35, 30334}
	,
	{40, 24591}
	,
	{45, 20048}
	,
	{50, 16433}
	,
	{55, 13539}
	,
	{60, 11210}
};
#endif

#if (BAT_NTC_100 == 1)
BATT_TEMPERATURE Batt_Temperature_Table[] = {
	{-20, 1151037}
	,
	{-15, 846579}
	,
	{-10, 628988}
	,
	{-5, 471632}
	,
	{0, 357012}
	,
	{5, 272500}
	,
	{10, 209710}
	,
	{15, 162651}
	,
	{20, 127080}
	,
	{25, 100000}
	,
	{30, 79222}
	,
	{35, 63167}
	,
	{40, 50677}
	,
	{45, 40904}
	,
	{50, 33195}
	,
	{55, 27091}
	,
	{60, 22224}
};
#endif

/* T0 -10C */
BATTERY_PROFILE_STRUC battery_profile_t0[] = {
	{0, 4180}
	,
	{2, 4157}
	,
	{4, 4139}
	,
	{5, 4120}
	,
	{7, 4105}
	,
	{9, 4087}
	,
	{11, 4072}
	,
	{12, 4057}
	,
	{14, 4043}
	,
	{16, 4028}
	,
	{18, 4013}
	,
	{20, 4001}
	,
	{21, 3989}
	,
	{23, 3976}
	,
	{25, 3964}
	,
	{27, 3953}
	,
	{28, 3942}
	,
	{30, 3930}
	,
	{32, 3920}
	,
	{34, 3910}
	,
	{36, 3898}
	,
	{37, 3883}
	,
	{39, 3865}
	,
	{41, 3851}
	,
	{43, 3840}
	,
	{45, 3831}
	,
	{46, 3824}
	,
	{48, 3816}
	,
	{50, 3811}
	,
	{52, 3804}
	,
	{53, 3800}
	,
	{55, 3794}
	,
	{57, 3790}
	,
	{59, 3786}
	,
	{61, 3784}
	,
	{62, 3780}
	,
	{64, 3778}
	,
	{66, 3774}
	,
	{68, 3772}
	,
	{69, 3768}
	,
	{71, 3765}
	,
	{73, 3761}
	,
	{75, 3755}
	,
	{77, 3748}
	,
	{78, 3741}
	,
	{80, 3735}
	,
	{82, 3725}
	,
	{84, 3715}
	,
	{86, 3704}
	,
	{87, 3688}
	,
	{89, 3679}
	,
	{91, 3673}
	,
	{93, 3666}
	,
	{94, 3657}
	,
	{96, 3632}
	,
	{98, 3553}
	,
	{100, 3426}
	,
	{101, 3329}
	,
	{101, 3306}
	,
	{101, 3298}
	,
	{101, 3294}
	,
	{101, 3292}
	,
	{101, 3291}
	,
	{101, 3288}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
};

/* T1 0C */
BATTERY_PROFILE_STRUC battery_profile_t1[] = {
	{0, 4180}
	,
	{2, 4157}
	,
	{4, 4139}
	,
	{5, 4120}
	,
	{7, 4105}
	,
	{9, 4087}
	,
	{11, 4072}
	,
	{12, 4057}
	,
	{14, 4043}
	,
	{16, 4028}
	,
	{18, 4013}
	,
	{20, 4001}
	,
	{21, 3989}
	,
	{23, 3976}
	,
	{25, 3964}
	,
	{27, 3953}
	,
	{28, 3942}
	,
	{30, 3930}
	,
	{32, 3920}
	,
	{34, 3910}
	,
	{36, 3898}
	,
	{37, 3883}
	,
	{39, 3865}
	,
	{41, 3851}
	,
	{43, 3840}
	,
	{45, 3831}
	,
	{46, 3824}
	,
	{48, 3816}
	,
	{50, 3811}
	,
	{52, 3804}
	,
	{53, 3800}
	,
	{55, 3794}
	,
	{57, 3790}
	,
	{59, 3786}
	,
	{61, 3784}
	,
	{62, 3780}
	,
	{64, 3778}
	,
	{66, 3774}
	,
	{68, 3772}
	,
	{69, 3768}
	,
	{71, 3765}
	,
	{73, 3761}
	,
	{75, 3755}
	,
	{77, 3748}
	,
	{78, 3741}
	,
	{80, 3735}
	,
	{82, 3725}
	,
	{84, 3715}
	,
	{86, 3704}
	,
	{87, 3688}
	,
	{89, 3679}
	,
	{91, 3673}
	,
	{93, 3666}
	,
	{94, 3657}
	,
	{96, 3632}
	,
	{98, 3553}
	,
	{100, 3426}
	,
	{101, 3329}
	,
	{101, 3306}
	,
	{101, 3298}
	,
	{101, 3294}
	,
	{101, 3292}
	,
	{101, 3291}
	,
	{101, 3288}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
};

/* T2 25C */
BATTERY_PROFILE_STRUC battery_profile_t2[] = {
	{0, 4180}
	,
	{2, 4157}
	,
	{4, 4139}
	,
	{5, 4120}
	,
	{7, 4105}
	,
	{9, 4087}
	,
	{11, 4072}
	,
	{12, 4057}
	,
	{14, 4043}
	,
	{16, 4028}
	,
	{18, 4013}
	,
	{20, 4001}
	,
	{21, 3989}
	,
	{23, 3976}
	,
	{25, 3964}
	,
	{27, 3953}
	,
	{28, 3942}
	,
	{30, 3930}
	,
	{32, 3920}
	,
	{34, 3910}
	,
	{36, 3898}
	,
	{37, 3883}
	,
	{39, 3865}
	,
	{41, 3851}
	,
	{43, 3840}
	,
	{45, 3831}
	,
	{46, 3824}
	,
	{48, 3816}
	,
	{50, 3811}
	,
	{52, 3804}
	,
	{53, 3800}
	,
	{55, 3794}
	,
	{57, 3790}
	,
	{59, 3786}
	,
	{61, 3784}
	,
	{62, 3780}
	,
	{64, 3778}
	,
	{66, 3774}
	,
	{68, 3772}
	,
	{69, 3768}
	,
	{71, 3765}
	,
	{73, 3761}
	,
	{75, 3755}
	,
	{77, 3748}
	,
	{78, 3741}
	,
	{80, 3735}
	,
	{82, 3725}
	,
	{84, 3715}
	,
	{86, 3704}
	,
	{87, 3688}
	,
	{89, 3679}
	,
	{91, 3673}
	,
	{93, 3666}
	,
	{94, 3657}
	,
	{96, 3632}
	,
	{98, 3553}
	,
	{100, 3426}
	,
	{101, 3329}
	,
	{101, 3306}
	,
	{101, 3298}
	,
	{101, 3294}
	,
	{101, 3292}
	,
	{101, 3291}
	,
	{101, 3288}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
};

/* T3 50C */
BATTERY_PROFILE_STRUC battery_profile_t3[] = {
	{0, 4180}
	,
	{2, 4157}
	,
	{4, 4139}
	,
	{5, 4120}
	,
	{7, 4105}
	,
	{9, 4087}
	,
	{11, 4072}
	,
	{12, 4057}
	,
	{14, 4043}
	,
	{16, 4028}
	,
	{18, 4013}
	,
	{20, 4001}
	,
	{21, 3989}
	,
	{23, 3976}
	,
	{25, 3964}
	,
	{27, 3953}
	,
	{28, 3942}
	,
	{30, 3930}
	,
	{32, 3920}
	,
	{34, 3910}
	,
	{36, 3898}
	,
	{37, 3883}
	,
	{39, 3865}
	,
	{41, 3851}
	,
	{43, 3840}
	,
	{45, 3831}
	,
	{46, 3824}
	,
	{48, 3816}
	,
	{50, 3811}
	,
	{52, 3804}
	,
	{53, 3800}
	,
	{55, 3794}
	,
	{57, 3790}
	,
	{59, 3786}
	,
	{61, 3784}
	,
	{62, 3780}
	,
	{64, 3778}
	,
	{66, 3774}
	,
	{68, 3772}
	,
	{69, 3768}
	,
	{71, 3765}
	,
	{73, 3761}
	,
	{75, 3755}
	,
	{77, 3748}
	,
	{78, 3741}
	,
	{80, 3735}
	,
	{82, 3725}
	,
	{84, 3715}
	,
	{86, 3704}
	,
	{87, 3688}
	,
	{89, 3679}
	,
	{91, 3673}
	,
	{93, 3666}
	,
	{94, 3657}
	,
	{96, 3632}
	,
	{98, 3553}
	,
	{100, 3426}
	,
	{101, 3329}
	,
	{101, 3306}
	,
	{101, 3298}
	,
	{101, 3294}
	,
	{101, 3292}
	,
	{101, 3291}
	,
	{101, 3288}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
	,
	{101, 3286}
};

/* battery profile for actual temperature. The size should be the same as T1, T2 and T3 */
BATTERY_PROFILE_STRUC battery_profile_temperature[] = {
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
};

/* ============================================================ */
/* <Rbat, Battery_Voltage> Table */
/* ============================================================ */
/* BAT1 R profile */
/* T0 -10C */
R_PROFILE_STRUC r_profile_t0[] = {
	{113, 4180}
	,
	{113, 4157}
	,
	{117, 4139}
	,
	{115, 4120}
	,
	{122, 4105}
	,
	{118, 4087}
	,
	{123, 4072}
	,
	{127, 4057}
	,
	{130, 4043}
	,
	{128, 4028}
	,
	{130, 4013}
	,
	{135, 4001}
	,
	{140, 3989}
	,
	{143, 3976}
	,
	{145, 3964}
	,
	{148, 3953}
	,
	{150, 3942}
	,
	{153, 3930}
	,
	{155, 3920}
	,
	{157, 3910}
	,
	{153, 3898}
	,
	{147, 3883}
	,
	{133, 3865}
	,
	{127, 3851}
	,
	{120, 3840}
	,
	{118, 3831}
	,
	{118, 3824}
	,
	{118, 3816}
	,
	{123, 3811}
	,
	{122, 3804}
	,
	{123, 3800}
	,
	{122, 3794}
	,
	{125, 3790}
	,
	{123, 3786}
	,
	{128, 3784}
	,
	{127, 3780}
	,
	{128, 3778}
	,
	{130, 3774}
	,
	{130, 3772}
	,
	{128, 3768}
	,
	{128, 3765}
	,
	{130, 3761}
	,
	{128, 3755}
	,
	{122, 3748}
	,
	{122, 3741}
	,
	{127, 3735}
	,
	{125, 3725}
	,
	{123, 3715}
	,
	{127, 3704}
	,
	{123, 3688}
	,
	{127, 3679}
	,
	{128, 3673}
	,
	{135, 3666}
	,
	{148, 3657}
	,
	{160, 3632}
	,
	{162, 3553}
	,
	{193, 3426}
	,
	{217, 3329}
	,
	{180, 3306}
	,
	{167, 3298}
	,
	{158, 3294}
	,
	{155, 3292}
	,
	{155, 3291}
	,
	{150, 3288}
	,
	{150, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
};

/* T1 0C */
R_PROFILE_STRUC r_profile_t1[] = {
	{113, 4180}
	,
	{113, 4157}
	,
	{117, 4139}
	,
	{115, 4120}
	,
	{122, 4105}
	,
	{118, 4087}
	,
	{123, 4072}
	,
	{127, 4057}
	,
	{130, 4043}
	,
	{128, 4028}
	,
	{130, 4013}
	,
	{135, 4001}
	,
	{140, 3989}
	,
	{143, 3976}
	,
	{145, 3964}
	,
	{148, 3953}
	,
	{150, 3942}
	,
	{153, 3930}
	,
	{155, 3920}
	,
	{157, 3910}
	,
	{153, 3898}
	,
	{147, 3883}
	,
	{133, 3865}
	,
	{127, 3851}
	,
	{120, 3840}
	,
	{118, 3831}
	,
	{118, 3824}
	,
	{118, 3816}
	,
	{123, 3811}
	,
	{122, 3804}
	,
	{123, 3800}
	,
	{122, 3794}
	,
	{125, 3790}
	,
	{123, 3786}
	,
	{128, 3784}
	,
	{127, 3780}
	,
	{128, 3778}
	,
	{130, 3774}
	,
	{130, 3772}
	,
	{128, 3768}
	,
	{128, 3765}
	,
	{130, 3761}
	,
	{128, 3755}
	,
	{122, 3748}
	,
	{122, 3741}
	,
	{127, 3735}
	,
	{125, 3725}
	,
	{123, 3715}
	,
	{127, 3704}
	,
	{123, 3688}
	,
	{127, 3679}
	,
	{128, 3673}
	,
	{135, 3666}
	,
	{148, 3657}
	,
	{160, 3632}
	,
	{162, 3553}
	,
	{193, 3426}
	,
	{217, 3329}
	,
	{180, 3306}
	,
	{167, 3298}
	,
	{158, 3294}
	,
	{155, 3292}
	,
	{155, 3291}
	,
	{150, 3288}
	,
	{150, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
};

/* T2 25C */
R_PROFILE_STRUC r_profile_t2[] = {
	{113, 4180}
	,
	{113, 4157}
	,
	{117, 4139}
	,
	{115, 4120}
	,
	{122, 4105}
	,
	{118, 4087}
	,
	{123, 4072}
	,
	{127, 4057}
	,
	{130, 4043}
	,
	{128, 4028}
	,
	{130, 4013}
	,
	{135, 4001}
	,
	{140, 3989}
	,
	{143, 3976}
	,
	{145, 3964}
	,
	{148, 3953}
	,
	{150, 3942}
	,
	{153, 3930}
	,
	{155, 3920}
	,
	{157, 3910}
	,
	{153, 3898}
	,
	{147, 3883}
	,
	{133, 3865}
	,
	{127, 3851}
	,
	{120, 3840}
	,
	{118, 3831}
	,
	{118, 3824}
	,
	{118, 3816}
	,
	{123, 3811}
	,
	{122, 3804}
	,
	{123, 3800}
	,
	{122, 3794}
	,
	{125, 3790}
	,
	{123, 3786}
	,
	{128, 3784}
	,
	{127, 3780}
	,
	{128, 3778}
	,
	{130, 3774}
	,
	{130, 3772}
	,
	{128, 3768}
	,
	{128, 3765}
	,
	{130, 3761}
	,
	{128, 3755}
	,
	{122, 3748}
	,
	{122, 3741}
	,
	{127, 3735}
	,
	{125, 3725}
	,
	{123, 3715}
	,
	{127, 3704}
	,
	{123, 3688}
	,
	{127, 3679}
	,
	{128, 3673}
	,
	{135, 3666}
	,
	{148, 3657}
	,
	{160, 3632}
	,
	{162, 3553}
	,
	{193, 3426}
	,
	{217, 3329}
	,
	{180, 3306}
	,
	{167, 3298}
	,
	{158, 3294}
	,
	{155, 3292}
	,
	{155, 3291}
	,
	{150, 3288}
	,
	{150, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
};

/* T3 50C */
R_PROFILE_STRUC r_profile_t3[] = {
	{113, 4180}
	,
	{113, 4157}
	,
	{117, 4139}
	,
	{115, 4120}
	,
	{122, 4105}
	,
	{118, 4087}
	,
	{123, 4072}
	,
	{127, 4057}
	,
	{130, 4043}
	,
	{128, 4028}
	,
	{130, 4013}
	,
	{135, 4001}
	,
	{140, 3989}
	,
	{143, 3976}
	,
	{145, 3964}
	,
	{148, 3953}
	,
	{150, 3942}
	,
	{153, 3930}
	,
	{155, 3920}
	,
	{157, 3910}
	,
	{153, 3898}
	,
	{147, 3883}
	,
	{133, 3865}
	,
	{127, 3851}
	,
	{120, 3840}
	,
	{118, 3831}
	,
	{118, 3824}
	,
	{118, 3816}
	,
	{123, 3811}
	,
	{122, 3804}
	,
	{123, 3800}
	,
	{122, 3794}
	,
	{125, 3790}
	,
	{123, 3786}
	,
	{128, 3784}
	,
	{127, 3780}
	,
	{128, 3778}
	,
	{130, 3774}
	,
	{130, 3772}
	,
	{128, 3768}
	,
	{128, 3765}
	,
	{130, 3761}
	,
	{128, 3755}
	,
	{122, 3748}
	,
	{122, 3741}
	,
	{127, 3735}
	,
	{125, 3725}
	,
	{123, 3715}
	,
	{127, 3704}
	,
	{123, 3688}
	,
	{127, 3679}
	,
	{128, 3673}
	,
	{135, 3666}
	,
	{148, 3657}
	,
	{160, 3632}
	,
	{162, 3553}
	,
	{193, 3426}
	,
	{217, 3329}
	,
	{180, 3306}
	,
	{167, 3298}
	,
	{158, 3294}
	,
	{155, 3292}
	,
	{155, 3291}
	,
	{150, 3288}
	,
	{150, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
	,
	{148, 3286}
};

/* r-table profile for actual temperature. The size should be the same as T1, T2 and T3 */
R_PROFILE_STRUC r_profile_temperature[] = {
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
	,
	{0, 0}
};

/* ============================================================ */
/* register device information */
/* ============================================================ */

mt_battery_meter_custom_data mt_bat_meter_data = {
	.cust_tabt_number = 8,
#ifdef CONFIG_SWCHR_POWER_PATH
	.vbat_channel_number = 1,	/* w/ powerpath, battery voltage is ISENSE(1) */
	.isense_channel_number = 0,	/* w/ powerpath, system voltage is VSENSE(0) */
#else
	.vbat_channel_number = 0,
	.isense_channel_number = 1,
#endif
	.vcharger_channel_number = 2,
	.vbattemp_channel_number = 3,

	/*ADC resistor  */
	.r_bat_sense = 4,
	.r_i_sense = 4,
	.r_charger_1 = 330,
	.r_charger_2 = 39,

	.temperature_t0 = 110,
	.temperature_t1 = 0,
	.temperature_t1_5 = 10,
	.temperature_t2 = 25,
	.temperature_t3 = 50,
	.temperature_t = 255,	/* This should be fixed, never change the value */

	.fg_meter_resistance = 0,

	.q_max_pos_50 = 3029,
	.q_max_pos_25 = 3029,
	.q_max_pos_10 = 3029,
	.q_max_pos_0 = 3029,
	.q_max_neg_10 = 3029,

	.q_max_pos_50_h_current = 2989,
	.q_max_pos_25_h_current = 2989,
	.q_max_pos_10_h_current = 2989,
	.q_max_pos_0_h_current = 2989,
	.q_max_neg_10_h_current = 2989,

	/* Discharge Percentage */
	.oam_d5 = 1,		/* 1 : D5,   0: D2 */

	.cust_tracking_point = 0,
	.cust_r_sense = 68,
	.cust_hw_cc = 0,
	.aging_tuning_value = 100,
	.cust_r_fg_offset = 23,

	.ocv_board_compesate = 0,	/* mV */
	.r_fg_board_base = 1000,
	.r_fg_board_slope = 1000,
	.car_tune_value = 92,

	/* HW Fuel gague  */
	.current_detect_r_fg = -1,	/* turn off auto detect for auxadc gauge */
	.min_error_offset = 1000,
	.fg_vbat_average_size = 18,
	.r_fg_value = 20,	/* mOhm, base is 20 */

	.poweron_delta_capacity_tolerance = 45,
	.poweron_low_capacity_tolerance = 5,
	.poweron_max_vbat_tolerance = 90,
	.poweron_delta_vbat_tolerance = 30,

	/* Dynamic change wake up period of battery thread when suspend */
	.vbat_normal_wakeup = 3600,	/* 3.6V */
	.vbat_low_power_wakeup = 3500,	/* 3.5V */
	.normal_wakeup_period = 5400,	/* 90 * 60 = 90 min */
	.low_power_wakeup_period = 300,	/* 5 * 60 = 5 min */
	.close_poweroff_wakeup_period = 30,	/* 30s */

	/* meter table */
	.rbat_pull_up_volt = 1200,
#if (BAT_NTC_10 == 1)
	.rbat_pull_up_r = 16900,
	.rbat_pull_down_r = 27000,
#elif (BAT_NTC_47 == 1)
	.rbat_pull_up_r = 61900,
	.rbat_pull_down_r = 100000,
#elif (BAT_NTC_100 == 1)
	.rbat_pull_up_r = 24000,
	.rbat_pull_down_r = 100000000,
#endif
	.battery_profile_saddles =
	    sizeof(battery_profile_temperature) / sizeof(BATTERY_PROFILE_STRUC),
	.battery_r_profile_saddles = sizeof(r_profile_temperature) / sizeof(R_PROFILE_STRUC),
	.battery_aging_table_saddles = 0,
	.battery_ntc_table_saddles = sizeof(Batt_Temperature_Table) / sizeof(BATT_TEMPERATURE),
	.p_batt_temperature_table = Batt_Temperature_Table,
	.p_battery_profile_t0 = battery_profile_t0,
	.p_battery_profile_t1 = battery_profile_t1,
	.p_battery_profile_t1_5 = NULL,
	.p_battery_profile_t2 = battery_profile_t2,
	.p_battery_profile_t3 = battery_profile_t3,
	.p_r_profile_t0 = r_profile_t0,
	.p_r_profile_t1 = r_profile_t1,
	.p_r_profile_t1_5 = NULL,
	.p_r_profile_t2 = r_profile_t2,
	.p_r_profile_t3 = r_profile_t3,
	.p_battery_profile_temperature = battery_profile_temperature,
	.p_r_profile_temperature = r_profile_temperature,
	.p_battery_aging_table = NULL
};

mt_battery_charging_custom_data mt_bat_charging_data = {

	.talking_recharge_voltage = 3800,
	.talking_sync_time = 60,

	/* Battery Temperature Protection */
	.max_charge_temperature = 50,
	.min_charge_temperature = 0,
	.err_charge_temperature = 0xFF,
	.use_avg_temperature = 1,

	/* Linear Charging Threshold */
	.v_pre2cc_thres = 3400,	/* mV */
	.v_cc2topoff_thres = 4050,
	.recharging_voltage = 4110,
	.charging_full_current = 150,	/* mA */

	/* CONFIG_USB_IF */
	.usb_charger_current_suspend = 0,
	.usb_charger_current_unconfigured = CHARGE_CURRENT_70_00_MA,
	.usb_charger_current_configured = CHARGE_CURRENT_500_00_MA,

	.usb_charger_current = CHARGE_CURRENT_500_00_MA,
	.ac_charger_current = 204800,
	.non_std_ac_charger_current = CHARGE_CURRENT_500_00_MA,
	.charging_host_charger_current = CHARGE_CURRENT_650_00_MA,
	.apple_0_5a_charger_current = CHARGE_CURRENT_500_00_MA,
	.apple_1_0a_charger_current = CHARGE_CURRENT_1000_00_MA,
	.apple_2_1a_charger_current = CHARGE_CURRENT_2000_00_MA,

	/* Charger error check */
	/* BAT_LOW_TEMP_PROTECT_ENABLE */
	.v_charger_enable = 0,	/* 1:ON , 0:OFF */
	.v_charger_max = 6500,	/* 6.5 V */
	.v_charger_min = 4400,	/* 4.4 V */

	/* Tracking time */
	.onehundred_percent_tracking_time = 10,	/* 10 second */
	.npercent_tracking_time = 20,	/* 20 second */
	.sync_to_real_tracking_time = 60,	/* 60 second */

	/* JEITA parameter */
	.cust_soc_jeita_sync_time = 30,
	.jeita_recharge_voltage = 4110,	/* for linear charging */
	.jeita_temp_above_pos_60_cv_voltage = BATTERY_VOLT_04_100000_V,
	.jeita_temp_pos_45_to_pos_60_cv_voltage = BATTERY_VOLT_04_100000_V,
	.jeita_temp_pos_10_to_pos_45_cv_voltage = BATTERY_VOLT_04_200000_V,
	.jeita_temp_pos_0_to_pos_10_cv_voltage = BATTERY_VOLT_04_100000_V,
	.jeita_temp_neg_10_to_pos_0_cv_voltage = BATTERY_VOLT_03_900000_V,
	.jeita_temp_below_neg_10_cv_voltage = BATTERY_VOLT_03_900000_V,
	.temp_pos_60_threshold = 50,
	.temp_pos_60_thres_minus_x_degree = 47,
	.temp_pos_45_threshold = 45,
	.temp_pos_45_thres_minus_x_degree = 39,
	.temp_pos_10_threshold = 10,
	.temp_pos_10_thres_plus_x_degree = 16,
	.temp_pos_0_threshold = 0,
	.temp_pos_0_thres_plus_x_degree = 6,
	.temp_neg_10_threshold = 0,
	.temp_neg_10_thres_plus_x_degree = 0,

	/* For JEITA Linear Charging Only */
	.jeita_neg_10_to_pos_0_full_current = 120,	/* mA */
	.jeita_temp_pos_45_to_pos_60_recharge_voltage = 4000,
	.jeita_temp_pos_10_to_pos_45_recharge_voltage = 4100,
	.jeita_temp_pos_0_to_pos_10_recharge_voltage = 4000,
	.jeita_temp_neg_10_to_pos_0_recharge_voltage = 3800,
	.jeita_temp_pos_45_to_pos_60_cc2topoff_threshold = 4050,
	.jeita_temp_pos_10_to_pos_45_cc2topoff_threshold = 4050,
	.jeita_temp_pos_0_to_pos_10_cc2topoff_threshold = 4050,
	.jeita_temp_neg_10_to_pos_0_cc2topoff_threshold = 3850
};

static struct platform_device battery_meter_device = {
	.name = "battery_meter",
	.id = -1,
	.dev = {
		.platform_data = &mt_bat_meter_data}
};

static struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
	.dev = {
		.platform_data = &mt_bat_charging_data}
};

void __init mt_battery_init(void)
{
	int ret;

	mt_custom_battery_init();

	ret = platform_device_register(&battery_meter_device);
	if (ret)
		pr_info("[battery meter] Unable to register device(%d)\n", ret);
	else
		pr_info("[battery meter] register device succeed(%d)\n", ret);

	ret = platform_device_register(&battery_device);
	if (ret)
		pr_info("[battery driver] Unable to device register(%d)\n", ret);
}
