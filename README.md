

# Disclaimer
This is still a prototype, the circuit design was built and tested on a perfboard, the kicad board design was never tested.  use at your risk and plz **don't  come whining if you damage your flip clock or set your house on fire.**


# Metro Blinds Driver


Arduino based driver board for Rome Metropolitana A front and side blinds

**Main features:**   
- (WIP)


# Disclaimer
This is still a prototype, the circuit design was built and tested on a perfboard, the kicad board design was never tested.  use at your risk and plz **don't  come whining if you damage your flip clock or set your house on fire.**


# Metro Blinds Driver


Arduino based driver board for Rome Metropolitana A front and side blinds

**Main features:**   
- (WIP)

## Dependencies


- [Regexp](https://github.com/nickgammon/Regexp)
- [Rocket Scream Low Power](https://github.com/rocketscream/Low-Power)


## Circuit Board


**WORK IN PROGRESS** check the schematic in this repo, I've built and tested it on perfboard, board design might not be 100% correct, and definitely the layout could 
use some improvement.



**How to read led signals**


Either for good or bad news, green led will flash. red will stay on when general enable switch is on


(WIP)


## How to install and adjust


**First things first, let's not fry stuff**

- Check your blind motor wiring, the motor requires rewiring to run at 220V
- Solder the motor N wire to pin #XX
- Wire output pins 1 to 7

(WIP)

- Power the board.
 **No smoke? good.**



By Default the controller will cycle through its default stops sequence


## Enabling motor movement

(WIP)

## Push buttons commands

(WIP)

## Serial port commands



Commands format: **(<<|>>)[A-Z]{1,10}[a-z,A-Z,0-9]{1,20}**

- **<<** to read info, **>>** to set
- command name, in caps
- (where applicable) parameter

One command per line, max 32 chars long. the parser is pretty crude, pls stick to tested commands ;-) 

**Available commands**:

- **<<BOOTTIMESTAMP** prints the curent boot timestamp

- **<<COMPILEDATETIME** print the sketch build timestamp
(WIP)


## Build flags

-DEBUG_MODE enable serial port and LCD debug messages, disbales sleep time  and force reinitialization of eeprom and rtc module on each read


## Troubleshooting

**Q:** I can't tell what's going on with eeprom data

**A:** <<EEPROMDATA command can extracts all the info saved on the eeprom

**Q:** That PCB design is **lame**!

**A:** It is. any help much appreciated! ;-)

## Todo
- tons of stuff still to be done





# Disclaimer
This is still a prototype, the circuit design was built and tested on a perfboard, the kicad board design was never tested.  use at your risk and plz **don't  come whining if you damage your flip clock or set your house on fire.**


# Metro Blinds Driver


Arduino based driver board for Rome Metropolitana A front and side blinds

**Main features:**   
- (WIP)

## Dependencies


- [Regexp](https://github.com/nickgammon/Regexp)
- [Rocket Scream Low Power](https://github.com/rocketscream/Low-Power)


## Circuit Board


**WORK IN PROGRESS** check the schematic in this repo, I've built and tested it on perfboard, board design might not be 100% correct, and definitely the layout could 
use some improvement.



**How to read led signals**


Either for good or bad news, green led will flash. red will stay on when general enable switch is on


(WIP)


## How to install and adjust


**First things first, let's not fry stuff**

- Check your blind motor wiring, the motor requires rewiring to run at 220V
- Solder the motor N wire to pin #XX
- Wire output pins 1 to 7

(WIP)

- Power the board.
 **No smoke? good.**



By Default the controller will cycle through its default stops sequence


## Enabling motor movement

(WIP)

## Push buttons commands

(WIP)

## Serial port commands



Commands format: **(<<|>>)[A-Z]{1,10}[a-z,A-Z,0-9]{1,20}**

- **<<** to read info, **>>** to set
- command name, in caps
- (where applicable) parameter

One command per line, max 32 chars long. the parser is pretty crude, pls stick to tested commands ;-) 

**Available commands**:

- **<<BOOTTIMESTAMP** prints the curent boot timestamp

- **<<COMPILEDATETIME** print the sketch build timestamp
(WIP)


## Build flags

-DEBUG_MODE enable serial port and LCD debug messages, disbales sleep time  and force reinitialization of eeprom and rtc module on each read


## Troubleshooting

**Q:** I can't tell what's going on with eeprom data

**A:** <<EEPROMDATA command can extracts all the info saved on the eeprom

**Q:** That PCB design is **lame**!

**A:** It is. any help much appreciated! ;-)

## Todo
- tons of stuff still to be done




## Dependencies


- [Regexp](https://github.com/nickgammon/Regexp)
- [Rocket Scream Low Power](https://github.com/rocketscream/Low-Power)


## Circuit Board


**WORK IN PROGRESS** check the schematic in this repo, I've built and tested it on perfboard, board design might not be 100% correct, and definitely the layout could 
use some improvement.



**How to read led signals**


Either for good or bad news, green led will flash. red will stay on when general enable switch is on


(WIP)


## How to install and adjust


**First things first, let's not fry stuff**

- Check your blind motor wiring, the motor requires rewiring to run at 220V
- Solder the motor N wire to pin #XX
- Wire output pins 1 to 7

(WIP)

- Power the board.
 **No smoke? good.**



By Default the controller will cycle through its default stops sequence


## Enabling motor movement

(WIP)

## Push buttons commands

(WIP)

## Serial port commands



Commands format: **(<<|>>)[A-Z]{1,10}[a-z,A-Z,0-9]{1,20}**

- **<<** to read info, **>>** to set
- command name, in caps
- (where applicable) parameter

One command per line, max 32 chars long. the parser is pretty crude, pls stick to tested commands ;-) 

**Available commands**:

- **<<BOOTTIMESTAMP** prints the curent boot timestamp

- **<<COMPILEDATETIME** print the sketch build timestamp
(WIP)


## Build flags

-DEBUG_MODE enable serial port and LCD debug messages, disbales sleep time  and force reinitialization of eeprom and rtc module on each read


## Troubleshooting

**Q:** I can't tell what's going on with eeprom data

**A:** <<EEPROMDATA command can extracts all the info saved on the eeprom

**Q:** That PCB design is **lame**!

**A:** It is. any help much appreciated! ;-)

## Todo
- tons of stuff still to be done



