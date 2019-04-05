//#define ARSENAL_DEBUG_FNUMBER
//#define ARSENAL_DEBUG_ISO
#define ARSENAL_DEBUG_EXPOSURE

typedef struct sony_update_config_info sony_update_config_info; 
typedef struct sony_update_config_info {
	uint8_t complete;
	uint32_t stepsRemain;
	uint32_t stepsTotalSent;

	uint32_t stepsLastCalc;

	// Time values
	double stepDelay;										// Minimum time delay between steps
	double loopStartTime;
	double loopEndTime;
	double lastChange;
	double changeErrorTimeout; 					// No change for x seconds times out
	double changeRecalcBaseTimeout; 		// The quickest we can expect an F step to complete
	double changeRecalcPerStepTimeout;  //

	int (*get_current_value)(sony_update_config_info *pInfo, PTPDevicePropDesc *dpd, CameraWidget *widget);
	int (*calculate_steps)(sony_update_config_info *pInfo, PTPDevicePropDesc *dpd, CameraWidget *widget, int32_t *steps);

	sony_update_config_info *p_next;
};

static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child);

static int
_calculate_iso_steps(PTPDevicePropDesc *dpd, uint32_t targetiso, int32_t *steps);

static int
_calculate_exposure_steps(PTPDevicePropDesc *dpd, CameraWidget *widgetExp, int targetExpDividend, int targetExpDivisor, int32_t *steps);

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

	char		*val;

	char *fnumberstr;
	char *isostr;
	char *exposurestr;
	char *pos;
	uint32_t commacount = 0;

	float fTarget;
	uint32_t isoTarget;
  int expTargetDividend = 0;
  int expTargetDivisor = 1;

	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;
	PTPPropertyValue	fMoveVal;
	PTPPropertyValue	isoMoveVal;
	PTPPropertyValue	expMoveVal;
	PTPDevicePropDesc dpd_f;
	PTPDevicePropDesc dpd_iso;
	PTPDevicePropDesc dpd_exp;

	uint32_t ret;
	uint8_t fComplete;
	uint32_t fStepsRemain 								= 0;
	uint32_t fStepsLastCalc 							= 1;
	uint32_t fStepsTotalSent              = 0;

	float fCurrent, fTargetStops, fCurrentStops, fMoves;
	
	float fMaxStops 							      	= 12;
	float fLastValue 											= 0.0;

	double startTime, endTime;
	double loopStartTime, loopEndTime;
	double fLastChange 										= 0.0;
	double fChangeErrorTimeout 			      = 5.0f; // No change for x seconds times out
	double fChangeRecalcBaseTimeout 		  = 0.6f; // The quickest we can expect an F step to complete
	double fChangeRecalcPerStepTimeout 		= 0.1f; //
	double fStepDelay 										= 0.08f;

	uint8_t isoComplete;

	int32_t isoSteps 									  	= 0;
	uint32_t isoCurrent 									= 0;
	uint32_t isoLastValue 								= 0;

	uint32_t isoStepsRemain 							= 0;
	uint32_t isoStepsLastCalc 						= 1;
	uint32_t isoStepsTotalSent            = 0;

	double isoLastChange 									= 0.0;
	double isoChangeErrorTimeout					= 5.0f; // No change for x seconds times out
	double isoChangeRecalcBaseTimeout     = 0.6f; // The quickest we can expect an ISO step to complete
	double isoChangeRecalcPerStepTimeout	= 0.1f; //
	double isoStepDelay 									= 0.08f;

	uint8_t expComplete;

	int32_t expSteps 									  	= 0;
	int expCurrentDividend	  						= 0;
	int expCurrentDivisor 	  						= 0;
	int expLastDividend	  				    		= 0;
	int expLastDivisor 	  								= 0;

	uint32_t expStepsRemain 							= 0;
	uint32_t expStepsLastCalc 						= 1;
	uint32_t expStepsTotalSent            = 0;

	double expLastChange 									= 0.0;
	double expChangeErrorTimeout					= 5.0f; // No change for x seconds times out
	double expChangeRecalcBaseTimeout     = 0.6f; // The quickest we can expect an shutterspeed step to complete
	double expChangeRecalcPerStepTimeout	= 0.1f; //
	double expStepDelay 									= 0.08f;

	fComplete = FALSE;
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
	fLastChange = startTime;

	fTargetStops = fMaxStops - (float)(log(fTarget*fTarget) / log(2));

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
		if (fComplete == FALSE) {
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_FNumber, &dpd_f));

			fCurrent = ((float)dpd_f.CurrentValue.u16) / 100.0;

#ifdef ARSENAL_DEBUG_FNUMBER
			printf("Last F = %lf\n", fLastValue);
			printf("Current F = %lf\n", fCurrent);
			printf("Last F change = %lf\n", (fLastChange == 0.0f) ? 0.0f : fLastChange - startTime);
      printf("Change will timeout at = %lf\n", fLastChange + fChangeErrorTimeout - startTime);
			printf("fStepsRemain = %u\n", fStepsRemain);
#endif

			// Check to make sure we're not stuck not movie
			if (fStepsTotalSent > 0) {
				if (fabs(fCurrent - fLastValue) < 0.3f) {
					// No movement
#ifdef ARSENAL_DEBUG_FNUMBER
					printf("No movement\n");
#endif
					if (fLastChange + fChangeErrorTimeout < loopStartTime) {
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
					fLastChange = loopStartTime;
				}
			}

			// Check how many stops we need to move
			fCurrentStops = fMaxStops - (float)(log(fCurrent*fCurrent) / log(2));

			// How many moves to get to the target (assumes camera is setup for 1/3rd stops)
			fMoves = (fCurrentStops - fTargetStops) * 3;

			if (fMoves > 3) {
				fMoves = 3;
			}

			// If we've hit the target exactly, or are within 0.1
			if (fTarget == fCurrent || fMoves < 0.1 && fMoves > -0.1) {
#ifdef ARSENAL_DEBUG_FNUMBER
				printf("Hit target F or close enough\n");
#endif
				fComplete = TRUE;
			} else {

#ifdef ARSENAL_DEBUG_FNUMBER
				printf("Not yet at F target.\n");
#endif

				// Ensure fStepDelay seconds have elapsed
				if (fStepsTotalSent = 0 || fLastChange +fStepDelay < loopStartTime) {
#ifdef ARSENAL_DEBUG_FNUMBER
					printf("Enough time has passed for a step.\n");
					printf("Last F change = %lf\n", (fLastChange == 0.0f) ? 0.0f : fLastChange - startTime);
					if (fLastChange < 0.1) {
						printf("fLastChange is zero\n");
					} else {
						printf("fLastChange is NOT zero\n");
					}
#endif
					// If no steps remain and recalc timeout since last change has passed, recalculate because we're not there yet
					if (fStepsRemain == 0 &&
						 (fStepsTotalSent = 0 || fLastChange + fChangeRecalcBaseTimeout + (fChangeRecalcPerStepTimeout * fStepsLastCalc) < loopStartTime)) {
						fStepsRemain = abs(round(fMoves));
						fStepsLastCalc = fStepsRemain;

#ifdef ARSENAL_DEBUG_FNUMBER
						printf("fMoves = %lf\n", fMoves);
						printf("abs(round(fMoves)) = %u\n", (uint8_t)abs(round(fMoves)));
						printf("Setting F steps remain to = %u\n", fStepsRemain);
#endif
					} else {
#ifdef ARSENAL_DEBUG_FNUMBER
						printf("Not time to recalc F yet.\n");
#endif
					}

					// If we still have steps to send, send one now
					if (fStepsRemain > 0 && fLastChange +fStepDelay < loopStartTime) {
						if (fMoves > 0) {
							fMoveVal.u8 = 0x01;
						} else {
							fMoveVal.u8 = 0xff;
						}

						//printf("Stepping F - fLastChange =%lf\n", fLastChange -startTime);
						C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_FNumber, &fMoveVal, PTP_DTC_UINT8 ));
						fLastChange =loopStartTime;
						fStepsTotalSent++;
						fStepsRemain--;
					}
				} else {
#ifdef ARSENAL_DEBUG_FNUMBER
					printf("Not time to step F yet.\n");
#endif
				}
			}

			fLastValue = fCurrent;

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

	} while(fComplete == FALSE || isoComplete == FALSE || expComplete == FALSE);

	gettimeofday(&tv, NULL);
	endTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

	printf("Exiting - cycle time = %lf\n", endTime - startTime);
	printf("%u steps sent\n", fStepsTotalSent);

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