# CS254
The Bro-Code v2.0

Team Members

Aman Jain - 160050034
Sushil Khyalia - 160050035
Kartik Khandelwal - 160070025
Syamantak Kumar - 16D070025

NOTE :- The coordinates are fixed as (2,2) in the code.
	The channels being used for communication are :- (1) Channel 2 for fpga -> host communication
                                                     (2) Channel 3 for host -> fpga communication

The explanation for the code are included in the comments in cksum_rtl.vhdl and main.c

We have chosen following values for the acknowledgments and key 

 Ack1 : 01010011010001010100111001000100
 Ack2 : 01001110010101010100010001000101
 Key :  10001010011001011100101001010101

How to Run : 

-> The Mandatory Part

	We have made changes(and included them in the submission) in the following files :- 

	1) main.c
	2) cksum_rtl.vhdl
	3) encrypter.vhdl
	4) decrypter.vhdl
	5) hdlmake.cfg (in the same directory as cksum_rtl.vhdl)
	6) board.ucf has not been changed
	7) debouncer.vhd (given in the earlier labs)
	8) basic_uart.vhd
	9) t_serial.vhd

	So in order to run our code, replace the following files 

	1) main.c in ~/20140524/makestuff/apps/flcli.
	2) cksum_rtl.vhdl, hdlmake.cfg in ~/20140524/makestuff/hdlmake/apps/makestuff/swled/cksum/vhdl/.
	3) Also include encrypter.vhdl and decrypter.vhdl in ~/20140524/makestuff/hdlmake/apps/makestuff/swled/cksum/vhdl/
	4) Place the "network.txt" file in same directory as the cksum_rtl.vhdl

	After that run these commands in order

	cd ~/20140524/makestuff/apps/flcli
	make deps
	cd ~/20140524/makestuff/hdlmake/apps/makestuff/swled/cksum/vhdl/
	../../../../../bin/hdlmake.py -t ../../templates/fx2all/vhdl -b atlys -p fpga
	sudo ../../../../../../apps/flcli/lin.x64/rel/flcli -v 1d50:602b:0002 -i 1443:0007
	sudo ../../../../../../apps/flcli/lin.x64/rel/flcli -v 1d50:602b:0002 -p J:D0D2D3D4:fpga.xsvf
	sudo ../../../../../../apps/flcli/lin.x64/rel/flcli -v 1d50:602b:0002 -r 

	The "-r" flag is made to start the automatic main.c functionality.
	If manual input-output is required use the "-s" flag which was previously used.

	We have already included the print commands in main.c file which can be seen on terminal during execution which direct the control flow. 

-> The Optional Part
    	
   Apart from the above mentioned files, we have written a python script for the relay laptop, uart.py.

   To run the project, perform the following steps :- 
  
   1) Program both the FPGA boards separately through the steps mentioned in the Mandatory part.
   2) Connect them to two laptops which perform the backend control for each of the boards through the USB ports.
   3) Connect both the boards to the relay laptop through the UART ports.
   4) Run the uart.py on the relay laptop and send information through the UART ports on the boards using the left-right buttons. 
