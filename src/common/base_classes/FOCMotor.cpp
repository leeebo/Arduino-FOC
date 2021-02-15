#include "FOCMotor.h"

/**
 * Default constructor - setting all variabels to default values
 */
FOCMotor::FOCMotor()
{
  // maximum angular velocity to be used for positioning 
  velocity_limit = DEF_VEL_LIM;
  // maximum voltage to be set to the motor
  voltage_limit = DEF_POWER_SUPPLY;
  // not set on the begining
  current_limit = DEF_CURRENT_LIM;

  // index search velocity
  velocity_index_search = DEF_INDEX_SEARCH_TARGET_VELOCITY;
  // sensor and motor align voltage
  voltage_sensor_align = DEF_VOLTAGE_SENSOR_ALIGN;

  // default modulation is SinePWM
  foc_modulation = FOCModulationType::SinePWM;

  // default target value
  target = 0;
  voltage.d = 0;
  voltage.q = 0;
  // current target values
  current_sp = 0;
  current.q = 0;
  current.d = 0;
  
  //monitor_port 
  monitor_port = nullptr;
  //sensor 
  sensor = nullptr;
  //current sensor 
  current_sense = nullptr;
}


/**
	Sensor linking method
*/
void FOCMotor::linkSensor(Sensor* _sensor) {
  sensor = _sensor;
}

/**
	CurrentSense linking method
*/
void FOCMotor::linkCurrentSense(CurrentSense* _current_sense) {
  current_sense = _current_sense;
}

// shaft angle calculation
float FOCMotor::shaftAngle() {
  // if no sensor linked return previous value ( for open loop )
  if(!sensor) return shaft_angle;
  return sensor_direction*sensor->getAngle() - sensor_offset;
}
// shaft velocity calculation
float FOCMotor::shaftVelocity() {
  // if no sensor linked return previous value ( for open loop )
  if(!sensor) return shaft_velocity;
  return sensor_direction*LPF_velocity(sensor->getVelocity());
}

float FOCMotor::electricalAngle(){
  return _normalizeAngle((shaft_angle + sensor_offset) * pole_pairs - zero_electric_angle);
}

/**
 *  Monitoring functions
 */
// function implementing the monitor_port setter
void FOCMotor::useMonitoring(Print &print){
  monitor_port = &print; //operate on the address of print
  if(monitor_port ) monitor_port->println(F("MOT: Monitor enabled!"));
}

// utility function intended to be used with serial plotter to monitor motor variables
// significantly slowing the execution down!!!!
void FOCMotor::monitor() {
  if(!monitor_port) return;
  switch (controller) {
    case MotionControlType::velocity_openloop:
    case MotionControlType::velocity:
      monitor_port->print(voltage.q);
      monitor_port->print("\t");
      monitor_port->print(shaft_velocity_sp);
      monitor_port->print("\t");
      monitor_port->println(shaft_velocity);
      break;
    case MotionControlType::angle_openloop:
    case MotionControlType::angle:
      monitor_port->print(voltage.q);
      monitor_port->print("\t");
      monitor_port->print(shaft_angle_sp);
      monitor_port->print("\t");
      monitor_port->println(shaft_angle);
      break;
    case MotionControlType::torque:
      monitor_port->print(voltage.q);
      monitor_port->print("\t");
      monitor_port->print(shaft_angle);
      monitor_port->print("\t");
      monitor_port->println(shaft_velocity);
      break;
  }
}

int FOCMotor::command(String user_command) {
  // error flag
  int errorFlag = 1;
  // if empty string
  if(user_command.length() < 1) return errorFlag;

  // parse command letter
  char cmd = user_command.charAt(0);
  // check if get command
  char GET = user_command.charAt(1) == '\n';
  // parse command values
  float value = user_command.substring(1).toFloat();

  // a bit of optimisation of variable memory for Arduino UNO (atmega328)
  switch(cmd){
    case 'P':      // velocity P gain change
    case 'I':      // velocity I gain change
    case 'D':      // velocity D gain change
    case 'R':      // velocity voltage ramp change
      if(monitor_port) monitor_port->print(F(" PID velocity| "));
      break;
    case 'F':      // velocity Tf low pass filter change
      if(monitor_port) monitor_port->print(F(" LPF velocity| "));
      break;
    case 'K':      // angle loop gain P change
      if(monitor_port) monitor_port->print(F(" PID angle| "));
      break;
    case 'L':      // velocity voltage limit change
    case 'N':      // angle loop gain velocity_limit change
      if(monitor_port) monitor_port->print(F(" Limits| "));
      break;
  }

  // apply the the command
  switch(cmd){
    case 'P':      // velocity P gain change
      if(monitor_port) monitor_port->print("P: ");
      if(!GET) PID_velocity.P = value;
      if(monitor_port) monitor_port->println(PID_velocity.P);
      break;
    case 'I':      // velocity I gain change
      if(monitor_port) monitor_port->print("I: ");
      if(!GET) PID_velocity.I = value;
      if(monitor_port) monitor_port->println(PID_velocity.I);
      break;
    case 'D':      // velocity D gain change
      if(monitor_port) monitor_port->print("D: ");
      if(!GET) PID_velocity.D = value;
      if(monitor_port) monitor_port->println(PID_velocity.D);
      break;
    case 'R':      // velocity voltage ramp change
      if(monitor_port) monitor_port->print("volt_ramp: ");
      if(!GET) PID_velocity.output_ramp = value;
      if(monitor_port) monitor_port->println(PID_velocity.output_ramp);
      break;
    case 'L':      // velocity voltage limit change
      if(monitor_port) monitor_port->print("volt_limit: ");
      if(!GET) {
        voltage_limit = value;
        PID_velocity.limit = value;
      }
      if(monitor_port) monitor_port->println(voltage_limit);
      break;
    case 'F':      // velocity Tf low pass filter change
      if(monitor_port) monitor_port->print("Tf: ");
      if(!GET) LPF_velocity.Tf = value;
      if(monitor_port) monitor_port->println(LPF_velocity.Tf);
      break;
    case 'K':      // angle loop gain P change
      if(monitor_port) monitor_port->print(" P: ");
      if(!GET) P_angle.P = value;
      if(monitor_port) monitor_port->println(P_angle.P);
      break;
    case 'N':      // angle loop gain velocity_limit change
      if(monitor_port) monitor_port->print("vel_limit: ");
      if(!GET){
        velocity_limit = value;
        P_angle.limit = value;
      }
      if(monitor_port) monitor_port->println(velocity_limit);
      break;
    case 'C':
      // change control type
      if(monitor_port) monitor_port->print("Control: ");
      
      if(GET){ // if get command
        switch(controller){
          case MotionControlType::torque:
            if(monitor_port) monitor_port->println("torque");
            break;
          case MotionControlType::velocity:
            if(monitor_port) monitor_port->println("velocity");
            break;
          case MotionControlType::angle:
            if(monitor_port) monitor_port->println("angle");
            break;
          case MotionControlType::velocity_openloop:
            if(monitor_port) monitor_port->println("velocity openloop");
            break;
          case MotionControlType::angle_openloop:
            if(monitor_port) monitor_port->println("angle openloop");
            break;
        }
      }else{ // if set command
        switch((int)value){
          case 0:
            if(monitor_port) monitor_port->println("torque");
            controller = MotionControlType::torque;
            break;
          case 1:
            if(monitor_port) monitor_port->println("velocity");
            controller = MotionControlType::velocity;
            break;
          case 2:
            if(monitor_port) monitor_port->println("angle");
            controller = MotionControlType::angle;
            break;
          case 3:
            if(monitor_port) monitor_port->println("velocity openloop");
            controller = MotionControlType::velocity_openloop;
            break;
          case 4:
            if(monitor_port) monitor_port->println("angle openloop");
            controller = MotionControlType::angle_openloop;
            break;
          default: // not valid command
            if(monitor_port) monitor_port->println("error");
            errorFlag = 0;
        }
      }
      break;
    case 'T':
      // change control type
      if(monitor_port) monitor_port->print("Torque: ");
      
      if(GET){ // if get command
        switch(torque_controller){
          case TorqueControlType::voltage:
            if(monitor_port) monitor_port->println("voltage");
            break;
          case TorqueControlType::current:
            if(monitor_port) monitor_port->println("current");
            break;
          case TorqueControlType::foc_current:
            if(monitor_port) monitor_port->println("foc current");
            break;
        }
      }else{ // if set command
        switch((int)value){
          case 0:
            if(monitor_port) monitor_port->println("voltage");
            torque_controller = TorqueControlType::voltage;
            break;
          case 1:
            if(monitor_port) monitor_port->println("current");
            torque_controller = TorqueControlType::current;
            break;
          case 2:
            if(monitor_port) monitor_port->println("foc current");
            torque_controller = TorqueControlType::foc_current;
            break;
          default: // not valid command
            if(monitor_port) monitor_port->println("error");
            errorFlag = 0;
        }
      }
      break;
    case 'V':     // get current values of the state variables
        switch((int)value){
          case 0: // get voltage
            if(monitor_port) monitor_port->print("Uq: ");
            if(monitor_port) monitor_port->println(voltage.q);
            break;
          case 1: // get velocity
            if(monitor_port) monitor_port->print("Velocity: ");
            if(monitor_port) monitor_port->println(shaft_velocity);
            break;
          case 2: // get angle
            if(monitor_port) monitor_port->print("Angle: ");
            if(monitor_port) monitor_port->println(shaft_angle);
            break;
          case 3: // get angle
            if(monitor_port) monitor_port->print("Target: ");
            if(monitor_port) monitor_port->println(target);
            break;
          default: // not valid command
            errorFlag = 0;
        }
      break;
    default:  // target change
      if(monitor_port) monitor_port->print("Target : ");
      target = user_command.toFloat();
      if(monitor_port) monitor_port->println(target);
  }
  // return 0 if error and 1 if ok
  return errorFlag;
}