

#include "NilanRegisters.h"

#define NILAN_MAX_REG_ADDR 2002 // Largest register address in nilan_registers

// address -> reg-id maps, 0xFFFF = "no register here"
static uint16_t input_map[NILAN_MAX_REG_ADDR + 1];
static uint16_t holding_map[NILAN_MAX_REG_ADDR + 1];

nilan_reg_state_t nilan_reg_state[NILAN_REGID_COUNT] = {0};

void nilan_update_state_range(uint8_t reg_type,
                              uint16_t start_addr,
                              uint16_t qty,
                              const uint16_t *regs,
                              uint32_t timestamp_ms)
{
    uint16_t end_addr = start_addr + qty; // exclusive

    for (int id = 0; id < NILAN_REGID_COUNT; ++id)
    {
        const nilan_reg_meta_t *meta = &nilan_registers[id];

        if (meta->reg_type != reg_type)
        {
            continue;
        }

        if (meta->addr < start_addr || meta->addr >= end_addr)
        {
            continue;
        }

        uint16_t offset = meta->addr - start_addr;

        nilan_reg_state[id].raw = regs[offset];
        nilan_reg_state[id].timestamp_ms = timestamp_ms;
        nilan_reg_state[id].valid = 1;
    }
}

// Define the array for holding the metadata of each register we want to implement control of.
const nilan_reg_meta_t nilan_registers[NILAN_REGID_COUNT] = {

    // =========================================================
    // INPUT REGISTERS (function 0x04)
    // =========================================================

    // -------------- 00x Software version ------------------
    [NILAN_REGID_IR_BUS_VERSION] = {
        .addr = 0,
        .reg_type = NILAN_INPUT_REG,
        .data_type = NILAN_DTYPE_UINT16,
        .name = "Protocol version"},

    [NILAN_REGID_IR_APP_VERSION_MAJOR] = {.addr = 1, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "SW version - major"},

    [NILAN_REGID_IR_APP_VERSION_MINOR] = {.addr = 2, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "SW version - minor"},

    [NILAN_REGID_IR_APP_VERSION_RELEASE] = {.addr = 3, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "SW version - release"},

    // -------------- 1xx Discrete inputs -------------------
    [NILAN_REGID_IR_INPUT_USERFUNC] = {.addr = 100, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "User function"},

    [NILAN_REGID_IR_INPUT_AIRFILTER_ALARM] = {.addr = 101, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Air filter alarm"},

    [NILAN_REGID_IR_INPUT_SMOKE_ALARM] = {.addr = 103, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Smoke alarm"},

    // [NILAN_REGID_IR_INPUT_MOTOR_THERMO_ALARM] = {
    //     .addr      = 104,
    //     .reg_type  = NILAN_INPUT_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Input: Motor thermo alarm"
    // },

    [NILAN_REGID_IR_INPUT_FROST_OVERHT] = {.addr = 105, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Heating surface frost/overheat"},

    [NILAN_REGID_IR_INPUT_AIRFLOW_MONITOR] = {.addr = 106, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Airflow monitor"},

    [NILAN_REGID_IR_INPUT_HI_PRESSURE_SWITCH] = {.addr = 107, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "High pressure switch"},

    // [NILAN_REGID_IR_INPUT_LO_PRESSURE_SWITCH] = {
    //     .addr      = 108,
    //     .reg_type  = NILAN_INPUT_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "LO pressure switch"
    // },

    [NILAN_REGID_IR_INPUT_BOILING] = {.addr = 109, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Hot water boiling"},

    [NILAN_REGID_IR_INPUT_DEFROST_THERMOSTAT] = {.addr = 112, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Defrost thermostat"},

    [NILAN_REGID_IR_INPUT_USERFUNC2] = {.addr = 113, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "User function 2"},

    // -------------- 2xx Temps / RH ------------------------
    [NILAN_REGID_IR_T0_CONTROLLER] = {.addr = 200, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Controller board"},

    [NILAN_REGID_IR_T2_INLET] = {.addr = 202, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Inlet preheater"},

    [NILAN_REGID_IR_T5_CONDENSOR] = {.addr = 205, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Condenser"},

    [NILAN_REGID_IR_T6_EVAPORATOR] = {.addr = 206, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Evaporator"},

    [NILAN_REGID_IR_T7_INLET_POSTHEATER] = {.addr = 207, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Inlet postheater"},

    [NILAN_REGID_IR_T8_OUTDOOR] = {.addr = 208, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Outdoor"},

    [NILAN_REGID_IR_T10_EXTERN_ROOM] = {.addr = 210, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "External room"},

    [NILAN_REGID_IR_T11_TANK_TOP] = {.addr = 211, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Tank top"},

    [NILAN_REGID_IR_T12_TANK_BOTTOM] = {.addr = 212, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Tank bottom"},

    [NILAN_REGID_IR_T15_ROOM_PANEL] = {.addr = 215, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Room (panel)"},

    [NILAN_REGID_IR_T16_AUX] = {.addr = 216, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "AUX / anode"},

    [NILAN_REGID_IR_HUMIDITY] = {.addr = 221, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Humidity"},

    // -------------- 4xx Alarm list ------------------------
    // 400 = alram state bit mask. 0x80: active alarms. 0x03: # of alarms
    [NILAN_REGID_IR_ALARM_STATUS] = {.addr = 400, .reg_type = NILAN_INPUT_REG,
                                     .data_type = NILAN_DTYPE_UINT16, // bitmask
                                     .name = "Alarm status"},

    [NILAN_REGID_IR_ALARM_LIST1_ID] = {.addr = 401, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 1 ID"},

    [NILAN_REGID_IR_ALARM_LIST1_DATE] = {.addr = 402, .reg_type = NILAN_INPUT_REG,
                                         .data_type = NILAN_DTYPE_UINT16, // DOS date
                                         .name = "Alarm 1 date"},

    [NILAN_REGID_IR_ALARM_LIST1_TIME] = {.addr = 403, .reg_type = NILAN_INPUT_REG,
                                         .data_type = NILAN_DTYPE_UINT16, // DOS time
                                         .name = "Alarm 1 time"},

    [NILAN_REGID_IR_ALARM_LIST2_ID] = {.addr = 404, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 2 ID"},

    [NILAN_REGID_IR_ALARM_LIST2_DATE] = {.addr = 405, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 2 date"},

    [NILAN_REGID_IR_ALARM_LIST2_TIME] = {.addr = 406, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 2 time"},

    [NILAN_REGID_IR_ALARM_LIST3_ID] = {.addr = 407, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 3 ID"},

    [NILAN_REGID_IR_ALARM_LIST3_DATE] = {.addr = 408, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 3 date"},

    [NILAN_REGID_IR_ALARM_LIST3_TIME] = {.addr = 409, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm 3 time"},

    // ------------- 1000 Operation/Control  ----------------
    [NILAN_REGID_IR_CONTROL_RUNNING] = {.addr = 1000, .reg_type = NILAN_INPUT_REG,
                                        .data_type = NILAN_DTYPE_UINT16, // 0=Off,1=On
                                        .name = "Power State"},

    [NILAN_REGID_IR_CONTROL_MODE] = {.addr = 1001, .reg_type = NILAN_INPUT_REG,
                                     .data_type = NILAN_DTYPE_ENUM16, // use control_modes[]
                                     .name = "Operation Mode"},

    [NILAN_REGID_IR_CONTROL_STATE] = {.addr = 1002, .reg_type = NILAN_INPUT_REG,
                                      .data_type = NILAN_DTYPE_ENUM16, // use control_states[]
                                      .name = "Control State"},

    [NILAN_REGID_IR_CONTROL_SEC_IN_STATE] = {.addr = 1003, .reg_type = NILAN_INPUT_REG,
                                             .data_type = NILAN_DTYPE_UINT16, // seconds in state
                                             .name = "Seconds in state"},

    // -------------- 1100 Airflow (status) -----------------
    [NILAN_REGID_IR_AIRFLOW_VENT_SETPOINT] = {.addr = 1100, .reg_type = NILAN_INPUT_REG,
                                              .data_type = NILAN_DTYPE_UINT16, // 0..4
                                              .name = "Ventilation Fan Setpoint"},

    [NILAN_REGID_IR_AIRFLOW_INLET_FAN_STEP] = {.addr = 1101, .reg_type = NILAN_INPUT_REG,
                                               .data_type = NILAN_DTYPE_UINT16, // 0..4
                                               .name = "Inlet Fan Setpoint"},

    [NILAN_REGID_IR_AIRFLOW_EXHAUST_FAN_STEP] = {.addr = 1102, .reg_type = NILAN_INPUT_REG,
                                                 .data_type = NILAN_DTYPE_UINT16, // 0..4
                                                 .name = "Exhaust Fan Setpoint"},

    [NILAN_REGID_IR_AIRFLOW_DAYS_SINCE_FILTER_CHG] = {.addr = 1103, .reg_type = NILAN_INPUT_REG,
                                                      .data_type = NILAN_DTYPE_UINT16, // days
                                                      .name = "Days since filter change"},

    [NILAN_REGID_IR_AIRFLOW_DAYS_TO_FILTER_CHG] = {.addr = 1104, .reg_type = NILAN_INPUT_REG,
                                                   .data_type = NILAN_DTYPE_UINT16, // days
                                                   .name = "Days to filter change"},

    // -------------- 1200 AirTemp (status) -----------------
    [NILAN_REGID_IR_AIRTEMP_SUMMER_STATE] = {.addr = 1200, .reg_type = NILAN_INPUT_REG,
                                             .data_type = NILAN_DTYPE_UINT16, // 0/1
                                             .name = "Summer state"},

    [NILAN_REGID_IR_AIRTEMP_TEMP_INLET_SETPT] = {.addr = 1201, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Inlet temperature setpoint"},

    [NILAN_REGID_IR_AIRTEMP_TEMP_CONTROL] = {.addr = 1202, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Controlled temperature"},

    [NILAN_REGID_IR_AIRTEMP_TEMP_ROOM] = {.addr = 1203, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "UserPanel temperature"},

    // [NILAN_REGID_IR_AIRTEMP_EFF_PCT] = {
    //     .addr      = 1204,
    //     .reg_type  = NILAN_INPUT_REG,
    //     .data_type = NILAN_DTYPE_UINT16,   // % *100
    //     .name      = "Heat exchanger efficiency"
    // },

    [NILAN_REGID_IR_AIRTEMP_CAP_SET] = {.addr = 1205, .reg_type = NILAN_INPUT_REG,
                                        .data_type = NILAN_DTYPE_UINT16, // % *100
                                        .name = "Capacity setpoint"},

    [NILAN_REGID_IR_AIRTEMP_CAP_ACT] = {.addr = 1206, .reg_type = NILAN_INPUT_REG,
                                        .data_type = NILAN_DTYPE_UINT16, // % *100
                                        .name = "Capacity (actual)"},

    // -------------- 1800 Central heating (status) ---------
    [NILAN_REGID_IR_CENTHEAT_HEAT_EXT_SETPOINT] = {.addr = 1800, .reg_type = NILAN_INPUT_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "External Heat Source Setpoint"},

    // =========================================================
    // HOLDING REGISTERS (function 0x03 / 0x10)
    // =========================================================

    // -------------- 00x Device / protocol -----------------
    [NILAN_REGID_HR_BUS_ADDRESS] = {.addr = 50, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Bus address"},

    // -------------- 1xx Digital outputs -------------------
    [NILAN_REGID_HR_OUTPUT_AIR_FLAP] = {.addr = 100, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Air flap"},

    // [NILAN_REGID_HR_OUTPUT_AIR_CIRC_PUMP] = {
    //     .addr      = 104,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Circulation Pump"
    // },

    // [NILAN_REGID_HR_OUTPUT_AIR_HEAT_ALLOW] = {
    //     .addr      = 105,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Air Heating Selected"
    // },

    // [NILAN_REGID_HR_OUTPUT_AIR_HEAT_1] = {
    //     .addr      = 106,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Out: Air heat 1"
    // },

    // [NILAN_REGID_HR_OUTPUT_AIR_HEAT_2] = {
    //     .addr      = 107,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Out: Air heat 2"
    // },

    // [NILAN_REGID_HR_OUTPUT_AIR_HEAT_3] = {
    //     .addr      = 108,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Out: Air heat 3"
    // },

    [NILAN_REGID_HR_OUTPUT_COMPRESSOR] = {.addr = 109, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Compressor"},

    [NILAN_REGID_HR_OUTPUT_4WAY_VALVE] = {.addr = 111, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "4-way Valve"},

    [NILAN_REGID_HR_OUTPUT_WATER_HEAT] = {.addr = 116, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Water heating"},

    [NILAN_REGID_HR_OUTPUT_CEN_HEAT_EXT] = {.addr = 122, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "External radiator heating"},

    [NILAN_REGID_HR_OUTPUT_USERFUNC] = {.addr = 123, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "User Function Active"},

    [NILAN_REGID_HR_OUTPUT_USERFUNC2] = {.addr = 124, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "User Function 2 Active"},

    [NILAN_REGID_HR_OUTPUT_DEFROSTING] = {.addr = 125, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Defrost Active"},

    [NILAN_REGID_HR_OUTPUT_ALARM_RELAY] = {.addr = 126, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm Relay"},

    [NILAN_REGID_HR_OUTPUT_PREHEAT] = {.addr = 127, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Preheater"},

    // -------------- 2xx Analog outputs --------------------
    [NILAN_REGID_HR_OUTPUT_EXHAUST_SPEED] = {.addr = 200, .reg_type = NILAN_HOLDING_REG,
                                             .data_type = NILAN_DTYPE_UINT16, // % *100
                                             .name = "Exhaust Fan speed"},

    [NILAN_REGID_HR_OUTPUT_INLET_SPEED] = {.addr = 201, .reg_type = NILAN_HOLDING_REG,
                                           .data_type = NILAN_DTYPE_UINT16, // % *100
                                           .name = "Inlet Fan Speed"},

    // [NILAN_REGID_HR_OUTPUT_AIR_HEAT_CAPACITY] = {
    //     .addr      = 202,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Air Heater Capacity"
    // },

    // [NILAN_REGID_HR_OUTPUT_CENTR_HEAT_CAPACITY] = {
    //     .addr      = 203,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "Out: Central heat capacity"
    // },

    [NILAN_REGID_HR_OUTPUT_COMPR_CAPAPACITY] = {.addr = 204, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Compressor capacity"},

    [NILAN_REGID_HR_OUTPUT_PREHEAT_CAP] = {.addr = 205, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Preheat capacity"},

    // -------------- 3xx Time & date -----------------------
    [NILAN_REGID_HR_TIME_SECOND] = {.addr = 300, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Time second"},

    [NILAN_REGID_HR_TIME_MINUTE] = {.addr = 301, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Time minute"},

    [NILAN_REGID_HR_TIME_HOUR] = {.addr = 302, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Time hour"},

    [NILAN_REGID_HR_TIME_DAY] = {.addr = 303, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Date day"},

    [NILAN_REGID_HR_TIME_MONTH] = {.addr = 304, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Date month"},

    [NILAN_REGID_HR_TIME_YEAR] = {.addr = 305, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Date year"},

    // -------------- 4xx Alarm control ---------------------
    [NILAN_REGID_HR_ALARM_RESET] = {.addr = 400, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Alarm reset"},

    // -------------- 50x Week program select ---------------
    [NILAN_REGID_HR_PROGRAM_SELECT] = {.addr = 500, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_ENUM16, .name = "Program select"},

    // -------------- 60x User function 1 -------------------
    [NILAN_REGID_HR_PROGRAM_USERFUNC_ACT] = {.addr = 600, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "UserFunc1 active"},

    [NILAN_REGID_HR_PROGRAM_USERFUNC_SET] = {.addr = 601, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_ENUM16, .name = "UserFunc1 select"},

    [NILAN_REGID_HR_PROGRAM_USER_TIME_SET] = {.addr = 602, .reg_type = NILAN_HOLDING_REG,
                                              .data_type = NILAN_DTYPE_UINT16, // minutes
                                              .name = "UserFunc1 time"},

    [NILAN_REGID_HR_PROGRAM_USER_VENT_SET] = {
        .addr = 603, .reg_type = NILAN_HOLDING_REG,
        .data_type = NILAN_DTYPE_UINT16, // step
        .name = "UserFunc1 vent step"    // Extend function...)
    },

    [NILAN_REGID_HR_PROGRAM_USER_TEMP_SET] = {.addr = 604, .reg_type = NILAN_HOLDING_REG,
                                              .data_type = NILAN_DTYPE_INT16, // °C (not x100)
                                              .name = "UserFunc1 temp"},

    [NILAN_REGID_HR_PROGRAM_USER_OFFS_SET] = {.addr = 605, .reg_type = NILAN_HOLDING_REG,
                                              .data_type = NILAN_DTYPE_INT16, // °C offset
                                              .name = "UserFunc1 offset"},

    // -------------- 61x User function 2 -------------------
    [NILAN_REGID_HR_PROGRAM_USER2_FUNC_ACT] = {.addr = 610, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "UserFunc2 active"},

    [NILAN_REGID_HR_PROGRAM_USER2_FUNC_SET] = {.addr = 611, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_ENUM16, .name = "UserFunc2 select"},

    [NILAN_REGID_HR_PROGRAM_USER2_TIME_SET] = {.addr = 612, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "UserFunc2 time"},

    [NILAN_REGID_HR_PROGRAM_USER2_VENT_SET] = {.addr = 613, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "UserFunc2 vent step"},

    [NILAN_REGID_HR_PROGRAM_USER2_TEMP_SET] = {.addr = 614, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_INT16, .name = "UserFunc2 temp"},

    [NILAN_REGID_HR_PROGRAM_USER2_OFFS_SET] = {.addr = 615, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_INT16, .name = "UserFunc2 offset"},

    // -------------- 1000 Control (setpoints) --------------
    [NILAN_REGID_HR_CONTROL_RUN_SET] = {.addr = 1001, .reg_type = NILAN_HOLDING_REG,
                                        .data_type = NILAN_DTYPE_UINT16, // 0/1
                                        .name = "Power"},

    [NILAN_REGID_HR_CONTROL_MODE_SET] = {.addr = 1002, .reg_type = NILAN_HOLDING_REG,
                                         .data_type = NILAN_DTYPE_ENUM16, // control_modes[]
                                         .name = "Operation mode"},

    [NILAN_REGID_HR_CONTROL_VENT_SET] = {.addr = 1003, .reg_type = NILAN_HOLDING_REG,
                                         .data_type = NILAN_DTYPE_UINT16, // 0..4
                                         .name = "Ventilation step"},

    [NILAN_REGID_HR_CONTROL_TEMP_SET] = {.addr = 1004, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Temperature setpoint"},

    [NILAN_REGID_HR_CONTROL_SERVICE_MODE] = {.addr = 1005, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_ENUM16, .name = "Service mode"},

    [NILAN_REGID_HR_CONTROL_SERVICE_PCT] = {.addr = 1006, .reg_type = NILAN_HOLDING_REG,
                                            .data_type = NILAN_DTYPE_UINT16, // % *100
                                            .name = "Service mode capacity"},

    // -------------- 1100 AirFlow settings -----------------
    [NILAN_REGID_HR_AIRFLOW_AIR_EXCH_MODE] = {.addr = 1100, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_ENUM16, .name = "Air exchange mode"},

    [NILAN_REGID_HR_AIRFLOW_COOL_VENT] = {.addr = 1101, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "Cooling vent step"},

    // [NILAN_REGID_HR_AIRFLOW_TEST_SELECT] = {
    //     .addr      = 1102,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_ENUM16,
    //     .name      = "Air damper test day"
    // },

    // [NILAN_REGID_HR_AIRFLOW_LAST_TEST_DAY] = {
    //     .addr      = 1103,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,   // DOS date
    //     .name      = "Last test date"
    // },

    // [NILAN_REGID_HR_AIRFLOW_TEST_STATE] = {
    //     .addr      = 1104,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_ENUM16,
    //     .name      = "Test state"
    // },

    // [NILAN_REGID_HR_AIRFLOW_FILT_ALM_TYPE] = {
    //     .addr      = 1105,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_ENUM16,
    //     .name      = "Filter alarm type"
    // },

    // -------------- 1200 AirTemp settings -----------------
    [NILAN_REGID_HR_AIRTEMP_COOL_SET] = {.addr = 1200, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_ENUM16, .name = "Cooling temp setpoint"},

    [NILAN_REGID_HR_AIRTEMP_TEMP_MIN_SUM] = {.addr = 1201, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Min. summer inlet temp"},

    [NILAN_REGID_HR_AIRTEMP_TEMP_MIN_WIN] = {.addr = 1202, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Min. winter inlet temp"},

    [NILAN_REGID_HR_AIRTEMP_TEMP_MAX_SUM] = {.addr = 1203, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "max. summer inlet temp"},

    [NILAN_REGID_HR_AIRTEMP_TEMP_MAX_WIN] = {.addr = 1204, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "max. winter inlet temp"},

    [NILAN_REGID_HR_AIRTEMP_TEMP_SUMMER] = {.addr = 1205, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Summer/winter limit"},

    [NILAN_REGID_HR_AIRTEMP_NIGHT_DAY_LIM] = {.addr = 1206, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Night-cooling day limit"},

    [NILAN_REGID_HR_AIRTEMP_NIGHT_SET] = {.addr = 1207, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Night-cooling setpoint"},

    // -------------- 1700 HotWater settings ----------------
    [NILAN_REGID_HR_HOTWATER_TEMPSET_TOP_ELEC] = {.addr = 1700, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Tank Top setpoint (elec.)"},

    [NILAN_REGID_HR_HOTWATER_TEMPSET_BOTTOM_COMPR] = {.addr = 1701, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Tank Bottom setpoint (compr.)"},

    // -------------- 1800 Central heat settings -----------
    [NILAN_REGID_HR_CENTHEAT_HEAT_EXTERN] = {.addr = 1800, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_TEMP_Cx100, .name = "Ext. heat offset temp setpoint"},

    // -------------- 1910 AirQual humidity -----------------
    [NILAN_REGID_HR_AIRQUAL_RH_VENT_LO] = {.addr = 1910, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "humidity low winter step"},

    [NILAN_REGID_HR_AIRQUAL_RH_VENT_HI] = {.addr = 1911, .reg_type = NILAN_HOLDING_REG, .data_type = NILAN_DTYPE_UINT16, .name = "humidity high step"},

    [NILAN_REGID_HR_AIRQUAL_RH_LIM_LO] = {.addr = 1912, .reg_type = NILAN_HOLDING_REG,
                                          .data_type = NILAN_DTYPE_UINT16, // % *100
                                          .name = "Humidity limit for low vent"},

    [NILAN_REGID_HR_AIRQUAL_RH_TIMEOUT] = {.addr = 1913, .reg_type = NILAN_HOLDING_REG,
                                           .data_type = NILAN_DTYPE_UINT16, // minutes
                                           .name = "Humidity high vent max time"},

    // // -------------- 1920 AirQual CO2 ----------------------
    // [NILAN_REGID_HR_AIRQUAL_CO2_VENT_HI] = {
    //     .addr      = 1920,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,
    //     .name      = "CO2 high vent step"
    // },

    // [NILAN_REGID_HR_AIRQUAL_CO2_LIM_LO] = {
    //     .addr      = 1921,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,   // ppm
    //     .name      = "CO2 low limit"
    // },

    // [NILAN_REGID_HR_AIRQUAL_CO2_LIM_HI] = {
    //     .addr      = 1922,
    //     .reg_type  = NILAN_HOLDING_REG,
    //     .data_type = NILAN_DTYPE_UINT16,   // ppm
    //     .name      = "CO2 high limit"
    // },

    // -------------- 2000 User panel -----------------------
    [NILAN_REGID_HR_USER_MENU_OPEN] = {.addr = 2002, .reg_type = NILAN_HOLDING_REG,
                                       .data_type = NILAN_DTYPE_ENUM16, // 0=Closed,1=Open,2=No OFF
                                       .name = "User menu open"},
};
