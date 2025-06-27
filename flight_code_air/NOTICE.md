NOTE: There is a slight modification to motor_control.c and ec_motor.c (and their header files)
	- Instead of using mvprint to print motor status, those fuctions that previously supported printing to screen now accepts an array as a parameter, and put the previously printed values to the array.
	- The array is then transferred to cli_oph.c, and downlinked to cli_ground.
