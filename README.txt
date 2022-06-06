1、eRPC
2、MQTT
3、accelerometer
4、matplot


	My student number is 30, so I do 31 figure.
	The frequency I set is 10 Hz.
	I cut my wave for 20 parts.
2、、2 Thread
	One is adc another is dac.
	When genwave is equal to 1, dac thread generate wave, adc thread sample data.
	When genwave is equal to 0, dac thread and adc thread both do sleep_for.
3、Button
	Wave generation、Data sampling(at this moment, mbed print data)
	Use interrupt and timer for debounce.
4、uLED
	Write in Interrupt funcion of buttons.
	When buttons are pressed, uLCD prints "Button A waveform generation" and "Button B data transfer".