static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child);

static int
_get_Sony_F_ISO_Exp(CONFIG_GET_ARGS) {
	return GP_OK;
}

static int
_put_Sony_F_ISO_Exp(CONFIG_PUT_ARGS) {
	return GP_OK;
}

int
_put_Sony_F_and_ISO(Camera *camera, float targetf, uint32_t targetiso, GPContext *context) {

	PTPParams		*params = &(camera->pl->params);
	PTPPropertyValue	moveval;
	PTPDevicePropDesc dpd;

	uint32_t ret;
	uint8_t fComplete, isoComplete;
	uint8_t fStepsRemain 									= 0;
	uint8_t fStepsLastCalc 								= 1;
	uint8_t fStepsTotalSent               = 0;

	float currentf, targetStops, currentStops, moves;
	
	float fMaxStops 							      	= 12;
	float fLastValue 											= 0.0;

	double startTime, endTime;
	double loopStartTime, loopEndTime;
	double fLastChange 										= 0.0;
	double fChangeErrorTimeout 			      = 5.0f;  // No change for x seconds times out
	double fChangeRecalcBaseTimeout 		  = 0.6f;  // The quickest we can expect a step to complete
	double fChangeRecalcPerStepTimeout 		= 0.1f; //
	double fStepDelay 										= 0.08f;

	struct timeval tv;

	fComplete = FALSE;
	isoComplete = FALSE;

	CameraWidget *widget;
	CameraWidget *child;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret == GP_OK) {
		printf("Got config\n");
		return GP_OK;
	} else {
		printf("Couldn't get config\n");
	}
	ret = _lookup_widget (widget, 'iso', &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
	}

	gettimeofday(&tv, NULL);
	startTime = tv.tv_sec + (tv.tv_usec / 1000000.0);
	fLastChange = startTime;

	targetStops = fMaxStops - (float)(log(targetf*targetf) / log(2));

	printf("Target F = %lf\n", targetf);

	do {
		gettimeofday(&tv, NULL);
		loopStartTime = tv.tv_sec + (tv.tv_usec / 1000000.0);
		printf("Loop %u start time = %lf\n", fStepsTotalSent,loopStartTime - startTime);

		// Set ISO if necessary.  Should only happen once.
		if (isoComplete == FALSE) {
			// Get current ISO
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ISO, &dpd));

			// Passing targetiso = 0 means do not set ISO
			if (targetiso == 0 || dpd.CurrentValue.u32 == targetiso) {
				isoComplete = TRUE;
			} else {
				_put_sony_value_u32(params, PTP_DPC_SONY_ISO, targetiso, 1);
			}
		}

		// If f-number has not been reached, calculate and send more steps		
		if (fComplete == FALSE) {
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_FNumber, &dpd));

			currentf = ((float)dpd.CurrentValue.u16) / 100.0;
			printf("Last F = %lf\n", fLastValue);
			printf("Current F = %lf\n", currentf);
			printf("Last F change = %lf\n", (fLastChange == 0.0f) ? 0.0f : fLastChange - startTime);
      printf("Change will timeout at = %lf\n", fLastChange + fChangeErrorTimeout - startTime);
			printf("fStepsRemain = %u\n", fStepsRemain);

			// Check to make sure we're not stuck not movie
			if (fStepsTotalSent > 0) {
				if (fabs(currentf - fLastValue) < 0.3f) {
					// No movement
					printf("No movement\n");
					if (fLastChange + fChangeErrorTimeout < loopStartTime) {
						// Change error timeout exceeded
						printf("Timed out for no movement\n");
						break;
					}
				} else {
					// Record time of last change for timeout
					printf("Something changed, recording time\n");
					fLastChange = loopStartTime;
				}
			}

			// Check how many stops we need to move
			currentStops = fMaxStops - (float)(log(currentf*currentf) / log(2));

			// How many moves to get to the target (assumes camera is setup for 1/3rd stops)
			moves = (currentStops - targetStops) * 3;

			// If we've hit the target exactly, or are within 0.1
			if (targetf == currentf || moves < 0.1 && moves > -0.1) {
				printf("Hit target or close enough\n");
				fComplete = TRUE;
			} else {

				printf("Not yet at target.\n");

				// Ensure fStepDelay seconds have elapsed
				if (fStepsTotalSent = 0 || fLastChange +fStepDelay < loopStartTime) {
					printf("Enough time has passed for a step.\n");
					printf("Last F change = %lf\n", (fLastChange == 0.0f) ? 0.0f : fLastChange - startTime);
					if (fLastChange < 0.1) {
						printf("fLastChange is zero\n");
					} else {
						printf("fLastChange is NOT zero\n");
					}
					// If no steps remain and recalc timeout since last change has passed, recalculate because we're not there yet
					if (fStepsRemain == 0 &&
						 (fStepsTotalSent = 0 || fLastChange + fChangeRecalcBaseTimeout + (fChangeRecalcPerStepTimeout * fStepsLastCalc) < loopStartTime)) {
						fStepsRemain = abs(round(moves));
						fStepsLastCalc = fStepsRemain;

						printf("Moves = %lf\n", moves);
						printf("abs(round(moves)) = %u\n", (uint8_t)abs(round(moves)));
						printf("Setting steps remain to = %u\n", fStepsRemain);
					} else {
						printf("Not time to recalc yet.\n");
					}

					// If we still have steps to send, send one now
					if (fStepsRemain > 0 && fLastChange +fStepDelay < loopStartTime) {
						if (moves > 0) {
							moveval.u8 = 0x01;
						} else {
							moveval.u8 = 0xff;
						}

						printf("Stepping - fLastChange =%lf\n", fLastChange -startTime);
						C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_FNumber, &moveval, PTP_DTC_UINT8 ));
						fLastChange =loopStartTime;
						fStepsTotalSent++;
						fStepsRemain--;
					}
				} else {
					printf("Not time to step yet.\n");
				}
			}

			fLastValue = currentf;

			usleep(50000);
		}

		if (isoComplete == FALSE) {
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ISO, &dpd));
		}

		gettimeofday(&tv, NULL);
		loopEndTime = tv.tv_sec + (tv.tv_usec / 1000000.0);

		printf("Loop time = %lf\n", loopEndTime - loopStartTime);

	} while(fComplete == FALSE);

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
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child) {
	int ret;
	ret = gp_widget_get_child_by_name (widget, key, child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label (widget, key, child);
	return ret;
}
