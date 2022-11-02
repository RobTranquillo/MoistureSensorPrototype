"""
Functions to easy control LED
"""

# general imports
from machine import Pin, PWM
from time import sleep


def SmoothBlink():
    frequency = 5000
    led = PWM(Pin(2), frequency)

    while True:
        for duty_cycle in range(0, 1024):
            led.duty(duty_cycle)
            sleep(0.005)
        for duty_cycle in reversed(range(0, 1024)):
            led.duty(duty_cycle)
            sleep(0.005)    