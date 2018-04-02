library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity TOP is
port( sys_clk	: in std_logic; -- 100 MHz system clock 
		reset_btn: in std_logic;
		uart_rx	: in std_logic;
		uart_tx	: out std_logic;
		rx_data2 : out std_logic_vector(7 downto 0);
		--Signals pulled on Atlys LEDs for debugging
		wr_en  : out std_logic; --20102016
		rd_en  : out std_logic; --20102016
		full   : out std_logic; --20102016
		empty  : out std_logic); --20102016);
end TOP;

architecture Structural of TOP is

	component uart is
	port (clk 	 : in std_logic;
			rst 	 : in std_logic;
			rx	 	 : in std_logic;
			tx	 	 : out std_logic;
			rx_data1 : out std_logic_vector(7 downto 0);
			--Signals pulled on Atlys LEDs for debugging
			Bwr_en  : out std_logic; --20102016
			Brd_en  : out std_logic; --20102016
			Bfull   : out std_logic; --20102016
			Bempty  : out std_logic); --20102016
	end component uart;	
	signal rst : STD_LOGIC;

begin
	-- UART module												
	--i_uart : uart port map (clk => sys_clk, rst => rst, rx => uart_rx, tx => uart_tx);				
	i_uart : uart port map (clk => sys_clk, rst => rst, rx => uart_rx, tx => uart_tx,rx_data1 => rx_data2, Bwr_en => wr_en, Brd_en => rd_en, Bfull => full, Bempty => empty);  --20102016

	rst <= not(reset_btn);

end Structural;