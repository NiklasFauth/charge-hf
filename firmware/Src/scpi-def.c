/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scpi/scpi.h"
#include "scpi-def.h"
#include "stm32g4xx_ll_hrtim.h"
#include "main.h"

extern sepic_control_t sepic;
extern knobs_t knobs;
extern hf_control_t hf;

static scpi_result_t SEPIC_MeasureVoltageIn(scpi_t * context) {
    return SCPI_ResultDouble(context, sepic.Vin);
}

static scpi_result_t SEPIC_MeasureVoltageOut(scpi_t * context) {
    return SCPI_ResultDouble(context, sepic.Vout);
}

static scpi_result_t SEPIC_MeasureCurrentIn(scpi_t * context) {
    return SCPI_ResultDouble(context, sepic.Iin);
}

static scpi_result_t CONTROL_MeasureVoltageKnobs(scpi_t * context) {
  uint32_t param;

  /* read first parameter if present */
  if (!SCPI_ParamUInt32(context, &param, TRUE)) {
      return SCPI_RES_ERR;
  }

  if (param == 1) {
    return SCPI_ResultDouble(context, knobs.knob1);
  } else if (param == 2) {
    return SCPI_ResultDouble(context, knobs.knob2);
  } else {
    return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
  }
}

static scpi_result_t SEPIC_ConfigureVoltageOut(scpi_t * context) {
    float param;

    /* read first parameter if present */
    if (!SCPI_ParamFloat(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (param < 0) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else if (param > SEPIC_VMAX) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else {
      sepic.Vset = param;
    }

    return SCPI_RES_OK;
}

static scpi_result_t SEPIC_ConfigureCurrentIn(scpi_t * context) {
    float param;

    /* read first parameter if present */
    if (!SCPI_ParamUInt32(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (param < 0) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else if (param > HF_FMAX) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else {
      sepic.Iset = param;
    }

    return SCPI_RES_OK;
}

static scpi_result_t HF_ConfigureFrequency(scpi_t * context) {
    uint32_t param;

    /* read first parameter if present */
    if (!SCPI_ParamUInt32(context, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (param < 10000) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else if (param > HF_FMAX) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else {
      hf.freq = param;
      uint32_t period = (144000*32) / (uint32_t)(hf.freq / 1000);
      LL_HRTIM_TIM_SetPeriod(HRTIM1, LL_HRTIM_TIMER_C, period);
      LL_HRTIM_TIM_SetCompare1(HRTIM1, LL_HRTIM_TIMER_C, period / 2);
    }

    return SCPI_RES_OK;
}

static scpi_result_t HF_ConfigureDeadtime(scpi_t * context) {
    uint32_t rising, falling;

    /* read first parameter if present */
    if (!SCPI_ParamUInt32(context, &rising, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (!SCPI_ParamUInt32(context, &falling, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (rising < 0 || falling < 0) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else if (rising > 511 || falling > 511) {
      return SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
    } else {
      hf.DTrising = rising;
      hf.DTfalling = falling;
      LL_HRTIM_DT_SetRisingValue(HRTIM1, LL_HRTIM_TIMER_C, rising);
      LL_HRTIM_DT_SetFallingValue(HRTIM1, LL_HRTIM_TIMER_C, falling);
    }

    return SCPI_RES_OK;
}


static scpi_result_t TEST_Bool(scpi_t * context) {
    scpi_bool_t param1;
    char _err[150] = {0};

    /* read first parameter if present */
    if (!SCPI_ParamBool(context, &param1, TRUE)) {
        return SCPI_RES_ERR;
    }

    sprintf(_err, "TEST:BOOL\r\n\tP1=%d\r\n", param1);
    CDC_Transmit_FS(_err, 150);

    return SCPI_RES_OK;
}

scpi_choice_def_t trigger_source[] = {
    {"BUS", 5},
    {"IMMediate", 6},
    {"EXTernal", 7},
    SCPI_CHOICE_LIST_END /* termination of option list */
};

static scpi_result_t TEST_ChoiceQ(scpi_t * context) {
    char _err[150] = {0};
    int32_t param;
    const char * name;

    if (!SCPI_ParamChoice(context, trigger_source, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    SCPI_ChoiceToName(trigger_source, param, &name);

    sprintf(_err, "\tP1=%s (%ld)\r\n", name, (long int) param);
    CDC_Transmit_FS(_err, 150);

    SCPI_ResultInt32(context, param);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_Numbers(scpi_t * context) {
    int32_t numbers[2];
    char _err[150] = {0};

    SCPI_CommandNumbers(context, numbers, 2, 1);

    sprintf(_err, "TEST numbers %d %d\r\n", numbers[0], numbers[1]);
    CDC_Transmit_FS(_err, 150);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_Text(scpi_t * context) {
    char buffer[100];
    char _err[150] = {0};
    size_t copy_len;

    if (!SCPI_ParamCopyText(context, buffer, sizeof(buffer), &copy_len, FALSE)) {
        buffer[0] = '\0';
    }

    sprintf(_err, "TEXT: ***%s***\r\n", buffer);
    CDC_Transmit_FS(_err, 150);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_ArbQ(scpi_t * context) {
    const char * data;
    size_t len;

    if (SCPI_ParamArbitraryBlock(context, &data, &len, FALSE)) {
        SCPI_ResultArbitraryBlock(context, data, len);
    }

    return SCPI_RES_OK;
}

/**
 * Reimplement IEEE488.2 *TST?
 *
 * Result should be 0 if everything is ok
 * Result should be 1 if something goes wrong
 *
 * Return SCPI_RES_OK
 */
static scpi_result_t My_CoreTstQ(scpi_t * context) {

    SCPI_ResultInt32(context, 0);

    return SCPI_RES_OK;
}


const scpi_command_t scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    { .pattern = "*CLS", .callback = SCPI_CoreCls,},
    { .pattern = "*ESE", .callback = SCPI_CoreEse,},
    { .pattern = "*ESE?", .callback = SCPI_CoreEseQ,},
    { .pattern = "*ESR?", .callback = SCPI_CoreEsrQ,},
    { .pattern = "*IDN?", .callback = SCPI_CoreIdnQ,},
    { .pattern = "*OPC", .callback = SCPI_CoreOpc,},
    { .pattern = "*OPC?", .callback = SCPI_CoreOpcQ,},
    { .pattern = "*RST", .callback = SCPI_CoreRst,},
    { .pattern = "*SRE", .callback = SCPI_CoreSre,},
    { .pattern = "*SRE?", .callback = SCPI_CoreSreQ,},
    { .pattern = "*STB?", .callback = SCPI_CoreStbQ,},
    { .pattern = "*TST?", .callback = My_CoreTstQ,},
    { .pattern = "*WAI", .callback = SCPI_CoreWai,},

    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {.pattern = "SYSTem:ERRor[:NEXT]?", .callback = SCPI_SystemErrorNextQ,},
    {.pattern = "SYSTem:ERRor:COUNt?", .callback = SCPI_SystemErrorCountQ,},
    {.pattern = "SYSTem:VERSion?", .callback = SCPI_SystemVersionQ,},

    /* {.pattern = "STATus:OPERation?", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:EVENt?", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:CONDition?", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:ENABle", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:ENABle?", .callback = scpi_stub_callback,}, */

    {.pattern = "STATus:QUEStionable[:EVENt]?", .callback = SCPI_StatusQuestionableEventQ,},
    /* {.pattern = "STATus:QUEStionable:CONDition?", .callback = scpi_stub_callback,}, */
    {.pattern = "STATus:QUEStionable:ENABle", .callback = SCPI_StatusQuestionableEnable,},
    {.pattern = "STATus:QUEStionable:ENABle?", .callback = SCPI_StatusQuestionableEnableQ,},

    {.pattern = "STATus:PRESet", .callback = SCPI_StatusPreset,},

    /* SEPIC */
    {.pattern = "MEASure:VOLTage:DC:INput?", .callback = SEPIC_MeasureVoltageIn,},
    {.pattern = "MEASure:VOLTage:DC:OUTput?", .callback = SEPIC_MeasureVoltageOut,},
    {.pattern = "CONFigure:VOLTage:DC:OUTput", .callback = SEPIC_ConfigureVoltageOut,},
    {.pattern = "MEASure:CURRent:DC:INput?", .callback = SEPIC_MeasureCurrentIn,},
    {.pattern = "CONFigure:CURRent:DC:INput", .callback = SEPIC_ConfigureCurrentIn,},

    /* HF AMP */
    {.pattern = "CONFigure:FREQuency", .callback = HF_ConfigureFrequency,},
    {.pattern = "CONFigure:DEADtime", .callback = HF_ConfigureDeadtime,},
    {.pattern = "MEASure:CURRent:AC?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:RESistance?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:FRESistance?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:FREQuency?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:PERiod?", .callback = SCPI_StubQ,},

    /* Knobs */
    {.pattern = "MEASure:VOLTage:DC:CONTrol?", .callback = CONTROL_MeasureVoltageKnobs,},

    //{.pattern = "TEST:BOOL", .callback = TEST_Bool,},
    //{.pattern = "TEST:CHOice?", .callback = TEST_ChoiceQ,},
    //{.pattern = "TEST#:NUMbers#", .callback = TEST_Numbers,},
    //{.pattern = "TEST:TEXT", .callback = TEST_Text,},
    //{.pattern = "TEST:ARBitrary?", .callback = TEST_ArbQ,},

    SCPI_CMD_LIST_END
};

scpi_interface_t scpi_interface = {
    .error = SCPI_Error,
    .write = SCPI_Write,
    .control = SCPI_Control,
    .flush = SCPI_Flush,
    .reset = SCPI_Reset,
};

char scpi_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];

scpi_t scpi_context;
