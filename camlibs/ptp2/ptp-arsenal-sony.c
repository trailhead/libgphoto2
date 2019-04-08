#define ARSENAL_DEBUG_FNUMBER
//#define ARSENAL_DEBUG_ISO
//#define ARSENAL_DEBUG_EXPOSURE

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

  int (*get_current_value)(sony_update_config_info *pInfo);
  int (*calculate_steps)(sony_update_config_info *pInfo);
  uint8_t (*check_complete)(sony_update_config_info *pInfo);
};

static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child);

static int
_calculate_iso_steps(PTPDevicePropDesc *dpd, uint32_t targetiso, int32_t *steps);

static int
_calculate_exposure_steps(PTPDevicePropDesc *dpd, CameraWidget *widgetExp, int targetExpDividend, int targetExpDivisor, int32_t *steps);

static int _sony_get_current_f_number(sony_update_config_info *pInfo);
static int _sony_get_current_iso(sony_update_config_info *pInfo);
static int _sony_get_current_shutter(sony_update_config_info *pInfo);

static int _sony_calc_f_number_steps(sony_update_config_info *pInfo);
static int _sony_calc_iso_steps(sony_update_config_info *pInfo);
static int _sony_calc_shutter_steps(sony_update_config_info *pInfo);

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

  sony_update_config_info info_f;
  info_f.complete                       = FALSE;
  info_f.camera                         = camera;
  info_f.propcode                       = PTP_DPC_FNumber;
  info_f.get_current_value              = _sony_get_current_f_number;
  info_f.calculate_steps                = _sony_calc_f_number_steps;
  info_f.check_complete                 = _sony_check_f_number_complete;

  info_f.stepsCalc                      = 0;
  info_f.stepsLastCalc                  = 1;
  info_f.stepsRemain                    = 0;
  info_f.stepsTotalSent                 = 0;

  info_f.lastChange                     = 0.0f;
  info_f.changeErrorTimeout             = 5.0f; // No change for x seconds times out
  info_f.changeRecalcBaseTimeout        = 0.6f; // The quickest we can expect an F step to complete
  info_f.changeRecalcPerStepTimeout     = 0.1f; //
  info_f.stepDelay                      = 0.08f;
  info_f.current.f                      = 0.0f;
  info_f.target.f                       = 0.0f;
  info_f.last.f                         = 0.0f;

  char    *val;

  char *fnumberstr;
  char *isostr;
  char *exposurestr;
  char *pos;
  uint32_t commacount                   = 0;

  float fTarget;
  uint32_t isoTarget;
  int expTargetDividend                 = 0;
  int expTargetDivisor                  = 1;

  PTPParams    *params = &(camera->pl->params);
  GPContext     *context = ((PTPData *) params->data)->context;
  PTPPropertyValue  fMoveVal;
  PTPPropertyValue  isoMoveVal;
  PTPPropertyValue  expMoveVal;
  PTPDevicePropDesc dpd_iso;
  PTPDevicePropDesc dpd_exp;

  uint32_t ret;

  double startTime, endTime;
  double loopStartTime, loopEndTime;

  uint8_t isoComplete;

  int32_t isoSteps                       = 0;
  uint32_t isoCurrent                   = 0;
  uint32_t isoLastValue                 = 0;

  uint32_t isoStepsRemain               = 0;
  uint32_t isoStepsLastCalc             = 1;
  uint32_t isoStepsTotalSent            = 0;

  double isoLastChange                   = 0.0;
  double isoChangeErrorTimeout          = 5.0f; // No change for x seconds times out
  double isoChangeRecalcBaseTimeout     = 0.6f; // The quickest we can expect an ISO step to complete
  double isoChangeRecalcPerStepTimeout  = 0.1f; //
  double isoStepDelay                   = 0.08f;

  uint8_t expComplete;

  int32_t expSteps                       = 0;
  int expCurrentDividend                 = 0;
  int expCurrentDivisor                 = 0;
  int expLastDividend                    = 0;
  int expLastDivisor                     = 0;

  uint32_t expStepsRemain               = 0;
  uint32_t expStepsLastCalc             = 1;
  uint32_t expStepsTotalSent            = 0;

  double expLastChange                   = 0.0;
  double expChangeErrorTimeout          = 5.0f; // No change for x seconds times out
  double expChangeRecalcBaseTimeout     = 0.6f; // The quickest we can expect an shutterspeed step to complete
  double expChangeRecalcPerStepTimeout  = 0.1f; //
  double expStepDelay                   = 0.08f;

  isoComplete = FALSE;
  expComplete = FALSE;

  struct timeval tv;

  CameraWidget *rootwidget;
  CameraWidget *widget_exp;

  // Get the new value as a string containing all 3 parameters
  CR (gp_widget_get_value(widget, &val));

  // Get the shutterspeed widget to use its list of possible options
  ret = camera->functions->get_config ( camera, &rootwidget, context);
  if (ret != GP_OK) {
    return ret;
  }
  _lookup_widget(rootwidget, "shutterspeed", &widget_exp);

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

  gettimeofday(&tv, NULL);
  startTime = tv.tv_sec + (tv.tv_usec / 1000000.0);
  info_f.lastChange = startTime;

#ifdef ARSENAL_DEBUG_FNUMBER
  printf("Target F = %lf\n", fTarget);
#endif

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("Target Exposure = %d/%d\n", expTargetDividend, expTargetDivisor);
#endif

  do {
    gettimeofday(&tv, NULL);
    loopStartTime = tv.tv_sec + (tv.tv_usec / 1000000.0);
    //printf("Loop %u start time = %lf\n", fStepsTotalSent,loopStartTime - startTime);

    // If f-number has not been reached, calculate and send more steps    
    if (info_f.complete == FALSE) {

      info_f.target.f = fTarget;
      info_f.get_current_value(&info_f);

      info_f.calculate_steps(&info_f);

      if (info_f.stepsCalc > 3) {
        info_f.stepsCalc = 3;
      }
      if (info_f.stepsCalc < -3) {
        info_f.stepsCalc = -3;
      }

#ifdef ARSENAL_DEBUG_FNUMBER
      printf("Last F = %lf\n", info_f.last.f);
      printf("Current F = %lf\n", info_f.current.f);
      printf("Last F change = %lf\n", (info_f.lastChange == 0.0f) ? 0.0f : info_f.lastChange - startTime);
      printf("Change will timeout at = %lf\n", info_f.lastChange + info_f.changeErrorTimeout - startTime);
      printf("fStepsRemain = %u\n", info_f.stepsRemain);
#endif

      // Check to make sure we're not stuck not movie
      if (info_f.stepsTotalSent > 0) {
        if (fabs(info_f.current.f - info_f.last.f) < 0.3f) {
          // No movement
#ifdef ARSENAL_DEBUG_FNUMBER
          printf("No movement\n");
#endif
          if (info_f.lastChange + info_f.changeErrorTimeout < loopStartTime) {
            // Change error timeout exceeded
#ifdef ARSENAL_DEBUG_FNUMBER
            printf("Timed out for no movement\n");
#endif
            break;
          }
        } else {
          // Record time of last change for timeout
#ifdef ARSENAL_DEBUG_FNUMBER
          printf("Something changed, recording time\n");
#endif
          info_f.lastChange = loopStartTime;
        }
      }

      // If we've hit the target
      if (fabs(info_f.target.f - info_f.current.f) < 0.1f || info_f.stepsCalc == 0) {
#ifdef ARSENAL_DEBUG_FNUMBER
        printf("Hit target F or close enough\n");
#endif
        info_f.complete = TRUE;
      } else {

#ifdef ARSENAL_DEBUG_FNUMBER
        printf("Not yet at F target.\n");
#endif

        // Ensure fStepDelay seconds have elapsed
        if (info_f.stepsTotalSent == 0 || info_f.lastChange + info_f.stepDelay < loopStartTime) {
#ifdef ARSENAL_DEBUG_FNUMBER
          printf("Enough time has passed for a step.\n");
          printf("Last F change = %lf\n", (info_f.lastChange == 0.0f) ? 0.0f : info_f.lastChange - startTime);
#endif
          // If no steps remain and recalc timeout since last change has passed, recalculate because we're not there yet
          if (info_f.stepsRemain == 0 &&
             (info_f.stepsTotalSent == 0 || info_f.lastChange + info_f.changeRecalcBaseTimeout + (info_f.changeRecalcPerStepTimeout * (double)info_f.stepsLastCalc) < loopStartTime)) {
            info_f.stepsRemain = fabs(info_f.stepsCalc);
            info_f.stepsLastCalc = info_f.stepsRemain;

#ifdef ARSENAL_DEBUG_FNUMBER
            printf("fMoves = %lf\n", info_f.stepsCalc);
            printf("Setting F steps remain to = %u\n", info_f.stepsRemain);
#endif
          } else {
#ifdef ARSENAL_DEBUG_FNUMBER
            printf("Not time to recalc F yet.\n");
#endif
          }

          // If we still have steps to send, send one now
          if (info_f.stepsRemain > 0 && info_f.lastChange + info_f.stepDelay < loopStartTime) {
            if (info_f.stepsCalc > 0) {
              fMoveVal.u8 = 0x01;
            } else {
              fMoveVal.u8 = 0xff;
            }

            //printf("Stepping F - fLastChange =%lf\n", fLastChange -startTime);
            C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, info_f.propcode, &fMoveVal, PTP_DTC_UINT8 ));
            info_f.lastChange = loopStartTime;
            info_f.stepsTotalSent++;
            info_f.stepsRemain--;
          }
        } else {
#ifdef ARSENAL_DEBUG_FNUMBER
          printf("Not time to step F yet.\n");
#endif
        }
      }

      info_f.last.f = info_f.current.f;

      usleep(50000);
    } // End update f-number

    // Set ISO if necessary.
    if (isoComplete == FALSE) {
      // Get current ISO
      C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
      C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ISO, &dpd_iso));

      isoCurrent = dpd_iso.CurrentValue.u32;

      _calculate_iso_steps(&dpd_iso, isoTarget, &isoSteps);

      if (isoSteps > 3) {
        isoSteps = 3;
      }
      if (isoSteps < -3) {
        isoSteps = -3;
      }

      // Passing targetiso = 0 means do not set ISO
      if (isoTarget == 0 || isoCurrent == isoTarget) {
        isoComplete = TRUE;
      } else {
        // Check to make sure we're not stuck not movie
        if (isoStepsTotalSent > 0) {
          if (isoCurrent == isoLastValue) {
            // No movement
            //printf("No ISO movement\n");
            if (isoLastChange + isoChangeErrorTimeout < loopStartTime) {
              // Change error timeout exceeded
              printf("Timed out for no ISO movement\n");
              break;
            }
          } else {
            // Record time of last change for timeout
            //printf("ISO changed, recording time\n");
            isoLastChange = loopStartTime;
          }
        }
        // Ensure isoStepDelay seconds have elapsed
        if (isoStepsTotalSent = 0 || isoLastChange +isoStepDelay < loopStartTime) {
#ifdef ARSENAL_DEBUG_ISO
          printf("Enough time has passed for an ISO step.\n");
          printf("Last ISO change = %lf\n", (isoLastChange == 0.0f) ? 0.0f : isoLastChange - startTime);
          if (isoLastChange < 0.1) {
            printf("isoLastChange is zero\n");
          } else {
            printf("isoLastChange is NOT zero\n");
          }
#endif
          // If no ISO steps remain and recalc timeout since last change has passed, recalculate because we're not there yet
          if (isoStepsRemain == 0 &&
            (isoStepsTotalSent = 0 || isoLastChange + isoChangeRecalcBaseTimeout + (isoChangeRecalcPerStepTimeout * isoStepsLastCalc) < loopStartTime)) {
            isoStepsRemain = abs(isoSteps);
            isoStepsLastCalc = isoStepsRemain;

#ifdef ARSENAL_DEBUG_ISO
            printf("isoSteps = %d\n", isoSteps);
            printf("Setting iso steps remain to = %u\n", isoStepsRemain);
#endif
          } else {
#ifdef ARSENAL_DEBUG_ISO
            printf("Not time to recalc ISO yet.\n");
#endif
          }

          // If we still have steps to send, send one now
          if (isoStepsRemain > 0 && isoLastChange +isoStepDelay < loopStartTime) {
            if (isoSteps > 0) {
              isoMoveVal.u8 = 0x01;
            } else {
              isoMoveVal.u8 = 0xff;
            }

            printf("Stepping ISO - isoLastChange =%lf\n", isoLastChange - startTime);
            C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_ISO, &isoMoveVal, PTP_DTC_UINT8 ));
            isoLastChange = loopStartTime;
            isoStepsTotalSent++;
            isoStepsRemain--;
          }
        } else {
#ifdef ARSENAL_DEBUG_ISO
          printf("Not time to step ISO yet.\n");
#endif
        }
      }
      // End update iso
    }

    // Set shutterspeed if necessary.
    if (expComplete == FALSE) {
      // Get current shutterspeed
      C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
      C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ShutterSpeed, &dpd_exp));

      expCurrentDividend = dpd_exp.CurrentValue.u32>>16;
      expCurrentDivisor = dpd_exp.CurrentValue.u32&0xffff;

      _calculate_exposure_steps(&dpd_exp, widget_exp, expTargetDividend, expTargetDivisor, &expSteps);

      if (expSteps > 3) {
        expSteps = 3;
      }
      if (expSteps < -3) {
        expSteps = -3;
      }
      // Passing expTargetDividend = 0 means do not set shutterspeed
      if (expTargetDividend == 0 || expSteps == 0) {
        expComplete = TRUE;
      } else {
        // Check to make sure we're not stuck not movie
        if (expStepsTotalSent > 0) {
          if (expCurrentDividend == expLastDividend && expCurrentDivisor == expLastDivisor) {
            // No movement
            //printf("No shtuterspeed movement\n");
            if (expLastChange + expChangeErrorTimeout < loopStartTime) {
              // Change error timeout exceeded
              printf("Timed out for no shutterspeed movement\n");
              break;
            }
          } else {
            // Record time of last change for timeout
            //printf("shutterspeed changed, recording time\n");
            expLastChange = loopStartTime;
          }
        }
        // Ensure expStepDelay seconds have elapsed
        if (expStepsTotalSent = 0 || expLastChange + expStepDelay < loopStartTime) {
#ifdef ARSENAL_DEBUG_EXPOSURE
          printf("Enough time has passed for a shutterspeed step.\n");
          printf("Last shutterspeed change = %lf\n", (expLastChange == 0.0f) ? 0.0f : expLastChange - startTime);
          if (expLastChange < 0.1) {
            printf("expLastChange is zero\n");
          } else {
            printf("expLastChange is NOT zero\n");
          }
#endif
          // If no shutterspeed steps remain and recalc timeout since last change has passed, recalculate because we're not there yet
          if (expStepsRemain == 0 &&
            (expStepsTotalSent = 0 || expLastChange + expChangeRecalcBaseTimeout + (expChangeRecalcPerStepTimeout * expStepsLastCalc) < loopStartTime)) {
            expStepsRemain = abs(expSteps);
            expStepsLastCalc = expStepsRemain;

#ifdef ARSENAL_DEBUG_EXPOSURE
            printf("expSteps = %d\n", expSteps);
            printf("Setting exp steps remain to = %u\n", expStepsRemain);
#endif
          } else {
#ifdef ARSENAL_DEBUG_EXPOSURE
            printf("Not time to recalc shutterspeed yet.\n");
#endif
          }

          // If we still have steps to send, send one now
          if (expStepsRemain > 0 && expLastChange +expStepDelay < loopStartTime) {
            if (expSteps > 0) {
              expMoveVal.u8 = 0x01;
            } else {
              expMoveVal.u8 = 0xff;
            }

            printf("Stepping shutterspeed - expLastChange =%lf\n", expLastChange - startTime);
            C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_ShutterSpeed, &expMoveVal, PTP_DTC_UINT8 ));
            expLastChange = loopStartTime;
            expStepsTotalSent++;
            expStepsRemain--;
          }
        } else {
#ifdef ARSENAL_DEBUG_EXPOSURE
          printf("Not time to step shutterspeed yet.\n");
#endif
        }
      }
    } // End update shutterspeed

    gettimeofday(&tv, NULL);
    loopEndTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

    //printf("Loop time = %lf\n", loopEndTime - loopStartTime);

  } while(info_f.complete == FALSE || isoComplete == FALSE || expComplete == FALSE);

  gettimeofday(&tv, NULL);
  endTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

  printf("Exiting - cycle time = %lf\n", endTime - startTime);
  printf("%u steps sent\n", info_f.stepsTotalSent);

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

static int
_calculate_iso_steps(PTPDevicePropDesc *dpd, uint32_t targetiso, int32_t *steps) {

  uint32_t i = 0;
  int32_t targetIndex = -1;
  int32_t currentIndex = -1;

  if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
    return (GP_ERROR);
  if (dpd->DataType != PTP_DTC_UINT32)
    return (GP_ERROR);

  /* match the closest value */
  for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
    if (dpd->FORM.Enum.SupportedValue[i].u32 == targetiso) {
      targetIndex = i;
    }
    if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32) {
      currentIndex = i;
    }
  }
  if (targetIndex == -1 || currentIndex == -1) {
#ifdef ARSENAL_DEBUG_ISO
    printf("target (%d) or current iso (%d) not found\n", targetiso, dpd->CurrentValue.u32);
#endif

    return GP_ERROR;
  }

#ifdef ARSENAL_DEBUG_ISO
  printf("target iso (%d) index = %d\n", targetiso, targetIndex);
  printf("current iso (%d) index = %d\n", dpd->CurrentValue.u32, currentIndex);
#endif

  *steps = targetIndex - currentIndex;

#ifdef ARSENAL_DEBUG_ISO
  printf("iso steps = %d\n", *steps);
#endif

  return GP_OK;
}

static int
_calculate_exposure_steps(PTPDevicePropDesc *dpd, CameraWidget *widgetExp, int targetExpDividend, int targetExpDivisor, int32_t *steps) {

  uint32_t ret;
  uint32_t i = 0;
  int32_t targetIndex = -1;
  int32_t currentIndex = -1;
  const char *choiceVal;
  char currentBuf[16];
  char targetBuf[16];

  if (dpd->FormFlag & PTP_DPFF_Enumeration)
    return (GP_ERROR);
  if (dpd->DataType != PTP_DTC_UINT32)
    return (GP_ERROR);

  int currentDivisor = 0;
  int currentDividend = 0;

  int testDivisor = 1;
  int testDividend = 0;

  currentDividend = dpd->CurrentValue.u32>>16;
  currentDivisor = dpd->CurrentValue.u32&0xffff;

  if (currentDivisor == 1) {
    sprintf(currentBuf, "%d", currentDividend);
  } else {
    sprintf(currentBuf, "%d/%d", currentDividend, currentDivisor);
  }

  if (targetExpDivisor == 1) {
    sprintf(targetBuf, "%d", targetExpDividend / targetExpDivisor);
  } else {
    sprintf(targetBuf, "%d/%d", targetExpDividend, targetExpDivisor);
  }

  /* match the closest value */
  int choices = gp_widget_count_choices (widgetExp);
  for (i = 0; i < choices; i++) {
    ret = gp_widget_get_choice (widgetExp, i, (const char**)&choiceVal);
    if (ret < GP_OK) {
      return ret;
    }
    testDividend = 1;
    testDivisor = 1;
    if (sscanf(choiceVal, "%d/%d", &testDividend, &testDivisor) < 1) {
      return GP_ERROR;
    }
    if ((float)testDividend / (float)testDivisor == (float)currentDividend / (float)currentDivisor) {
      currentIndex = i;
    }
    if ((float)testDividend / (float)testDivisor == (float)targetExpDividend / (float)targetExpDivisor) {
      targetIndex = i;
    }
    if (targetIndex != -1 && currentIndex != -1) {
      break;
    }
  }

  if (targetIndex == -1 || currentIndex == -1) {
#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("target (%d/%d) or current shutterspeed (%d/%d) not found\n", targetExpDividend, targetExpDivisor, currentDividend, currentDivisor);
#endif

    return GP_ERROR;
  }

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("target shutterspeed (%d/%d) index = %d\n", targetExpDividend, targetExpDivisor, targetIndex);
  printf("current shutterspeed (%d/%d) index = %d\n", currentDividend, currentDivisor, currentIndex);
#endif

  *steps = targetIndex - currentIndex;

#ifdef ARSENAL_DEBUG_EXPOSURE
  printf("shutterspeed steps = %d\n", *steps);
#endif

  return GP_OK;
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

  fCurrentSteps = maxStops - (float)(log(pInfo->current.f*pInfo->current.f) / log(2));
  fTargetSteps = maxStops - (float)(log(pInfo->target.f*pInfo->target.f) / log(2));

  // How many moves to get to the target (assumes camera is setup for 1/3rd stops)
  pInfo->stepsCalc = roundf((fCurrentSteps - fTargetSteps) * 3);
  //pInfo->stepsRemain = fabs(pInfo->stepsCalc);

  return GP_OK;
}

static uint8_t _sony_check_f_number_complete(sony_update_config_info *pInfo) {
  return GP_OK;
}
