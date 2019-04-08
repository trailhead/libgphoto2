#define ARSENAL_DEBUG_FNUMBER
#define ARSENAL_DEBUG_ISO
#define ARSENAL_DEBUG_EXPOSURE

typedef struct {
  uint16_t dividend;                  // Ex: 1/10 sec, dividend is 10
  uint16_t divisor;                   // Ex: 1/10 sec, divisor is 1
} uint16_fraction;

typedef union {
  uint16_fraction u16_fraction;
  uint32_t         u32;
  float            f;
} step_value;

typedef struct sony_update_config_info sony_update_config_info; 

typedef struct sony_update_config_info {
  Camera *camera;                      // Camera struct passed in with CONFIG_PUT_ARGS
  PTPDevicePropDesc dpd;              // DPD for this single property also from CONFIG_PUT_ARGS
  uint16_t propcode;                  // PTP property code such as PTP_DPC_FNumber

  // Unions containing two 16 bit or a single 32 bit value
  // These numbers represent the real world values.  Ex: current.f = 5.6F
  step_value target;
  step_value current;
  step_value last;

  uint8_t complete;

  // Step counts in terms of up/down steps
  int32_t stepsCalc;                  // Positive or negative steps for up or down
  int32_t stepsLastCalc;              // Calc steps from the last iteration
  int32_t stepsRemain;                // Unsigned remaining steps until target
  int32_t stepsTotalSent;

  // Time values
  double stepDelay;                    // Minimum time delay between steps
  double lastChange;
  double changeErrorTimeout;           // No change for x seconds times out
  double changeRecalcBaseTimeout;     // The quickest we can expect an F step to complete
  double changeRecalcPerStepTimeout;  //
  double loopStartTime;

  int (*get_current_value)(sony_update_config_info *pInfo);
  int (*calculate_steps)(sony_update_config_info *pInfo);
  uint8_t (*check_timeout)(sony_update_config_info *pInfo);
  uint8_t (*check_complete)(sony_update_config_info *pInfo);
} sony_update_config_info;

static int _sony_multiple_update_loop(sony_update_config_info *pInfo, uint8_t count);

static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child);

void _sony_config_f_number_struct(sony_update_config_info *pInfo, Camera *camera, float fTarget);
void _sony_config_iso_struct(sony_update_config_info *pInfo, Camera *camera, uint32_t isoTarget);
void _sony_config_shutter_struct(sony_update_config_info *pInfo, Camera *camera, uint16_t dividend, uint16_t divisor);

static int _sony_get_current_f_number(sony_update_config_info *pInfo);
static int _sony_get_current_iso(sony_update_config_info *pInfo);
static int _sony_get_current_shutter(sony_update_config_info *pInfo);

static int _sony_calc_f_number_steps(sony_update_config_info *pInfo);
static int _sony_calc_iso_steps(sony_update_config_info *pInfo);
static int _sony_calc_shutter_steps(sony_update_config_info *pInfo);

static uint8_t _sony_check_f_number_timeout(sony_update_config_info *pInfo);
static uint8_t _sony_check_iso_timeout(sony_update_config_info *pInfo);
static uint8_t _sony_check_shutter_timeout(sony_update_config_info *pInfo);

static uint8_t _sony_check_f_number_complete(sony_update_config_info *pInfo);
static uint8_t _sony_check_iso_complete(sony_update_config_info *pInfo);
static uint8_t _sony_check_shutter_complete(sony_update_config_info *pInfo);

static int
_get_Sony_F_ISO_Exp(CONFIG_GET_ARGS) {

  char value[64] = "Unsupported.  Read individual configs.";

  gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
  gp_widget_set_name (*widget, menu->name);

  gp_widget_set_value (*widget,value);

  return GP_OK;
}

static int
_put_Sony_F_ISO_Exp(CONFIG_PUT_ARGS) {

  // Put args:
  // Camera *camera, CameraWidget *widget, PTPPropertyValue *propval, PTPDevicePropDesc *dpd

  sony_update_config_info param_info[3];

  char    *val;

  int8_t i;
  char *fnumberstr;
  char *isostr;
  char *exposurestr;
  char *pos;
  uint32_t commacount                   = 0;

  float fTarget;
  uint32_t isoTarget;
  int expTargetDividend                 = 0;
  int expTargetDivisor                  = 1;

  struct timeval tv;
  double startTime, endTime;

  PTPParams    *params = &(camera->pl->params);
  GPContext     *context = ((PTPData *) params->data)->context;
  uint32_t ret;

  // Get the new value as a string containing all 3 parameters
  CR (gp_widget_get_value(widget, &val));

  pos = val;
  do {
    if (*pos == ',') {
      commacount++;
    }
  } while (*pos++ != 0);

  if (commacount != 2) {
    gp_context_error (context, _("Could not parse 3 parameters (f,iso,exp)."));
    return GP_ERROR_BAD_PARAMETERS;
  }

  // F and Exposure are floats, which do not always parse exactly, so process them as strings
  // For example, sscanf parses 5.6 as 5.5999999
  exposurestr = strrchr(val, ',');
  if (exposurestr == 0) {
    gp_context_error (context, _("Could not parse 3 parameters (f,iso,exp)."));
    return GP_ERROR_BAD_PARAMETERS;
  };
  *exposurestr = 0;
  exposurestr++;

  isostr = strrchr(val, ',');
  if (isostr == 0) {
    gp_context_error (context, _("Could not parse 3 parameters (f,iso,exp)."));
    return GP_ERROR_BAD_PARAMETERS;
  };
  *isostr = 0;
  isostr++;

  fnumberstr = val;

  if (sscanf(fnumberstr, "%f", &fTarget) != 1) {
    gp_context_error (context, _("Could not parse a numeric value for f-number."));
    return GP_ERROR_BAD_PARAMETERS;
  }

  if (sscanf(isostr, "%d", &isoTarget) != 1) {
    gp_context_error (context, _("Could not parse a numeric value for ISO."));
    return GP_ERROR_BAD_PARAMETERS;
  }

  if (sscanf(exposurestr, "%d/%d", &expTargetDividend, &expTargetDivisor) != 2 &&
      sscanf(exposurestr, "%d", &expTargetDividend) != 1) {
    gp_context_error (context, _("Could not parse a numeric value for exposure."));
    return GP_ERROR_BAD_PARAMETERS;
  }

#ifdef ARSENAL_DEBUG_FNUMBER
  printf("Target F = %lf\n", fTarget);
#endif

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("Target Exposure = %d/%d\n", expTargetDividend, expTargetDivisor);
#endif

  // Initialize all 3 parameters
  _sony_config_f_number_struct(&param_info[0], camera, fTarget);
  _sony_config_iso_struct(&param_info[1], camera, isoTarget);
  _sony_config_shutter_struct(&param_info[2], camera, expTargetDividend, expTargetDivisor);

  // Initialize timers.
  gettimeofday(&tv, NULL);
  startTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

  ret = _sony_multiple_update_loop(&param_info[0], sizeof(param_info)/sizeof(sony_update_config_info));

  gettimeofday(&tv, NULL);
  endTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

  printf("Exiting - cycle time = %lf\n", endTime - startTime);

  return ret;
}

static int
_sony_multiple_update_loop(sony_update_config_info *pInfo, uint8_t count) {

  PTPParams    *params    = NULL;
  GPContext    *context   = NULL;

  uint8_t i;
  uint8_t incomplete = FALSE;
  struct timeval tv;
	PTPPropertyValue	moveVal;
  double startTime, endTime;
  double loopStartTime, loopEndTime;

  for (i = 0; i < count; i++) {
    pInfo[i].lastChange = startTime;
  }

  do {
    for (i = 0; i < count; i++) {

      params = &(pInfo[i].camera->pl->params);
      context = ((PTPData *) params->data)->context;

      gettimeofday(&tv, NULL);
      loopStartTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

      pInfo[i].loopStartTime = loopStartTime;
      //printf("Loop %u start time = %lf\n", fStepsTotalSent,loopStartTime - startTime);

      // If target has not been reached, calculate and send more steps    
      if (pInfo[i].complete != TRUE) {
        pInfo[i].get_current_value(&pInfo[i]);

        pInfo[i].calculate_steps(&pInfo[i]);

  #ifdef ARSENAL_DEBUG_FNUMBER
        printf("Last = %d\n", pInfo[i].last.u32);
        printf("Current = %d\n", pInfo[i].current.u32);
        printf("Last change = %lf\n", (pInfo[i].lastChange == 0.0f) ? 0.0f : pInfo[i].lastChange - startTime);
        printf("Change will timeout at = %lf\n", pInfo[i].lastChange + pInfo[i].changeErrorTimeout - startTime);
        printf("stepsRemain = %u\n", pInfo[i].stepsRemain);
  #endif

        // Check to make sure we're not stuck not movie
        if (pInfo[i].stepsTotalSent > 0) {
          if (pInfo[i].check_timeout(&pInfo[i])) {
  #ifdef ARSENAL_DEBUG_FNUMBER
            printf("Timed out for no movement\n");
  #endif
            break;
          }
        }

        // If we've hit the target
        if (pInfo[i].check_complete(&pInfo[i]) == TRUE) {
          pInfo[i].complete = TRUE;
  #ifdef ARSENAL_DEBUG_FNUMBER
          printf("Hit target F or close enough\n");
  #endif
        } else {

  #ifdef ARSENAL_DEBUG_FNUMBER
          printf("Not yet at F target.\n");
  #endif

          // Ensure fStepDelay seconds have elapsed
          if (pInfo[i].stepsTotalSent == 0 || pInfo[i].lastChange + pInfo[i].stepDelay < pInfo[i].loopStartTime) {
  #ifdef ARSENAL_DEBUG_FNUMBER
            printf("Enough time has passed for a step.\n");
            printf("Last F change = %lf\n", (pInfo[i].lastChange == 0.0f) ? 0.0f : pInfo[i].lastChange - startTime);
  #endif
            // If no steps remain and recalc timeout since last change has passed, recalculate because we're not there yet
            if (pInfo[i].stepsRemain == 0 &&
              (pInfo[i].stepsTotalSent == 0 || pInfo[i].lastChange + pInfo[i].changeRecalcBaseTimeout + (pInfo[i].changeRecalcPerStepTimeout * (double)pInfo[i].stepsLastCalc) < loopStartTime)) {
              pInfo[i].stepsRemain = fabs(pInfo[i].stepsCalc);
              pInfo[i].stepsLastCalc = pInfo[i].stepsRemain;

  #ifdef ARSENAL_DEBUG_FNUMBER
              printf("fMoves = %lf\n", pInfo[i].stepsCalc);
              printf("Setting F steps remain to = %u\n", pInfo[i].stepsRemain);
  #endif
            } else {
  #ifdef ARSENAL_DEBUG_FNUMBER
              printf("Not time to recalc F yet.\n");
  #endif
            }

            // If we still have steps to send, send one now
            if (pInfo[i].stepsRemain > 0 && pInfo[i].lastChange + pInfo[i].stepDelay < pInfo[i].loopStartTime) {
              if (pInfo[i].stepsCalc > 0) {
                moveVal.u8 = 0x01;
              } else {
                moveVal.u8 = 0xff;
              }

              //printf("Stepping F - fLastChange =%lf\n", fLastChange -startTime);
              C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, pInfo[i].propcode, &moveVal, PTP_DTC_UINT8 ));
              pInfo[i].lastChange =  pInfo[i].loopStartTime;
              pInfo[i].stepsTotalSent++;
              pInfo[i].stepsRemain--;
            }
          } else {
  #ifdef ARSENAL_DEBUG_FNUMBER
            printf("Not time to step F yet.\n");
  #endif
          }
        }

        pInfo[i].last.u32 = pInfo[i].current.u32;
      } // End update f-number
    }

    usleep(50000); // Sleep 50ms

    gettimeofday(&tv, NULL);
    loopEndTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

    //printf("Loop time = %lf\n", loopEndTime - loopStartTime);

    // Check that each config is complete
    incomplete = FALSE;
    for (i = 0; i < count; i++) {
      if (pInfo[i].complete == FALSE) {
        incomplete = TRUE;
      }
    }
  } while(incomplete);

  return GP_OK;
}

/*
 * This function looks up a label or key entry of
 * a configuration widget.
 * The functions descend recursively, so you can just
 * specify the last component.
 */

static int
_lookup_widget(CameraWidget *widget, const char *key, CameraWidget **child) {
  int ret;
  ret = gp_widget_get_child_by_name (widget, key, child);
  if (ret < GP_OK)
    ret = gp_widget_get_child_by_label (widget, key, child);
  return ret;
}

void _sony_config_f_number_struct(sony_update_config_info *pInfo, Camera *camera, float fTarget) {
  pInfo->complete                       = FALSE;
  pInfo->camera                         = camera;
  pInfo->propcode                       = PTP_DPC_FNumber;
  pInfo->get_current_value              = _sony_get_current_f_number;
  pInfo->calculate_steps                = _sony_calc_f_number_steps;
  pInfo->check_timeout                  = _sony_check_f_number_timeout;
  pInfo->check_complete                 = _sony_check_f_number_complete;

  pInfo->stepsCalc                      = 0;
  pInfo->stepsLastCalc                  = 1;
  pInfo->stepsRemain                    = 0;
  pInfo->stepsTotalSent                 = 0;

  pInfo->lastChange                     = 0.0f;
  pInfo->changeErrorTimeout             = 5.0f; // No change for x seconds times out
  pInfo->changeRecalcBaseTimeout        = 0.6f; // The quickest we can expect an F step to complete
  pInfo->changeRecalcPerStepTimeout     = 0.1f; //
  pInfo->stepDelay                      = 0.08f;
  pInfo->current.f                      = 0.0f;
  pInfo->target.f                       = fTarget;
  pInfo->last.f                         = 0.0f;
}

void _sony_config_iso_struct(sony_update_config_info *pInfo, Camera *camera, uint32_t isoTarget) {
  pInfo->complete                       = FALSE;
  pInfo->camera                         = camera;
  pInfo->propcode                       = PTP_DPC_SONY_ISO;
  pInfo->get_current_value              = _sony_get_current_iso;
  pInfo->calculate_steps                = _sony_calc_iso_steps;
  pInfo->check_timeout                  = _sony_check_iso_timeout;
  pInfo->check_complete                 = _sony_check_iso_complete;

  pInfo->stepsCalc                      = 0;
  pInfo->stepsLastCalc                  = 1;
  pInfo->stepsRemain                    = 0;
  pInfo->stepsTotalSent                 = 0;

  pInfo->lastChange                     = 0.0f;
  pInfo->changeErrorTimeout             = 5.0f; // No change for x seconds times out
  pInfo->changeRecalcBaseTimeout        = 0.6f; // The quickest we can expect an F step to complete
  pInfo->changeRecalcPerStepTimeout     = 0.1f; //
  pInfo->stepDelay                      = 0.08f;
  pInfo->current.u32                    = 0;
  pInfo->target.u32                     = isoTarget;
  pInfo->last.u32                       = 0;  
}

void _sony_config_shutter_struct(sony_update_config_info *pInfo, Camera *camera, uint16_t expTargetDividend, uint16_t expTargetDivisor) {
  pInfo->complete                       = FALSE;
  pInfo->camera                         = camera;
  pInfo->propcode                       = PTP_DPC_SONY_ShutterSpeed;
  pInfo->get_current_value              = _sony_get_current_shutter;
  pInfo->calculate_steps                = _sony_calc_shutter_steps;
  pInfo->check_timeout                  = _sony_check_shutter_timeout;
  pInfo->check_complete                 = _sony_check_shutter_complete;

  pInfo->stepsCalc                      = 0;
  pInfo->stepsLastCalc                  = 1;
  pInfo->stepsRemain                    = 0;
  pInfo->stepsTotalSent                 = 0;

  pInfo->lastChange                     = 0.0f;
  pInfo->changeErrorTimeout             = 5.0f; // No change for x seconds times out
  pInfo->changeRecalcBaseTimeout        = 0.6f; // The quickest we can expect an F step to complete
  pInfo->changeRecalcPerStepTimeout     = 0.1f; //
  pInfo->stepDelay                      = 0.08f;
  pInfo->current.u32                    = 0.0f;
  pInfo->target.u16_fraction.dividend   = expTargetDividend;
  pInfo->target.u16_fraction.divisor    = expTargetDivisor;
  pInfo->last.u32                       = 0.0f;  
}

static int _sony_get_current_f_number(sony_update_config_info *pInfo) {
  PTPParams    *params = &(pInfo->camera->pl->params);
  GPContext     *context = ((PTPData *) params->data)->context;

  C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
  C_PTP_REP (ptp_generic_getdevicepropdesc (params, pInfo->propcode, &pInfo->dpd));

  pInfo->current.f = ((float)pInfo->dpd.CurrentValue.u16) / 100.0;

  return GP_OK;
}

static int _sony_calc_f_number_steps(sony_update_config_info *pInfo) {
  float maxStops = 12;
  float fCurrentSteps;
  float fTargetSteps;

#ifdef ARSENAL_DEBUG_FNUMBER
  printf("calculating f steps\n");
#endif

  fCurrentSteps = maxStops - (float)(log(pInfo->current.f*pInfo->current.f) / log(2));
  fTargetSteps = maxStops - (float)(log(pInfo->target.f*pInfo->target.f) / log(2));

  // How many moves to get to the target (assumes camera is setup for 1/3rd stops)
  pInfo->stepsCalc = roundf((fCurrentSteps - fTargetSteps) * 3);

  return GP_OK;
}

static uint8_t _sony_check_f_number_timeout(sony_update_config_info *pInfo) {
  if (pInfo->last.f > 0.0f && fabs(pInfo->current.f - pInfo->last.f) < 0.3f) {
    // No change
    #ifdef ARSENAL_DEBUG_FNUMBER
      printf("Checking for timeout - no movement\n");
    #endif
    if (pInfo->lastChange + pInfo->changeErrorTimeout < pInfo->loopStartTime) {
      return TRUE;
    }
  } else {
    // Record time of last change for timeout
#ifdef ARSENAL_DEBUG_FNUMBER
    printf("Something changed, recording time\n");
#endif
    pInfo->lastChange = pInfo->loopStartTime;
  }

  return FALSE;
}

static uint8_t _sony_check_f_number_complete(sony_update_config_info *pInfo) {
  if (fabs(pInfo->target.f - pInfo->current.f) < 0.1f || pInfo->stepsCalc == 0) {
    return TRUE;
  }

  return FALSE;
}

static int _sony_get_current_iso(sony_update_config_info *pInfo) {
  PTPParams    *params = &(pInfo->camera->pl->params);
  GPContext     *context = ((PTPData *) params->data)->context;

  C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
  C_PTP_REP (ptp_generic_getdevicepropdesc (params, pInfo->propcode, &pInfo->dpd));

  pInfo->current.u32 = pInfo->dpd.CurrentValue.u32;

  return GP_OK;
}

static int _sony_calc_iso_steps(sony_update_config_info *pInfo) {
  uint32_t i = 0;
  int32_t targetIndex = -1;
  int32_t currentIndex = -1;

#ifdef ARSENAL_DEBUG_ISO
  printf("calculating ISO steps\n");
#endif

  if (!(pInfo->dpd.FormFlag & PTP_DPFF_Enumeration))
    return (GP_ERROR);
  if (pInfo->dpd.DataType != PTP_DTC_UINT32)
    return (GP_ERROR);

  /* match the closest value */
  for (i=0;i<pInfo->dpd.FORM.Enum.NumberOfValues; i++) {
    if (pInfo->dpd.FORM.Enum.SupportedValue[i].u32 == pInfo->target.u32) {
      targetIndex = i;
    }
    if (pInfo->dpd.FORM.Enum.SupportedValue[i].u32 == pInfo->dpd.CurrentValue.u32) {
      currentIndex = i;
    }
  }
  if (targetIndex == -1 || currentIndex == -1) {
#ifdef ARSENAL_DEBUG_ISO
    printf("target (%d) or current iso (%d) not found\n", pInfo->target.u32, pInfo->dpd.CurrentValue.u32);
#endif

    return GP_ERROR;
  }

#ifdef ARSENAL_DEBUG_ISO
  printf("target iso (%d) index = %d\n", pInfo->target.u32, targetIndex);
  printf("current iso (%d) index = %d\n", pInfo->dpd.CurrentValue.u32, currentIndex);
#endif

  pInfo->stepsCalc = targetIndex - currentIndex;

  if (pInfo->stepsCalc > 3) {
    pInfo->stepsCalc = 3;
  }
  if (pInfo->stepsCalc < -3) {
    pInfo->stepsCalc = -3;
  }

#ifdef ARSENAL_DEBUG_ISO
  printf("ISO steps = %d\n", pInfo->stepsCalc);
#endif

  return GP_OK;
}

static uint8_t _sony_check_iso_timeout(sony_update_config_info *pInfo) {
  if (pInfo->current.u32 == pInfo->last.u32) {
    #ifdef ARSENAL_DEBUG_ISO
      printf("Checking for ISO timeout - no movement\n");
    #endif
    if (pInfo->lastChange + pInfo->changeErrorTimeout < pInfo->loopStartTime) {
      return TRUE;
    }
  } else {
    // Record time of last change for timeout
#ifdef ARSENAL_DEBUG_ISO
    printf("ISO changed, recording time\n");
#endif
    pInfo->lastChange = pInfo->loopStartTime;
  }

  return FALSE;
}

static uint8_t _sony_check_iso_complete(sony_update_config_info *pInfo) {
  if (pInfo->target.u32 == pInfo->current.u32 || pInfo->stepsCalc == 0) {
    return TRUE;
  }

  return FALSE;
}

static int _sony_get_current_shutter(sony_update_config_info *pInfo) {
  PTPParams    *params = &(pInfo->camera->pl->params);
  GPContext     *context = ((PTPData *) params->data)->context;

  C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
  C_PTP_REP (ptp_generic_getdevicepropdesc (params, pInfo->propcode, &pInfo->dpd));

  if (pInfo->dpd.FormFlag & PTP_DPFF_Enumeration)
    return (GP_ERROR);
  if (pInfo->dpd.DataType != PTP_DTC_UINT32)
    return (GP_ERROR);

  pInfo->current.u16_fraction.dividend = pInfo->dpd.CurrentValue.u32>>16;
  pInfo->current.u16_fraction.divisor = pInfo->dpd.CurrentValue.u32&0xffff;

  return GP_OK;
}

static int _sony_calc_shutter_steps(sony_update_config_info *pInfo) {
  uint32_t ret;
  uint32_t i = 0;
  int32_t targetIndex = -1;
  int32_t currentIndex = -1;
  const char *choiceVal;
  char currentBuf[16];
  char targetBuf[16];

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("Calculating shutter steps\n");
#endif

  PTPParams    *params = &(pInfo->camera->pl->params);
  GPContext     *context = ((PTPData *) params->data)->context;

  CameraWidget *rootwidget;
  CameraWidget *widget_exp;

  // Get the shutterspeed widget to use its list of possible options
  ret = pInfo->camera->functions->get_config ( pInfo->camera, &rootwidget, context);
  if (ret != GP_OK) {
    return ret;
  }
  _lookup_widget(rootwidget, "shutterspeed", &widget_exp);

  int testDivisor = 1;
  int testDividend = 0;

  if (pInfo->current.u16_fraction.divisor == 1) {
    sprintf(currentBuf, "%d", pInfo->current.u16_fraction.dividend);
  } else {
    sprintf(currentBuf, "%d/%d", pInfo->current.u16_fraction.dividend, pInfo->current.u16_fraction.divisor);
  }

  if (pInfo->target.u16_fraction.divisor == 1) {
    sprintf(targetBuf, "%d", pInfo->target.u16_fraction.dividend / pInfo->target.u16_fraction.divisor);
  } else {
    sprintf(targetBuf, "%d/%d", pInfo->target.u16_fraction.dividend, pInfo->target.u16_fraction.divisor);
  }

  /* match the closest value */
  int choices = gp_widget_count_choices (widget_exp);
  for (i = 0; i < choices; i++) {
    ret = gp_widget_get_choice (widget_exp, i, (const char**)&choiceVal);
    if (ret < GP_OK) {
      return ret;
    }
    testDividend = 1;
    testDivisor = 1;
    if (sscanf(choiceVal, "%d/%d", &testDividend, &testDivisor) < 1) {
      return GP_ERROR;
    }
    if ((float)testDividend / (float)testDivisor == (float)pInfo->current.u16_fraction.dividend / (float)pInfo->current.u16_fraction.divisor) {
      currentIndex = i;
    }
    if ((float)testDividend / (float)testDivisor == (float)pInfo->target.u16_fraction.dividend / (float)pInfo->target.u16_fraction.divisor) {
      targetIndex = i;
    }
    if (targetIndex != -1 && currentIndex != -1) {
      break;
    }
  }

  if (targetIndex == -1 || currentIndex == -1) {
#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("target (%d/%d) or current shutterspeed (%d/%d) not found\n", pInfo->target.u16_fraction.dividend, pInfo->target.u16_fraction.divisor, pInfo->current.u16_fraction.dividend, pInfo->current.u16_fraction.divisor);
#endif

    return GP_ERROR;
  }

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("target shutterspeed (%d/%d) index = %d\n", pInfo->target.u16_fraction.dividend, pInfo->target.u16_fraction.divisor, targetIndex);
  printf("current shutterspeed (%d/%d) index = %d\n", pInfo->current.u16_fraction.dividend, pInfo->current.u16_fraction.divisor, currentIndex);
#endif

  pInfo->stepsCalc = targetIndex - currentIndex;

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("shutterspeed steps = %d\n", pInfo->stepsCalc);
#endif

  return GP_OK;
}

static uint8_t _sony_check_shutter_timeout(sony_update_config_info *pInfo) {
  if ((float)pInfo->last.u16_fraction.dividend / (float)pInfo->last.u16_fraction.divisor == (float)pInfo->current.u16_fraction.dividend / (float)pInfo->current.u16_fraction.divisor) {
    // No change
    #ifdef ARSENAL_DEBUG_EXP
      printf("Checking for exp timeout - no movement\n");
    #endif
    if (pInfo->lastChange + pInfo->changeErrorTimeout < pInfo->loopStartTime) {
      return TRUE;
    }
  } else {
    // Record time of last change for timeout
#ifdef ARSENAL_DEBUG_EXP
    printf("Something changed, recording time\n");
#endif
    pInfo->lastChange = pInfo->loopStartTime;
  }

  return FALSE;
}

static uint8_t _sony_check_shutter_complete(sony_update_config_info *pInfo) {
  if ((float)pInfo->target.u16_fraction.dividend / (float)pInfo->target.u16_fraction.divisor == (float)pInfo->current.u16_fraction.dividend / (float)pInfo->current.u16_fraction.divisor) {
    return TRUE;
  }

  return FALSE;
}
