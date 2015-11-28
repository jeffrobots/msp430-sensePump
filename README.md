# msp430-sensePump
Implementation of a wireless sensor and a water pump using MSP430 Launchpad (msp430g2553)

This project focuses on the basic control of a water pump using an msp430. Since the pump requires much more voltage and current than the msp430 can provide itself, an interface to the pump needs to be used in order to allow for high current, high voltage applications.

In addition to this, we would like to be able to check water levels before pumping to avoid damage. This is handled using a float switch or water level meter. I will attempt to implement both to see what works best for this application.

I will also work to tie in an existing project, senseRF, in order to obtain information for the water pump wirelessly. 


## Related Projects

The related project, senseRF, uses the Anaren C110L Boosterpack to communicate wirelessly between two msp430 modules. Further documentation on this project can be found [here](https://github.com/jeffrobots/senseRF). 
