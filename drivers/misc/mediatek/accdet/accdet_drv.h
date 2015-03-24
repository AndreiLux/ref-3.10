
/****************************************************
globle ACCDET variables
****************************************************/

enum accdet_report_state {
	NO_DEVICE = 0,
	HEADSET_MIC = 1,
	HEADSET_NO_MIC = 2,
	/* HEADSET_ILEGAL = 3, */
	/* DOUBLE_CHECK_TV = 4 */
};

enum accdet_status {
	PLUG_OUT = 0,
	MIC_BIAS = 1,
	/* DOUBLE_CHECK = 2, */
	HOOK_SWITCH = 2,
	/* MIC_BIAS_ILLEGAL =3, */
	/* TV_OUT = 5, */
	STAND_BY = 4
};

char *accdet_status_string[5] = {
	"Plug_out",
	"Headset_plug_in",
	/* "Double_check", */
	"Hook_switch",
	/* "Tvout_plug_in", */
	"Stand_by"
};

char *accdet_report_string[4] = {
	"No_device",
	"Headset_mic",
	"Headset_no_mic",
	/* "HEADSET_illegal", */
	/* "Double_check" */
};

enum hook_switch_result {
	DO_NOTHING = 0,
	ANSWER_CALL = 1,
	REJECT_CALL = 2
};

/*
typedef enum
{
    TVOUT_STATUS_OK = 0,
    TVOUT_STATUS_ALREADY_SET,
    TVOUT_STATUS_ERROR,
} TVOUT_STATUS;
*/

/****************************************************
extern ACCDET API
****************************************************/
void accdet_auxadc_switch(int enable);
void clear_accdet_interrupt(void);

