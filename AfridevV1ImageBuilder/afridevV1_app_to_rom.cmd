CCS_AfridevV1_MSP430.out
--image
--memwidth 16
--ti_txt
--order MS 

ROMS
{
	EPROM1: org = 0xC000, len = 0x3000, romwidth = 16, fill = 0xFFFF
		files = { afridevV1_MSP430_rom.txt }
}
