This directory contains the files for fabricating the physical board.
The original version (V1, V1 rev. 2) was fabricated using [JLCPCB](https://jlcpcb.com/), which provides a low cost PCB assembly service.
(5 boards + components ~ $120.)
**If you are going to have these fabricated, you should use the rev. 2 version, which fixes a problem with one of the chip connections!**

All files needed to fabricate the board are in this directory, note that there are two versions of the component placement file -- the one ending in `pos-fixed.csv` has the rotations fixed to JLCPCB's specifications, and this is the file you should use for this board manufacturer.  The similarly named file without the `-fixed` is the raw part placement output from KiCad.

In addition to the surface mount components (which can be installed by JLCPCB), there are a number of through hole components which need to be manually attached.
These are listed below, along with links to Digikey or Amazon to purchase them.
Note that many of these are optional.  
Also not listed are headers for connecting to the peripherial outputs near the center of the board: these are all standard 0.1" pin headers.

**Mandatory Components:**
* Micro-USB connector (1 per board: [Amphenol 10103594-0001LF](https://www.digikey.com/en/products/detail/amphenol-icc-fci/10103594-0001LF/2350351?s=N4IgTCBcDaIIwAZEGYCsBOALCAugXyA))
* DC-DC converter (1 per board: [CUI PDME1-S5-D12-S](https://www.digikey.com/en/products/detail/cui-inc/PDME1-S5-D12-S/10229857) or equivalent)

**Output Connectors:**  (Note: the 8 main outputs are designed to be BNC jacks, but the holes on the board also support attaching a 0.1" header.)
* BNC (up to 8 per board: [TE connectivity 1-1337543-0](https://www.digikey.com/en/products/detail/te-connectivity-amp-connectors/1-1337543-0/1755940) or equivalent.)
* Alternate: 2 pin polarized right angle header (up to 8 per board: [Adam Tech LHA-02-TRB](https://www.digikey.com/en/products/detail/adam-tech/LHA-02-TRB/9830340)) (Mating plug: [Adam Tech MTD-A-02-D-2](https://www.digikey.com/en/products/detail/adam-tech/MTD-A-02-D-2/9830865))
* Serial outputs: 10 pin IDC connectors (up to 2 per board: [On Shore 302-S101](https://www.digikey.com/en/products/detail/on-shore-technology-inc/302-S101/2178422))
* IDC -> DB9 serial connectors (2 per board: [standard DB9 to 10 pin motherboard connector](https://www.amazon.com/CablesOnline-10-Pin-Motherboard-Adapter-AD-I01/dp/B077F2WDZ7/))

**Stand-offs:**
* 1/4"-20 x 1" for mounting the board to an optical table (4 per board: [Essentra 15TSP035](https://www.digikey.com/en/products/detail/essentra-components/15TSP035/11639005))
* 4-40 x 5/8" for mounting a protective top panel (4 per board: [RAF 2059-440-AL](https://www.digikey.com/en/products/detail/raf-electronic-hardware/2059-440-AL/7680412))

**Misc. Optional:**
* Power jack -- only needed if you aren't powering through USB (1 per board: [Wurth 	
694106301002](https://www.digikey.com/en/products/detail/w%C3%BCrth-elektronik/694106301002/5047522))
* Filter capacitors: the analog output amplifiers have empty slots for a through hole capacitor.  
The amplifier resistor is 120 kOhm, so 100 pF gives a -3dB frequency of 13.3 KHz.  
There is already 30 pF attached to the output (CF3/CF13) giving a 44 kHz cutoff; if higher speeds are required you could remove these resistors, but this is not recommended.
If you would like to provide additional filtering, a capacitor could be attachd to CF2/CF12.

* Output resistors: the digital outputs come with a surface mount 50 Ohm resistor installed.  If you would like a different output resistance, you can remove this resistor and add a through hole resistor next to the BNC output.  In most cases this should not be needed.
