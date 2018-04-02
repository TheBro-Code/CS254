library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity uart is
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
end uart;

architecture Structural of uart is

	component baudrate_gen is
	port (clk	: in std_logic;
			rst	: in std_logic;
			sample: out std_logic);
	end component baudrate_gen;
	signal sample : std_logic;
	
	component uart_rx is			
	port (clk	: in std_logic;
			rst	: in std_logic;
			rx		: in std_logic;
			sample: in STD_LOGIC;
			rxdone: out std_logic;
			rxdata: out std_logic_vector(7 downto 0));
	end component uart_rx;
	signal rxdone : std_logic;
	signal rxdata : std_logic_vector(7 downto 0);
	
	component uart_tx is			
	port (clk    : in std_logic;
			rst    : in std_logic;
			txstart: in std_logic;
			sample : in std_logic;
			txdata : in std_logic_vector(7 downto 0);
			txdone : out std_logic;
			tx	    : out std_logic);
	end component uart_tx;
   signal txstart: std_logic;
   signal txdata : std_logic_vector(7 downto 0);
	signal txdone : std_logic; --tx rdy to get new byte 

	COMPONENT fifo
	PORT ( clk : IN STD_LOGIC;
			 srst : IN STD_LOGIC;
			 din : IN STD_LOGIC_VECTOR(7 DOWNTO 0);
			 wr_en : IN STD_LOGIC;
			 rd_en : IN STD_LOGIC;
			 dout : OUT STD_LOGIC_VECTOR(7 DOWNTO 0);
			 full : OUT STD_LOGIC;
			 empty : OUT STD_LOGIC);
	END COMPONENT;
	signal wr_en, rd_en, full, empty : std_logic;
   signal din, dout : std_logic_vector(7 downto 0);

	signal flag : std_logic;

begin
	i_brg : baudrate_gen port map (clk => clk, rst => rst, sample => sample);
	
	i_rx : uart_rx port map( clk => clk, rst => rst,
                            rx => rx, sample => sample,
                            rxdone => rxdone, rxdata => rxdata);
									
	i_tx : uart_tx port map( clk => clk, rst => rst,
                            txstart => txstart,
                            sample => sample, txdata => txdata,
                            txdone => txdone, tx => tx);
									 
	i_fifo : fifo PORT MAP ( clk => clk,
									 srst => rst,
									 din => din,
									 wr_en => wr_en,
									 rd_en => rd_en,
									 dout => dout,
									 full => full,
									 empty => empty);
									 
---on 20102016 Signals pulled on Atlys LEDs for debugging
	process(clk)
	begin
		if rising_edge(clk) then
			Bwr_en <= wr_en;
			Brd_en <= rd_en;
			Bfull  <= full;
			Bempty <= empty;
			rx_data1 <= rxdata;
		end if;
	end process;

----loopback logic
	 p_wr : process(clk,rst,full,rxdone)
	 begin
		if rst = '1' then
			wr_en <= '0';
		else
			if rising_edge(clk) then
				if full = '0' then
					din <= rxdata + "00000001";
					if rxdone = '1' then
						wr_en <= '1';
					else
						wr_en <= '0';
					end if;
				end if;		
			end if;
		end if;
	end process p_wr;
	
-- standard procedure to generate 1 pulse	
	p_flag : process(clk,rst,wr_en)
	begin
		if rst = '1' then
			flag <= '0';
		else
			if rising_edge(clk) then
				if wr_en = '1' then
					flag <= '1';
				else
					flag <= '0';
				end if;
			end if;
		end if;
	end process p_flag;
	
	rd_en <= '1' when (empty = '0' and txdone = '1') else -- purely combinational logic
				'0';
				
	p_rd : process(clk,rst,empty,txdone,flag,dout)
	begin
------some data remains in FIFO. So empty = 0. Hence after long time full = 1. Also, seq+comb logic
--		if rst = '1' then
--			rd_en <= '0';
--			--dout <= (others => '0');
--		else
--			if rising_edge(clk) then
--				if empty = '0' then
--					if txdone = '1' then
--						if flag = '1' then
--							rd_en <= '1';
--						else
--							rd_en <= '0';
--						end if;
--					end if;
--				end if;
--			end if;
--		end if;

		txdata <= dout;
	end process p_rd;
	
	p_TXstart : process(clk,rst,rd_en)
	begin
		if rst = '1' then
			txstart <= '0';
		else	
			if rising_edge(clk) then
				if rd_en = '1' then
					txstart <= '1';
				else
					txstart <= '0';
				end if;
			end if;
		end if;
	end process p_TXstart;
		
end Structural;