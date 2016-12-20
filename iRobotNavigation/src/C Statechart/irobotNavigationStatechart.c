#include "irobotNavigationStatechart.h"
#include <math.h>
#include <stdlib.h>

/// Program States
typedef enum{
	INITIAL = 0,						///< Initial state
	PAUSE_WAIT_BUTTON_RELEASE,			///< Paused; pause button pressed down, wait until released before detecting next press
	UNPAUSE_WAIT_BUTTON_PRESS,			///< Paused; wait for pause button to be pressed
	UNPAUSE_WAIT_BUTTON_RELEASE,		///< Paused; pause button pressed down, wait until released before returning to previous state
	DRIVE,								///< Drive straight
	TURNLEFT,							///< Turn left
	TURNRIGHT,							///< Turn right
	STOP,								///< Stop
	BACK								///< Drive backward
} robotState_t;

#define DEG_PER_RAD			(180.0 / M_PI)		///< degrees per radian
#define RAD_PER_DEG			(M_PI / 180.0)		///< radians per degree

void irobotNavigationStatechart(
	const int32_t 				netDistance,
	const int32_t 				netAngle,
	const irobotSensorGroup6_t 	sensors,
	const accelerometer_t 		accel,
	const bool					isSimulator,
	int16_t * const 			pRightWheelSpeed,
	int16_t * const 			pLeftWheelSpeed
){
	// local state
	static robotState_t 		state = INITIAL;				// current program state
	static robotState_t			unpausedState = DRIVE;			// state history for pause region
	static int32_t				distanceAtManeuverStart = 0;	// distance robot had travelled when a maneuver begins, in mm
	static int32_t				angleAtManeuverStart = 0;		// angle through which the robot had turned when a maneuver begins, in deg
	
	static bool					bump = false;					
	static bool					passed = false;					// passed the obstacle?
	static bool					driveForAdjustment = false;		// drive for avoiding the obstacle
	static bool					timeToAdjustAngle = false;
	static int32_t				rightTurnAngle = 90;			// angle for turning right
	static int32_t				leftTurnAngle = 90;				// angle for turning left
	static int32_t				distance = 0;					// distance for adjustment
	static int32_t				angle = 0;						// angle for adjustment

	// outputs
	int16_t						leftWheelSpeed = 0;				// speed of the left wheel, in mm/s
	int16_t						rightWheelSpeed = 0;			// speed of the right wheel, in mm/s

	//*****************************************************
	// state data - process inputs                        *
	//*****************************************************


	//*****************************************************
	// state transition - pause region (highest priority) *
	//*****************************************************
	if(   state == INITIAL
	   || state == PAUSE_WAIT_BUTTON_RELEASE
	   || state == UNPAUSE_WAIT_BUTTON_PRESS
	   || state == UNPAUSE_WAIT_BUTTON_RELEASE
	   || sensors.buttons.play				// pause button
	){
		switch(state){
		case INITIAL:
			// set state data that may change between simulation and real-world
			if(isSimulator){
			}
			else{
			}
			state = UNPAUSE_WAIT_BUTTON_PRESS; // place into pause state
			break;
		case PAUSE_WAIT_BUTTON_RELEASE:
			// remain in this state until released before detecting next press
			if(!sensors.buttons.play){
				state = UNPAUSE_WAIT_BUTTON_PRESS;
			}
			break;
		case UNPAUSE_WAIT_BUTTON_RELEASE:
			// user pressed 'pause' button to return to previous state
			if(!sensors.buttons.play){
				state = unpausedState;
			}
			break;
		case UNPAUSE_WAIT_BUTTON_PRESS:
			// remain in this state until user presses 'pause' button
			if(sensors.buttons.play){
				state = UNPAUSE_WAIT_BUTTON_RELEASE;
			}
			break;
		default:
			// must be in run region, and pause button has been pressed
			unpausedState = state;
			state = PAUSE_WAIT_BUTTON_RELEASE;
			break;
		}
	}
	//*************************************
	// state transition - run region      *
	//*************************************
	else if (state == DRIVE){
		if (sensors.bumps_wheelDrops.bumpLeft && sensors.bumps_wheelDrops.bumpRight) {
			bump = true;
			rightTurnAngle = 90;
			angle += rightTurnAngle;
			angleAtManeuverStart = netAngle;
			distanceAtManeuverStart = netDistance;
			state = BACK;
		}
		else if (sensors.bumps_wheelDrops.bumpLeft) {
			bump = true;
			rightTurnAngle = 30;
			angle += rightTurnAngle;
			angleAtManeuverStart = netAngle;
			distanceAtManeuverStart = netDistance;
			state = BACK;
		}
		else if (sensors.bumps_wheelDrops.bumpRight) {
			bump = true;
			rightTurnAngle = 145;
			angle += rightTurnAngle;
			angleAtManeuverStart = netAngle;
			distanceAtManeuverStart = netDistance;
			state = BACK;
		}
		else if (!passed && driveForAdjustment && abs(netDistance - distanceAtManeuverStart) >= 500) {
			distance += abs(netDistance - distanceAtManeuverStart);
			angleAtManeuverStart = netAngle;
			distanceAtManeuverStart = netDistance;
			driveForAdjustment = false;
			angle -= leftTurnAngle;
			state = TURNLEFT;
		}
		else if (passed && timeToAdjustAngle && abs(netDistance - distanceAtManeuverStart) >= distance) {
			angleAtManeuverStart = netAngle;
			distanceAtManeuverStart = netDistance;
			timeToAdjustAngle = false;
			rightTurnAngle = abs(angle);
			state = TURNRIGHT;
		}
		else if (bump && abs(netDistance - distanceAtManeuverStart) >= 600) {
			angleAtManeuverStart = netAngle;
			distanceAtManeuverStart = netDistance;
			bump = false;
			passed = true;
			timeToAdjustAngle = true;
			state = TURNLEFT;
			angle -= leftTurnAngle;
		}
	}

	else if(state == TURNRIGHT && abs(netAngle - angleAtManeuverStart) >= rightTurnAngle){
		angleAtManeuverStart = netAngle;
		distanceAtManeuverStart = netDistance;
		driveForAdjustment = true;
		if (passed) {
			bump = false;
			passed = false;					// passed the obstacle?
			driveForAdjustment = false;		// drive for avoiding the obstacle
			timeToAdjustAngle = false;
			rightTurnAngle = 90;			// angle for turning right
			leftTurnAngle = 90;				// angle for turning left
			distance = 0;					// distance for adjustment
			angle = 0;						// angle for adjustment
		}
		state = DRIVE;
	}

	else if (state == TURNLEFT && abs(netAngle - angleAtManeuverStart) >= leftTurnAngle) {
		driveForAdjustment = false;
		angleAtManeuverStart = netAngle;
		distanceAtManeuverStart = netDistance;
		state = DRIVE;
	}
	else if (state == BACK && abs(netDistance - distanceAtManeuverStart) >= 50){
		angleAtManeuverStart = netAngle;
		distanceAtManeuverStart = netDistance;
		state = TURNRIGHT;
	}
	
	// else, no transitions are taken

	//*****************
	//* state actions *
	//*****************
	switch(state){
	case INITIAL:
	case PAUSE_WAIT_BUTTON_RELEASE:
	case UNPAUSE_WAIT_BUTTON_PRESS:
	case UNPAUSE_WAIT_BUTTON_RELEASE:
		// in pause mode, robot should be stopped
		leftWheelSpeed = rightWheelSpeed = 0;
		break;
	case DRIVE:
		// full speed ahead!
		leftWheelSpeed = rightWheelSpeed = 150;
		break;
	case TURNRIGHT:
		leftWheelSpeed = 100;
		rightWheelSpeed = -leftWheelSpeed;
		break;
	case TURNLEFT:
		rightWheelSpeed = 100;
		leftWheelSpeed = -rightWheelSpeed;
		break;
	case BACK:
		rightWheelSpeed = -100;
		leftWheelSpeed = -100;
		break;
	case STOP:
	default:
		// Unknown state
		leftWheelSpeed = rightWheelSpeed = 0;
		break;
	}

	// write outputs
	*pLeftWheelSpeed = leftWheelSpeed;
	*pRightWheelSpeed = rightWheelSpeed;
}
