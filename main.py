#std lib 
#import time

#custom libs
# import leds
# leds.SmoothBlink()

# read analog pin
from time import sleep
from machine import Pin, PWM, ADC   
analog = ADC(0)

while True:
    print(analog.read())
    sleep(1)
