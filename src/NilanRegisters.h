#ifndef NILAN_REGISTERS_H
#define NILAN_REGISTERS_H

#include <stdint.h>

/**
 * One array holds all metadata - this goes to flash.
 * Another array holds values for each register - in ram.
 */

#define NILAN_HOLDING_REG 0x03//((uint8_t)3)
#define NILAN_INPUT_REG 0x04//((uint8_t)4)

static const char *control_modes[] = {
    "Off", "Heat", "Cool", "Auto", "Service"};

static const char *control_states[] = {
    "Off", "Shift", "Stop", "Start", "Standby", "Ventilation Stop",
    "Ventilation", "Heating", "Cooling", "Hot Water",
    "Legionella", "Cooling + hot water", "Central Heating",
    "Defrost", "Frost secure", "Service", "Alarm",
    "Heatin + hot water"};

// static const char *alarm_list[] = {
//     "None", "Hardware", "Timeout", "Fire", "Pressure",
//     "Door", "Defrost", "Frost", "N/A(frost-T9)", "OverTemp",
//     "Overheat", "Airflow", "Thermo", "Boiling", "Sensor",
//     "Room low", "Software", "Watchdog", "Config", "Filter"
//     "Legionella", "Power", "Air Temp", "Water Temp", "Heat Temp",
//     "N/A (modem)", ... hmmm... jumps in number...

// };

static const char *week_programs[] = {
    "None", "Program 1", "Program 2", "Program 3", "Erase"};

static const char *user_functions[] = {
    "None", "Exted", "Inlet", "Exhaust", "External heater offset",
    "Ventilate", "Cooker hood"};

static const char *air_exchange_modes[] = {
    "Energy", "Comfort", "Comfort water"};

typedef enum
{
    //
    // Input registers (IR, function code 0x04)
    //

    // 000 – Device / protocol
    NILAN_REGID_IR_BUS_VERSION = 0,     // 000
    NILAN_REGID_IR_APP_VERSION_MAJOR,   // 001
    NILAN_REGID_IR_APP_VERSION_MINOR,   // 002
    NILAN_REGID_IR_APP_VERSION_RELEASE, // 003

    // 100 – Discrete inputs
    NILAN_REGID_IR_INPUT_USERFUNC, // 100
    NILAN_REGID_IR_INPUT_AIRFILTER_ALARM,
    // NILAN_REGID_IR_INPUT_DOOR_OPEN,
    NILAN_REGID_IR_INPUT_SMOKE_ALARM,
    // NILAN_REGID_IR_INPUT_MOTOR_THERMO_ALARM,
    NILAN_REGID_IR_INPUT_FROST_OVERHT,
    NILAN_REGID_IR_INPUT_AIRFLOW_MONITOR,
    NILAN_REGID_IR_INPUT_HI_PRESSURE_SWITCH,
    // NILAN_REGID_IR_INPUT_LO_PRESSURE_SWITCH,
    NILAN_REGID_IR_INPUT_BOILING,
    // NILAN_REGID_IR_INPUT_3WAY_POS,
    // NILAN_REGID_IR_INPUT_DEFROST_HG,
    NILAN_REGID_IR_INPUT_DEFROST_THERMOSTAT,
    NILAN_REGID_IR_INPUT_USERFUNC2,
    // NILAN_REGID_IR_INPUT_DAMPER_CLOSED,
    // NILAN_REGID_IR_INPUT_DAMPER_OPENED,
    // NILAN_REGID_IR_INPUT_FC_OR_THERMO_ALARM,

    // 200 – Temperatures, pressures, RH, CO2
    NILAN_REGID_IR_T0_CONTROLLER,
    // NILAN_REGID_IR_T1_INTAKE, // connected to T8
    NILAN_REGID_IR_T2_INLET,      // HMMMM... gives 0.....
                                  // NILAN_REGID_IR_T3_EXHAUST,    // not connected
                                  // NILAN_REGID_IR_T4_OUTLET,     // not connected
    NILAN_REGID_IR_T5_CONDENSOR,  //*
    NILAN_REGID_IR_T6_EVAPORATOR, //*
    NILAN_REGID_IR_T7_INLET_POSTHEATER,
    NILAN_REGID_IR_T8_OUTDOOR,
    // NILAN_REGID_IR_T9_HEATER,
    NILAN_REGID_IR_T10_EXTERN_ROOM,
    NILAN_REGID_IR_T11_TANK_TOP,    //*
    NILAN_REGID_IR_T12_TANK_BOTTOM, //*
                                    // NILAN_REGID_IR_T13_RETURN,
                                    // NILAN_REGID_IR_T14_SUPPLY,
    NILAN_REGID_IR_T15_ROOM_PANEL,  //*
    NILAN_REGID_IR_T16_AUX,
    // NILAN_REGID_IR_T17_PREHEAT,
    // NILAN_REGID_IR_T18_PRESS_PIPE,
    // NILAN_REGID_IR_P_SUC,
    // NILAN_REGID_IR_P_DIS,
    NILAN_REGID_IR_HUMIDITY, // RH humidity.
                             // NILAN_REGID_IR_AIRQUAL_CO2,

    // 4xx – Alarm list
    NILAN_REGID_IR_ALARM_STATUS, // bitmask 0x80 active alarms, 0x03 # of alarms
    NILAN_REGID_IR_ALARM_LIST1_ID,
    NILAN_REGID_IR_ALARM_LIST1_DATE,
    NILAN_REGID_IR_ALARM_LIST1_TIME,
    NILAN_REGID_IR_ALARM_LIST2_ID,
    NILAN_REGID_IR_ALARM_LIST2_DATE,
    NILAN_REGID_IR_ALARM_LIST2_TIME,
    NILAN_REGID_IR_ALARM_LIST3_ID,
    NILAN_REGID_IR_ALARM_LIST3_DATE,
    NILAN_REGID_IR_ALARM_LIST3_TIME,

    // 1xxx – Control (status)
    NILAN_REGID_IR_CONTROL_RUNNING, // 0=Off, 1=on
    NILAN_REGID_IR_CONTROL_MODE,    // 0=off, 1=Heat, 2=cool, 3=auto, 4=service
    NILAN_REGID_IR_CONTROL_STATE,   // 0 - 17. see document.
    NILAN_REGID_IR_CONTROL_SEC_IN_STATE,

    // 11xx – AirFlow (status)
    NILAN_REGID_IR_AIRFLOW_VENT_SETPOINT, // 1-4, 0=off
    NILAN_REGID_IR_AIRFLOW_INLET_FAN_STEP,
    NILAN_REGID_IR_AIRFLOW_EXHAUST_FAN_STEP,
    NILAN_REGID_IR_AIRFLOW_DAYS_SINCE_FILTER_CHG,
    NILAN_REGID_IR_AIRFLOW_DAYS_TO_FILTER_CHG,

    // 12xx – AirTemp (status)
    NILAN_REGID_IR_AIRTEMP_SUMMER_STATE, // 0=winter.
    NILAN_REGID_IR_AIRTEMP_TEMP_INLET_SETPT,
    NILAN_REGID_IR_AIRTEMP_TEMP_CONTROL,
    NILAN_REGID_IR_AIRTEMP_TEMP_ROOM,
    // NILAN_REGID_IR_AIRTEMP_EFF_PCT,
    NILAN_REGID_IR_AIRTEMP_CAP_SET,
    NILAN_REGID_IR_AIRTEMP_CAP_ACT,

    // 1500 – Compressor
    // NILAN_REGID_IR_COMPRESSOR_TYPE,

    // 1700 – HotWater (status)
    // NILAN_REGID_IR_HOTWATER_TYPE,

    // 1800 – Central heating (status)
    NILAN_REGID_IR_CENTHEAT_HEAT_EXT_SETPOINT, // Actual setpoint for external heating source

    // 2100 – PreHeat (status)
    // NILAN_REGID_IR_PREHEAT_BLOCK_REMAIN,

    // 2200 – DPT (status)
    // NILAN_REGID_IR_DPT_IN_SESSION,
    // NILAN_REGID_IR_DPT_AIRFLOW1,
    // NILAN_REGID_IR_DPT_AIRFLOW2,

    // 3000 - Display - we ignore them...

    // ==============================================================
    // Holding registers (HR, function codes 0x03 / 0x10)
    // ==============================================================

    // 000 – Device / protocol
    NILAN_REGID_HR_BUS_ADDRESS,

    // 100 – Outputs
    NILAN_REGID_HR_OUTPUT_AIR_FLAP,
    // NILAN_REGID_HR_OUTPUT_SMOKE_FLAP,
    // NILAN_REGID_HR_OUTPUT_BYPASS_OPEN,
    // NILAN_REGID_HR_OUTPUT_BYPASS_CLOSE,
    // NILAN_REGID_HR_OUTPUT_AIR_CIRC_PUMP,
    // NILAN_REGID_HR_OUTPUT_AIR_HEAT_ALLOW,
    // NILAN_REGID_HR_OUTPUT_AIR_HEAT_1, // air heater relays
    // NILAN_REGID_HR_OUTPUT_AIR_HEAT_2,
    // NILAN_REGID_HR_OUTPUT_AIR_HEAT_3,
    NILAN_REGID_HR_OUTPUT_COMPRESSOR,
    // NILAN_REGID_HR_OUTPUT_COMPRESSOR_2,
    NILAN_REGID_HR_OUTPUT_4WAY_VALVE,
    // NILAN_REGID_HR_OUTPUT_HOTGAS_HEAT,
    // NILAN_REGID_HR_OUTPUT_HOTGAS_COOL,
    // NILAN_REGID_HR_OUTPUT_COND_OPEN,
    // NILAN_REGID_HR_OUTPUT_COND_CLOSE,
    NILAN_REGID_HR_OUTPUT_WATER_HEAT,
    // NILAN_REGID_HR_OUTPUT_3WAY_VALVE,
    //  NILAN_REGID_HR_OUTPUT_CEN_CIRC_PUMP,
    //  NILAN_REGID_HR_OUTPUT_CEN_HEAT_1,
    //  NILAN_REGID_HR_OUTPUT_CEN_HEAT_2,
    //  NILAN_REGID_HR_OUTPUT_CEN_HEAT_3,
    NILAN_REGID_HR_OUTPUT_CEN_HEAT_EXT,
    NILAN_REGID_HR_OUTPUT_USERFUNC,
    NILAN_REGID_HR_OUTPUT_USERFUNC2,
    NILAN_REGID_HR_OUTPUT_DEFROSTING,
    NILAN_REGID_HR_OUTPUT_ALARM_RELAY,
    NILAN_REGID_HR_OUTPUT_PREHEAT,

    // 200 – Analog outputs - in percent
    NILAN_REGID_HR_OUTPUT_EXHAUST_SPEED,
    NILAN_REGID_HR_OUTPUT_INLET_SPEED,
    // NILAN_REGID_HR_OUTPUT_AIR_HEAT_CAPACITY,
    // NILAN_REGID_HR_OUTPUT_CENTR_HEAT_CAPACITY,
    NILAN_REGID_HR_OUTPUT_COMPR_CAPAPACITY,
    NILAN_REGID_HR_OUTPUT_PREHEAT_CAP,

    // 3xx – Time & date
    NILAN_REGID_HR_TIME_SECOND,
    NILAN_REGID_HR_TIME_MINUTE,
    NILAN_REGID_HR_TIME_HOUR,
    NILAN_REGID_HR_TIME_DAY,
    NILAN_REGID_HR_TIME_MONTH,
    NILAN_REGID_HR_TIME_YEAR,

    // 4xx – Alarm control
    NILAN_REGID_HR_ALARM_RESET,

    // 5xx – Week program select
    NILAN_REGID_HR_PROGRAM_SELECT,

    // 60x – User function 1
    NILAN_REGID_HR_PROGRAM_USERFUNC_ACT,
    NILAN_REGID_HR_PROGRAM_USERFUNC_SET,
    NILAN_REGID_HR_PROGRAM_USER_TIME_SET,
    NILAN_REGID_HR_PROGRAM_USER_VENT_SET,
    NILAN_REGID_HR_PROGRAM_USER_TEMP_SET,
    NILAN_REGID_HR_PROGRAM_USER_OFFS_SET,

    // 61x – User function 2
    NILAN_REGID_HR_PROGRAM_USER2_FUNC_ACT,
    NILAN_REGID_HR_PROGRAM_USER2_FUNC_SET,
    NILAN_REGID_HR_PROGRAM_USER2_TIME_SET,
    NILAN_REGID_HR_PROGRAM_USER2_VENT_SET,
    NILAN_REGID_HR_PROGRAM_USER2_TEMP_SET,
    NILAN_REGID_HR_PROGRAM_USER2_OFFS_SET,

    // 700 – Data log
    // NILAN_REGID_HR_LOG_INTERVAL,

    // 1xxx – Control (setpoints)
    // NILAN_REGID_HR_CONTROL_TYPE, // applicable - but no reason to change the machine type.
    NILAN_REGID_HR_CONTROL_RUN_SET,
    NILAN_REGID_HR_CONTROL_MODE_SET,
    NILAN_REGID_HR_CONTROL_VENT_SET,
    NILAN_REGID_HR_CONTROL_TEMP_SET,
    NILAN_REGID_HR_CONTROL_SERVICE_MODE, // SERVICE MODE SELECT
    NILAN_REGID_HR_CONTROL_SERVICE_PCT,  // SERVICE MODE CAPACITY SETPOINT
                                         // NILAN_REGID_HR_CONTROL_PRESET,          // factory restore

    // 11xx – AirFlow settings
    NILAN_REGID_HR_AIRFLOW_AIR_EXCH_MODE,
    NILAN_REGID_HR_AIRFLOW_COOL_VENT,
    // NILAN_REGID_HR_AIRFLOW_TEST_SELECT,
    // NILAN_REGID_HR_AIRFLOW_LAST_TEST_DAY,
    // NILAN_REGID_HR_AIRFLOW_TEST_STATE,
    // NILAN_REGID_HR_AIRFLOW_FILT_ALM_TYPE,

    // 1200 – AirTemp settings
    NILAN_REGID_HR_AIRTEMP_COOL_SET,
    NILAN_REGID_HR_AIRTEMP_TEMP_MIN_SUM,
    NILAN_REGID_HR_AIRTEMP_TEMP_MIN_WIN,
    NILAN_REGID_HR_AIRTEMP_TEMP_MAX_SUM,
    NILAN_REGID_HR_AIRTEMP_TEMP_MAX_WIN,
    NILAN_REGID_HR_AIRTEMP_TEMP_SUMMER,
    NILAN_REGID_HR_AIRTEMP_NIGHT_DAY_LIM,
    NILAN_REGID_HR_AIRTEMP_NIGHT_SET,
    // NILAN_REGID_HR_AIRTEMP_SENSOR_SELECT,

    // 1700 – HotWater settings
    NILAN_REGID_HR_HOTWATER_TEMPSET_TOP_ELEC,     // T11
    NILAN_REGID_HR_HOTWATER_TEMPSET_BOTTOM_COMPR, // T12

    // 1800 – Central heat settings
    NILAN_REGID_HR_CENTHEAT_HEAT_EXTERN,

    // 1910 – AirQual humidity
    NILAN_REGID_HR_AIRQUAL_RH_VENT_LO,
    NILAN_REGID_HR_AIRQUAL_RH_VENT_HI,
    NILAN_REGID_HR_AIRQUAL_RH_LIM_LO,
    NILAN_REGID_HR_AIRQUAL_RH_TIMEOUT,

    // // 1920 – AirQual CO2
    // NILAN_REGID_HR_AIRQUAL_CO2_VENT_HI,
    // NILAN_REGID_HR_AIRQUAL_CO2_LIM_LO,
    // NILAN_REGID_HR_AIRQUAL_CO2_LIM_HI,

    // 2000 – User panel
    NILAN_REGID_HR_USER_MENU_OPEN, // 2002

    // 2100 – PreHeat control
    // NILAN_REGID_HR_PREHEAT_BLOCK,

    // 2200 – DPT control
    // NILAN_REGID_HR_DPT_DO_CALIBRATE,

    // Number of implemented registers
    NILAN_REGID_COUNT

} nilan_reg_id_t;

// This enumeration is used for defining how to handle the data from each register.
typedef enum
{
    NILAN_DTYPE_TEMP_Cx100, // signed, °C * 100
    NILAN_DTYPE_UINT16,
    NILAN_DTYPE_INT16,
    NILAN_DTYPE_ENUM16,

    // later: bitfield, 32-bit, etc.
} nilan_data_type_t;

// Set up a struct definition for holding the values read from each register, and a timestamp.
typedef struct
{
    uint16_t raw;          // last raw 16-bit Modbus value
    uint32_t timestamp_ms; // 0 if never updated
    uint8_t valid;         // 0 = never / invalid, 1 = OK
} nilan_reg_state_t;

// Declare the value array.
extern nilan_reg_state_t nilan_reg_state[NILAN_REGID_COUNT];

//////////////////////////////////////////////////

// This is the structure definition, for holding the meta-data for each register.
typedef struct
{
    uint16_t addr;               // Modbus register
    uint8_t reg_type;            // 3 = holding, 4 = input
    nilan_data_type_t data_type; // how to interpret raw value
    const char *name;            // "Tank top T11", for UI
} nilan_reg_meta_t;

extern const nilan_reg_meta_t nilan_registers[NILAN_REGID_COUNT];

// Update a contiguous block [start_addr ... start_addr + qty - 1] into the state array.
void nilan_update_state_range(uint8_t reg_type,
                              uint16_t start_addr,
                              uint16_t qty,
                              const uint16_t *regs,
                              uint32_t timestamp_ms);


// Init lookup tables
void nilan_registers_init();

#endif /* NILAN_REGISTERS_H */