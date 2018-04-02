library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity TB is
end TB;

architecture Behavioral of TB is

	component baudrate_gen is
    port (  clk		: in std_logic;
            rst		: in std_logic;
            sample	: out std_logic);
	end component baudrate_gen;
	signal clk, rst, reset_btn, sample : STD_LOGIC;
	signal tickcntr : integer;
	
	component uart is
	port (clk 	 : in std_logic;
			rst 	 : in std_logic;
			rx	 	 : in std_logic;
			tx	 	 : out std_logic;
			--Signals pulled on Atlys LEDs for debugging
			Bwr_en  : out std_logic; --20102016
			Brd_en  : out std_logic; --20102016
			Bfull   : out std_logic; --20102016
			Bempty  : out std_logic); --20102016
	end component uart;
	signal rx, tx : STD_LOGIC;
	signal wr_en, rd_en, full, empty : std_logic;  --20102016

	type uart_frame is array (0 to 3) of STD_LOGIC_VECTOR (9 downto 0);
	constant uart_data : uart_frame := (0 => '1' & "00000000" & '0' ,
													1 => '1' & "11111111" & '0' ,
													2 => '1' & "11110000" & '0' ,
													3 => '1' & "10101010" & '0' );
	--signal rx_stim : STD_LOGIC_VECTOR (9 downto 0) := '1' & X"FF" & '0';

begin
	brg : baudrate_gen port map (clk => clk, rst => rst, sample => sample);

	--i_uart : uart port map (clk => clk, rst => rst, rx => rx, tx => tx);
	i_uart : uart port map (clk => clk, rst => rst, rx => rx, tx => tx, Bwr_en => wr_en, Brd_en => rd_en, Bfull => full, Bempty => empty);  --20102016				
	
	gen_clk : process	-- 100 MHz
	begin
		clk <= '0';
		wait for 5 ns;
		clk <= '1';
		wait for 5 ns;
	end process gen_clk;

	gen_rst : process
	begin
		reset_btn <= '0';
		wait for 15 ns;
		reset_btn <= '1';
		wait for 1000 ms;
	end process gen_rst;
	
	rst <= not(reset_btn);
	
	stimulus : process
		procedure tick16 (signal sample		: in std_logic;
								signal tickcntr	: inout integer) is
		begin
			tickcntr <= 0;
			for i in 0 to 15 loop
				wait until rising_edge(sample);
				tickcntr <= tickcntr + 1;
			end loop;
		end procedure tick16;
	begin
		-- Send 10bits on Rx
		for k in 0 to 49 loop
		for j in 0 to 3 loop
		for i in 0 to 9 loop
			tick16 (sample, tickcntr);
			if tickcntr = 15 then
				rx <= uart_data (j) (i);
				--rx <= rx_stim(i);
			end if;
		end loop;
		end loop;
		end loop;
		wait;
	end process stimulus;

end Behavioral;