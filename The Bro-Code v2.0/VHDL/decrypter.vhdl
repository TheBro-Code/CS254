----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    05:19:03 01/23/2018 
-- Design Name: 
-- Module Name:    decrypter - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;
 
entity decrypter is
     Port ( clock : in  STD_LOGIC; -- clock
           K : in  STD_LOGIC_VECTOR (31 downto 0); -- key
           C : in  STD_LOGIC_VECTOR (31 downto 0); -- encrypted message
           P : out  STD_LOGIC_VECTOR (31 downto 0); -- actual message
           reset : in  STD_LOGIC; -- reset wire
           enable : in  STD_LOGIC); -- enable wire      
end decrypter;

architecture Behavioral of decrypter is
	
	signal counter : integer := 0; -- singal used for ensuring that XOR with TTTTTTTT is taken N0 times
	signal T : std_logic_vector (3 downto 0); -- 4 bit signal used for decryption
	signal temp : std_logic_vector (31 downto 0); -- signal used for temporarily storing the output of the XOR's 
	signal T1 : std_logic_vector (3 downto 0); -- signal used for storing the initial value of T
	signal init : std_logic := '0';
	
begin
-- computing initial value of T and storing in T1
	T1(3) <= K(31) xor K(27) xor K(23) xor K(19) xor K(15) xor K(11) xor K(7) xor K(3);
	T1(2) <= K(30) xor K(26) xor K(22) xor K(18) xor K(14) xor K(10) xor K(6) xor K(2);
	T1(1) <= K(29) xor K(25) xor K(21) xor K(17) xor K(13) xor K(9) xor K(5) xor K(1);
	T1(0) <= K(28) xor K(24) xor K(20) xor K(16) xor K(12) xor K(8) xor K(4) xor K(0);
	
	-- defining what to preform when value of any one input in the sensitivity list changes
	process(clock, reset, enable, counter, temp)
	 begin
	 -- defining what to do if reset pin is enabled
		if (reset = '1') then
			init <= '0';
			counter <= 0;
		-- defining what to do when clock tick happens and enable pin is 1.
		elsif (clock'event and clock = '1') then
			if  (enable = '1' and counter /= 32) then
				-- this if statement ensures that the XOR's are taken only N0 times.
				if(init = '0') then
					P <= "00000000000000000000000000000000";
					T <= T1 + "1111";
					temp <= C;
					init <= '1';
				end if;
				if(k(counter) = '0' and init = '1') then
					temp <=  temp xor (T & T & T & T & T & T & T & T);
					T <= T + "1111";
				end if;
				if (init = '1') then
				counter <= counter + 1;
				end if;
			end if;
		end if;	
		-- storing the final value of temp in P
		P <=  temp;
	 end process;

end Behavioral;