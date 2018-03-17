--
-- Copyright (C) 2009-2012 Chris McClelland
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

architecture rtl of swled is
	-- Flags for display on the 7-seg decimal points
	signal flags                   : std_logic_vector(3 downto 0);

	COMPONENT encrypter
	Port ( clock : in  STD_LOGIC; -- clock
           K : in  STD_LOGIC_VECTOR (31 downto 0); -- key
           C : out  STD_LOGIC_VECTOR (31 downto 0); -- encrypted message
           P : in  STD_LOGIC_VECTOR (31 downto 0); -- correct message
           reset : in  STD_LOGIC; -- reset wire
           enable : in  STD_LOGIC); -- enable wire
	END COMPONENT;

	COMPONENT decrypter
	Port ( clock : in  STD_LOGIC; -- clock
           K : in  STD_LOGIC_VECTOR (31 downto 0); -- key
           C : in  STD_LOGIC_VECTOR (31 downto 0); -- encrypted message
           P : out  STD_LOGIC_VECTOR (31 downto 0); -- actual message
           reset : in  STD_LOGIC; -- reset wire
           enable : in  STD_LOGIC); -- enable wire
    END COMPONENT;

	-- Registers implementing the channels
	signal reg0, reg0_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg1, reg1_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg2, reg2_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg3, reg3_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg4, reg4_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg5, reg5_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg6, reg6_next : std_logic_vector(7 downto 0)  := (others => '0');
	signal reg7, reg7_next : std_logic_vector(7 downto 0)  := (others => '0');

	-- Output Registers
	signal out0, out0_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out1, out1_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out2, out2_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out3, out3_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out4, out4_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out5, out5_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out6, out6_next : std_logic_vector(23 downto 0)  := (others => '0');
	signal out7, out7_next : std_logic_vector(23 downto 0)  := (others => '0');

	signal encrypted_info, encrypted_info_next  : std_logic_vector(63 downto 0)  := (others => '0'); -- Storing the encrypted information
	signal decrypted_info : std_logic_vector(63 downto 0)  := (others => '0'); -- Storing the decrypted information
	signal encrypted_cd2, encrypted_cd2_next, decrypted_cd2 : std_logic_vector(31 downto 0)  := (others => '0'); -- Storing the encrypted co-ordinates received from the host
	signal ack1 : std_logic_vector(31 downto 0) := "01010011010001010100111001000100"; -- Acknowledgement 1
	signal ack2 : std_logic_vector(31 downto 0) := "01001110010101010100010001000101"; -- Acknowledgement 2
	signal ack1_encrypted : std_logic_vector(31 downto 0); -- Encrypted Acknowledgement 1
	signal ack2_encrypted, ack2_decrypted, ack2_encrypted_next : std_logic_vector(31 downto 0); -- Storing the encrypted Acknowledgement 2, decrypted Acknowledgement 2
	signal ack2_decrypted1, ack2_encrypted_next1 : std_logic_vector(31 downto 0); -- Storing the encrypted Acknowledgement 2, decrypted Acknowledgement 2
	signal ack2_encrypted1 : std_logic_vector(31 downto 0) := (others => '0'); -- Storing the encrypted Acknowledgement 2
	signal counter, cnt_next : integer := 0; -- Counter for displaying outputs after processing input
	signal counter1, cnt_next1 : std_logic_vector(35 downto 0)  := (others => '0'); -- Counter used for timeout of 256 seconds
	signal counter2, cnt_next2 : integer  := 0; -- Counter which increments when h2fValid_in = '1'
	signal counter3, cnt_next3 : integer  := 0; -- Counter which increments when f2hReady_in = '1'
	signal counter4, cnt_next4 : integer  := 0; -- Counter which increments which waits for decryption of ACK2
	signal led_out1, led_out1_next : std_logic_vector(7 downto 0)  := (others => '0'); -- Displaying led signals
	signal cd_match, cd_match1, cd_match2: std_logic := '0'; -- Checking matching of received co-ordinates and ecnrypted ACK2
	signal encrypted_cd : std_logic_vector(31 downto 0) := (others => '0'); -- encrypted coordinates to send to host
	signal key : std_logic_vector(31 downto 0) := "10001010011001011100101001010101"; -- Key for encryption  
	signal coordinates : std_logic_vector(31 downto 0) := (others => '0'); -- Co-ordinates to be encrypted 
	signal myenable,myenable_next : std_logic := '0'; -- Enable for decrypting received co-ordinates
	signal myenable2,myenable2_next : std_logic := '0'; -- Enable for decrypting encrypted ack2
	signal myenable3,myenable3_next : std_logic := '0'; -- Enable for decrypting first 4 bytes of encrypted information from host 
	signal myenable4,myenable4_next : std_logic := '0'; -- Enable for decrypting next 4 bytes of encrypted information from host
	signal myenable5,myenable5_next : std_logic := '0'; -- Enable for decrypting encrypted ack2
	signal reset1 : std_logic := '0'; -- common reset pin for all encrypters and decrypters
 
	
begin                                                                     --BEGIN_SNIPPET(registers)
	-- Infer registers
	process(clk_in)
	begin
		if ( rising_edge(clk_in) ) then
			if ( reset_in = '1' -- reset the circuit
				or counter = 1536000000 -- 32 seconds (24 seconds for displaying outputs and 8 seconds wait before next input)
				or counter1 = "001011011100011011000000000000000000")  then -- 256 seconds for timeout
				reg0 <= (others => '0');
				reg1 <= (others => '0');
				reg2 <= (others => '0');
				reg3 <= (others => '0');
				reg4 <= (others => '0');
				reg5 <= (others => '0');
				reg6 <= (others => '0');
				reg7 <= (others => '0');
				out0 <= (others => '0');
				out1 <= (others => '0');
				out2 <= (others => '0');
				out3 <= (others => '0');
				out4 <= (others => '0');
				out5 <= (others => '0');
				out6 <= (others => '0');
				out7 <= (others => '0');
				led_out1 <= (others => '0');
				counter <= 0;
				counter1 <= (others => '0');
				counter2 <= 0;
				counter3 <= 0;
				counter4 <= 0;
				myenable <= '0';
				myenable2 <= '0';
				myenable3 <= '0';
				myenable4 <= '0';
				myenable5 <= '0';
				reset1 <= '1';
			else
				reg0 <= reg0_next;
				reg1 <= reg1_next;
				reg2 <= reg2_next;
				reg3 <= reg3_next;
				reg4 <= reg4_next;
				reg5 <= reg5_next;
				reg6 <= reg6_next;
				reg7 <= reg7_next;
				out0 <= out0_next;
				out1 <= out1_next;
				out2 <= out2_next;
				out3 <= out3_next;
				out4 <= out4_next;
				out5 <= out5_next;
				out6 <= out6_next;
				out7 <= out7_next;
				led_out1 <= led_out1_next;
				myenable <= myenable_next;
				myenable2 <= myenable2_next;
				myenable3 <= myenable3_next;
				myenable4 <= myenable4_next;
				myenable5 <= myenable5_next;
				counter <= cnt_next;
				counter1 <= cnt_next1;
				counter2 <= cnt_next2;
				counter3 <= cnt_next3;
				counter4 <= cnt_next4;
				encrypted_cd2 <= encrypted_cd2_next;
				ack2_encrypted <= ack2_encrypted_next;
				ack2_encrypted1 <= ack2_encrypted_next1;
				encrypted_info <= encrypted_info_next;
				reset1 <= '0';
			end if;
		end if;
	end process;

	-- Assert that there's always data for reading, and always room for writing
	f2hValid_out <= '1';
	h2fReady_out <= '1';

	cnt_next1 <=
		counter1 + 1 when (counter3 = 4 and cd_match = '0') or (counter3 = 8 and cd_match1 = '0' ) or (counter3 = 16 and cd_match2 = '0')
		else (others => '0');
	cnt_next2 <=
		counter2 + 1 when h2fValid_in = '1'
		else counter2;
	cnt_next3 <= 
		counter3 + 1 when f2hReady_in = '1'
		else counter3;

	coordinates <=  "00100010000000000000000000000000";
	enc1 : encrypter
	port map (clk_in, key, encrypted_cd, coordinates, reset1, '1');
	enc2 : encrypter
	port map (clk_in, key, ack1_encrypted, ack1, reset1, '1');

	-- Select values to return for each channel when the host is reading
	f2hData_out <=
		encrypted_cd(7 downto 0) when chanAddr_in = "0000000" and counter3 = 0
		else encrypted_cd(15 downto 8) when chanAddr_in = "0000000" and counter3 = 1
		else encrypted_cd(23 downto 16) when chanAddr_in = "0000000" and counter3 = 2
		else encrypted_cd(31 downto 24) when chanAddr_in = "0000000" and counter3 = 3
		else ack1_encrypted(7 downto 0) when chanAddr_in = "0000000" and counter3 = 4 and cd_match = '1'
		else ack1_encrypted(15 downto 8) when chanAddr_in = "0000000" and counter3 = 5 and cd_match = '1'
		else ack1_encrypted(23 downto 16) when chanAddr_in = "0000000" and counter3 = 6 and cd_match = '1'
		else ack1_encrypted(31 downto 24) when chanAddr_in = "0000000" and counter3 = 7 and cd_match = '1'
		else ack1_encrypted(7 downto 0) when chanAddr_in = "0000000" and counter3 = 8 
		else ack1_encrypted(15 downto 8) when chanAddr_in = "0000000" and counter3 = 9
		else ack1_encrypted(23 downto 16) when chanAddr_in = "0000000" and counter3 = 10
		else ack1_encrypted(31 downto 24) when chanAddr_in = "0000000" and counter3 = 11
		else ack1_encrypted(7 downto 0) when chanAddr_in = "0000000" and counter3 = 12
		else ack1_encrypted(15 downto 8) when chanAddr_in = "0000000" and counter3 = 13
		else ack1_encrypted(23 downto 16) when chanAddr_in = "0000000" and counter3 = 14
		else ack1_encrypted(31 downto 24) when chanAddr_in = "0000000" and counter3 = 15
		else x"AB";

	-- read and store encrypted data written by host on channel 1 in encrypted_cd2
	-- Each time the host writes h2fValid_in becomes '1' and counter2 increases by 1 ensuring that data is stored sequentially in encrypted_cd2.
	
	encrypted_cd2_next(7 downto 0) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 0 
		else encrypted_cd2(7 downto 0);

	encrypted_cd2_next(15 downto 8) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 1
		else encrypted_cd2(15 downto 8);

	encrypted_cd2_next(23 downto 16) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 2
		else encrypted_cd2(23 downto 16);

	encrypted_cd2_next(31 downto 24) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 3
		else encrypted_cd2(31 downto 24);

	-- set myenable to '1' while board is decrypting and counter2 stays 4
	myenable_next <= 
		'1' when counter2 = 4
		else myenable;

	-- myenable drives the decrypter dec1 for board to decrypt the received encrypted coordinates  
	dec1 : decrypter
	port map (clk_in, key, encrypted_cd2, decrypted_cd2, reset1, myenable);

	-- cd_match becomes '1' when board finishes decryption and the decrypted coordinates received from host matches the original coordinates sent.
	cd_match <=
		'1' when decrypted_cd2(31 downto 0) = coordinates(31 downto 0)	
		else '0';

	-- read encrypted Ack2 on channel 1 sent by host sequentially driven by counter2 which increases from 4 to 7 whenever host writes on channel 1
	ack2_encrypted_next(7 downto 0) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 4 
		else ack2_encrypted(7 downto 0);

	ack2_encrypted_next(15 downto 8) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 5 
		else ack2_encrypted(15 downto 8);
    
    ack2_encrypted_next(23 downto 16) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 6 
		else ack2_encrypted(23 downto 16);
    
    ack2_encrypted_next(31 downto 24) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 7 
		else ack2_encrypted(31 downto 24);

	-- myenable2 drives the decrypter dec2 and becomes '1' whenever host finishes writing
	myenable2_next <= 
		'1' when counter2 = 8
		else myenable2;
		
	-- board is decrypting ack2_encrypted while counter2 stays at 8.
	dec2 : decrypter
	port map (clk_in, key, ack2_encrypted, ack2_decrypted, reset1, myenable2);
	
	-- signal which becomes '1' as soon as board finishes decryption and the decrypted Ack2 matches the chosen Ack2.
	cd_match1 <=
		'1' when ack2_decrypted(31 downto 0) = ack2(31 downto 0)
		else '0';

	-- 
	encrypted_info_next(7 downto 0) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 8 and cd_match1 = '1'
		else encrypted_info(7 downto 0);
	encrypted_info_next(15 downto 8) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 9 and cd_match1 = '1'
		else encrypted_info(15 downto 8);
	encrypted_info_next(23 downto 16) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 10 and cd_match1 = '1'
		else encrypted_info(23 downto 16);
	encrypted_info_next(31 downto 24) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 11 and cd_match1 = '1'
		else encrypted_info(31 downto 24);

	myenable3_next <= 
		'1' when counter2 = 12
		else myenable3;

	dec3 : decrypter
	port map (clk_in, key, encrypted_info(31 downto 0), decrypted_info(31 downto 0), reset1, myenable3);

	encrypted_info_next(39 downto 32) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 12
		else encrypted_info(39 downto 32);
	encrypted_info_next(47 downto 40) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 13
		else encrypted_info(47 downto 40);
	encrypted_info_next(55 downto 48) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 14
		else encrypted_info(55 downto 48);
	encrypted_info_next(63 downto 56) <=
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 15
		else encrypted_info(63 downto 56);
		
	myenable4_next <= 
		'1' when counter2 = 16
		else myenable4;

	dec4 : decrypter
	port map (clk_in, key, encrypted_info(63 downto 32), decrypted_info(63 downto 32), reset1, myenable4);

	ack2_encrypted_next1(7 downto 0) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 16 
		else ack2_encrypted1(7 downto 0);

	ack2_encrypted_next1(15 downto 8) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 17
		else ack2_encrypted1(15 downto 8);
    
    ack2_encrypted_next1(23 downto 16) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 18
		else ack2_encrypted1(23 downto 16);
    
    ack2_encrypted_next1(31 downto 24) <= 
		h2fData_in when chanAddr_in = "0000001" and h2fValid_in = '1' and counter2 = 19
		else ack2_encrypted1(31 downto 24);

	myenable5_next <= 
		'1' when counter2 = 20
		else myenable5;

	cnt_next4 <= 
		counter4 + 1 when myenable5 = '1'
		else counter4;

	dec5 : decrypter
	port map (clk_in, key, ack2_encrypted1, ack2_decrypted1, reset1, myenable5);

	cd_match2 <=
		'1' when ack2_decrypted1(31 downto 0) = ack2(31 downto 0)
		else '0';


	-- Drive register inputs for each channel when the host is writing
	reg0_next <=
		decrypted_info(7 downto 0) when counter4 >= 33 and cd_match2 = '1'
		else reg0;
	reg1_next <=
		decrypted_info(15 downto 8) when counter4 >= 33 and cd_match2 = '1'
		else reg1;
	reg2_next <=
		decrypted_info(23 downto 16) when counter4 >= 33 and cd_match2 = '1'
		else reg2;
	reg3_next <=
		decrypted_info(31 downto 24) when counter4 >= 33 and cd_match2 = '1'
		else reg3;
	reg4_next <=
		decrypted_info(39 downto 32) when counter4 >= 33 and cd_match2 = '1'
		else reg4;
	reg5_next <=
		decrypted_info(47 downto 40) when counter4 >= 33 and cd_match2 = '1'
		else reg5;
	reg6_next <=
		decrypted_info(55 downto 48) when counter4 >= 33 and cd_match2 = '1'
		else reg6;
	reg7_next <=
		decrypted_info(63 downto 56) when counter4 >= 33 and cd_match2 = '1'
		else reg7;

	cnt_next <=
		counter + 1 when counter4 >= 40
		else counter;
	
	-- for first four directions the lights displayed will remain same for 3 seconds since according to given conditions since amber
	-- light can only be displayed for the last four directions.
	
	
	-- In direction 0 (north) the light will be red if either track does not exist or is not OK or if there is a train in opposite direction
	-- (south) or a train is not coming from north.
	out0_next <=
		"000001000000010000000100" when reg0(7) = '1' and reg0(6) = '1' and sw_in(0) = '1' and sw_in(4) = '0'
		else "000000010000000100000001";
		
	-- In direction 1 (north-east) the light will be red if either track does not exist or is not OK or if there is a train in opposite direction
	-- (south-west) or a train is not coming from north-east.
	out1_next <=
		"001001000010010000100100" when reg1(7) = '1' and reg1(6) = '1' and sw_in(1) = '1' and sw_in(5) = '0'
		else "001000010010000100100001";
	
	-- In direction 2 (east) the light will be red if either track does not exist or is not OK or if there is a train in opposite direction
	-- (west) or a train is not coming from north.
	out2_next <=
		"010001000100010001000100" when reg2(7) = '1' and reg2(6) = '1' and sw_in(2) = '1' and sw_in(6) = '0'
		else "010000010100000101000001";
		
	
	-- In direction 3 (south-east) the light will be red if either track does not exist or is not OK or if there is a train in opposite direction
	-- (north-west) or a train is not coming from north.
	out3_next <=
		"011001000110010001100100" when reg3(7) = '1' and reg3(6) = '1' and sw_in(3) = '1' and sw_in(7) = '0'
		else "011000010110000101100001";
	
	
	-- In direction 4 (south) the lights may be different for the 3 seconds.
	
	-- In direction 4(south) light is red for the 1st sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction
	out4_next(7 downto 0) <=
		"10000001" when reg4(7) = '0' or reg4(6) = '0' or sw_in(4) = '0'
		else "10000100";
	
	-- In direction 4(south) light is red for the 2nd sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction, amber if train is coming from opposite direction, else it shows green.
	out4_next(15 downto 8) <=
		"10000001" when reg4(7) = '0' or reg4(6) = '0' or sw_in(4) = '0'
		else "10000010" when reg4(7) = '1' and reg4(6) = '1' and sw_in(4) = '1' and sw_in(0) = '1'
		else "10000100";

	-- In direction 4(south) light is green for the 3rd sec if train is either track exists and track is OK and train is coming from
	-- this direction, else it shows red.
	out4_next(23 downto 16) <=
		"10000100" when reg4(7) = '1' and reg4(6) = '1' and sw_in(4) = '1' and sw_in(0) = '0'
		else "10000001";

	-- In direction 5 (south-west) the lights may be different for the 3 seconds.
	
	-- In direction 5(south-west) light is red for the 1st sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction
	out5_next(7 downto 0) <=
		"10000001" when reg5(7) = '0' or reg5(6) = '0' or sw_in(5) = '0'
		else "10000100";
		
	-- In direction 5(south-west) light is red for the 2nd sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction, amber if train is coming from opposite direction, else it shows green.	
	out5_next(15 downto 8) <=
		"10000001" when reg5(7) = '0' or reg5(6) = '0' or sw_in(5) = '0'
		else "10000010" when reg5(7) = '1' and reg5(6) = '1' and sw_in(5) = '1' and sw_in(1) = '1'
		else "10000100";

	-- In direction 5(south-west) light is green for the 3rd sec if train is either track exists and track is OK and train is coming from
	-- this direction, else it shows red.
	out5_next(23 downto 16) <=
		"10000100" when reg5(7) = '1' and reg5(6) = '1' and sw_in(5) = '1' and sw_in(1) = '0'
		else "10000001";
		
	-- In direction 6 (west) the lights may be different for the 3 seconds.
	
	-- In direction 6(west) light is red for the 1st sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction
	out6_next(7 downto 0) <=
		"10000001" when reg6(7) = '0' or reg6(6) = '0' or sw_in(6) = '0'
		else "10000100";

	-- In direction 6(west) light is red for the 2nd sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction, amber if train is coming from opposite direction, else it shows green.
	out6_next(15 downto 8) <=
		"10000001" when reg6(7) = '0' or reg6(6) = '0' or sw_in(6) = '0'
		else "10000010" when reg6(7) = '1' and reg6(6) = '1' and sw_in(6) = '1' and sw_in(2) = '1'
		else "10000100";

	-- In direction 6(west) light is green for the 3rd sec if train is either track exists and track is OK and train is coming from
	-- this direction, else it shows red.
	out6_next(23 downto 16) <=
		"10000100" when reg6(7) = '1' and reg6(6) = '1' and sw_in(6) = '1' and sw_in(2) = '0'
		else "10000001";

	-- In direction 7 (north-west) the lights may be different for the 3 seconds.
	
	-- In direction 7(north-west) light is red for the 1st sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction
	out7_next(7 downto 0) <=
		"10000001" when reg7(7) = '0' or reg7(6) = '0' or sw_in(7) = '0'
		else "10000100";

	-- In direction 7(north-west) light is red for the 2nd sec if train is either track does not exist or track is not OK or train is not coming from
	-- this direction, amber if train is coming from opposite direction, else it shows green.
	out7_next(15 downto 8) <=
		"10000001" when reg7(7) = '0' or reg7(6) = '0' or sw_in(7) = '0'
		else "10000010" when reg7(7) = '1' and reg7(6) = '1' and sw_in(7) = '1' and sw_in(3) = '1'
		else "10000100";

	-- In direction 7(north-west) light is green for the 3rd sec if train is either track exists and track is OK and train is coming from
	-- this direction, else it shows red.
	out7_next(23 downto 16) <=
		"10000100" when reg7(7) = '1' and reg7(6) = '1' and sw_in(7) = '1' and sw_in(3) = '0'
		else "10000001";

	-- LEDs and 7-seg display
	with counter select led_out1_next <=
		out0(7 downto 0)      when 1,
		out0(15 downto 8)     when 48000000,
		out0(23 downto 16)    when 96000000,
		out1(7 downto 0)      when 144000000,
		out1(15 downto 8)     when 192000000,
		out1(23 downto 16)    when 240000000,
		out2(7 downto 0)      when 288000000,
		out2(15 downto 8)     when 336000000,
		out2(23 downto 16)    when 384000000,
		out3(7 downto 0)      when 432000000,
		out3(15 downto 8)     when 480000000,
		out3(23 downto 16)    when 528000000,
		out4(7 downto 0)      when 576000000,
		out4(15 downto 8)     when 624000000,
		out4(23 downto 16)    when 720000000,
		out5(7 downto 0)      when 768000000,
		out5(15 downto 8)     when 816000000,
		out5(23 downto 16)    when 864000000,
		out6(7 downto 0)      when 912000000,
		out6(15 downto 8)     when 960000000,
		out6(23 downto 16)    when 1008000000,
		out7(7 downto 0)      when 1056000000,
		out7(15 downto 8)     when 1104000000,
		out7(23 downto 16)    when 1152000000,
		led_out1  when others;
		
	led_out <= led_out1;
	flags <= "00" & f2hReady_in & reset_in;
	seven_seg : entity work.seven_seg
		port map(
			clk_in     => clk_in,
			data_in    => "0000000000000000",
			dots_in    => flags,
			segs_out   => sseg_out,
			anodes_out => anode_out
		);

end architecture;
