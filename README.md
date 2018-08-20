Date: Fall 2016

Authors:  
Greyson Wang  
Ricardo Dupouy  
Bradley Dairymaple  
Alastair Scott

Implements a VGA and UART driver that is used to create a graphical preformance monitor that runs on an embedded linux (petalinux) system.

The timer driver will use the clock in the Zedboard to keep track of time, and send an asynchronous notification to the user level program every set interval, which will trigger the user level interrupt service routine that updates the display.

The VGA driver will cut pixel blocks of letters and paste them onto the screen to form sentences.

Statitics for read/write to the Kermit console will be tracked and displayed on the screen.  These statistics are updated every timer interrupt.
