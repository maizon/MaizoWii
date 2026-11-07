/* WiiMC-SSLC numeric options which can be safely configured without
 * editing WiiMC's XML, credentials, sources or free-form text fields.
 * The patched module consumes the same values from maizo-launch.txt.
 * Not all original WiiMC GUI settings are numeric or remotely controllable. */
#ifndef MAIZO_WIIMC_OPTIONS_H
#define MAIZO_WIIMC_OPTIONS_H
#define MAIZO_WIIMC_OPTION_COUNT 35
typedef enum {MAIZO_WIIMC_NUMBER,MAIZO_WIIMC_PERCENT,MAIZO_WIIMC_SIGNED,MAIZO_WIIMC_TENTHS,MAIZO_WIIMC_BOOL,MAIZO_WIIMC_TENTHS_POS} MaizoWiiMCOptionType;
typedef struct {const char *key,*label;unsigned min,max,step,default_value;MaizoWiiMCOptionType type;} MaizoWiiMCOption;
extern const MaizoWiiMCOption maizo_wiimc_options[MAIZO_WIIMC_OPTION_COUNT];
#endif
