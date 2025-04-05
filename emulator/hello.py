from pioemu import emulate
from adafruit_pioasm import assemble

#program = [0xE029, 0x0041, 0x2080]  # Count down from 9 using X register
#program = """
#.program quickstart_example
#  set x 9
#loop:
#  jmp x-- loop
#  wait 1 gpio 0
#"""
program = """
.program kclk
    set pins 1 [7]  ; high for 16 cycles / 40us total, 8 here and 8 on next line
    irq clear 4 [7] ; clear irq then wait another 7 cycles / 20us total to sync KDAT which waits for it to clear before changing
    set pins 0 [7]  ; low for 8 cycles / 20us
"""

generator = emulate(assemble(program), stop_when=lambda _, state: state.x_register < 0)

for before, after in generator:
  print(f"[{after.clock}] X register: {before.x_register} -> {after.x_register}")
