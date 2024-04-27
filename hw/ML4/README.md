This file contains the details for the version 3 (ML3) of the MicroLink hardware.
This is the first version that has a real enclosure so we are paying more attention
to mechanical detail.

# Enclosure Considerations
## Connectors That Penetrate the Enclosure
* DC power connector - 5.5mm jack, male center pin positive.
* Signal/audio connector - DB9, female.
## Indicators That Penetrate the Enclosure
* Three indicator LEDs - D1 Red, D3 blue, D4 green
## Thermal 
* The 3.3V LDO gets warm so there should be airflow in that vicinity

# Change Log (V3)

## R1 (17-Apr-2024)
* Moved Pi Pico W daughter card up by 1.00mm.
* Moved clear area under Pi Pico W daughter card up by 0.75mm.
* Moved J2 (5.5mm power jack) up by 2.5mm.
* Moved J1 (DB9) right by 2.5mm.

## During Build (25-Apr-2024)
* Changed R5 and R5 to 2.2K
* Changed R17, R20, R22 to 100 (LED)

DRC Notes:
* There are 12 warnings: "Silkscreen clipped by board edge."

