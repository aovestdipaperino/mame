// license:BSD-3-Clause
// copyright-holders: Aaron Giles, Enzo Lombardi
/***************************************************************************

    Model Racing Dribbling hardware
    MEF00284 main board + MEF00277 'counter drive' board + CS283 player control

    driver by Aaron Giles

    Games supported:
        * Dribbling

    TODO:
    * Implement the 2 banks of 8 DIP switches which determine coinage for the 2 players
    * Actual game duration doesn't match the time reported in the manual

****************************************************************************

    Memory map

****************************************************************************

    ========================================================================
    CPU #1
    ========================================================================
    tbd
    ========================================================================
    Interrupts:
        NMI not connected
        IRQ generated by VBLANK
    ========================================================================

***************************************************************************/

#include "emu.h"

#include "nl_dribling.h"

#include "cpu/z80/z80.h"
#include "machine/i8255.h"
#include "machine/netlist.h"
#include "machine/watchdog.h"

#include "emupal.h"
#include "screen.h"
#include "speaker.h"


// configurable logging
#define LOG_MISC     (1U << 1)
#define LOG_SOUND    (1U << 2)

//#define VERBOSE (LOG_GENERAL | LOG_MISC | LOG_SOUND)

#include "logmacro.h"

#define LOGMISC(...)     LOGMASKED(LOG_MISC,     __VA_ARGS__)
#define LOGSOUND(...)    LOGMASKED(LOG_SOUND,    __VA_ARGS__)


namespace {

class dribling_state : public driver_device
{
public:
	dribling_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_watchdog(*this, "watchdog"),
		m_ppi8255(*this, "ppi8255%d", 0),
		m_videoram(*this, "videoram"),
		m_colorram(*this, "colorram"),
		m_matrix(*this, "MUX%u", 0),
		m_proms(*this, "proms"),
		m_gfxroms(*this, "gfx"),
		m_i_pb(*this, "snd_nl:pb%u", 0U),
		m_i_folla_b(*this, "snd_nl:folla_b"),
		m_i_folla_m(*this, "snd_nl:folla_m"),
		m_i_folla_a(*this, "snd_nl:folla_a"),
		m_i_calcio_b(*this, "snd_nl:calcio_b"),
		m_i_fischio(*this, "snd_nl:fischio"),
		m_i_calcio_a(*this, "snd_nl:calcio_a"),
		m_i_contrasto(*this, "snd_nl:contrasto"),
		m_i_stop_palla(*this, "snd_nl:stop_palla"),
		m_i_parata(*this, "snd_nl:parata"),
		m_i_enable(*this, "snd_nl:enable")
	{ }

	void dribling(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	// devices
	required_device<cpu_device> m_maincpu;
	required_device<watchdog_timer_device> m_watchdog;
	required_device_array<i8255_device, 2>  m_ppi8255;

	// memory pointers
	required_shared_ptr<uint8_t> m_videoram;
	required_shared_ptr<uint8_t> m_colorram;
	required_ioport_array<3> m_matrix;
	required_region_ptr<uint8_t> m_proms;
	required_region_ptr<uint8_t> m_gfxroms;

	// sound
	required_device_array<netlist_mame_logic_input_device, 8>    m_i_pb;
	required_device<netlist_mame_logic_input_device>    m_i_folla_b;
	required_device<netlist_mame_logic_input_device>    m_i_folla_m;
	required_device<netlist_mame_logic_input_device>    m_i_folla_a;
	required_device<netlist_mame_logic_input_device>    m_i_calcio_b;
	required_device<netlist_mame_logic_input_device>    m_i_fischio;
	required_device<netlist_mame_logic_input_device>    m_i_calcio_a;
	required_device<netlist_mame_logic_input_device>    m_i_contrasto;
	required_device<netlist_mame_logic_input_device>    m_i_stop_palla;
	required_device<netlist_mame_logic_input_device>    m_i_parata;
	required_device<netlist_mame_logic_input_device>    m_i_enable;

	// misc
	uint8_t m_abca = 0U;
	uint8_t m_dr = 0U;
	uint8_t m_ds = 0U;
	uint8_t m_sh = 0U;
	uint8_t m_input_sel = 0U;
	uint8_t m_di = 0U;

	uint8_t ioread(offs_t offset);
	void iowrite(offs_t offset, uint8_t data);
	void colorram_w(offs_t offset, uint8_t data);
	uint8_t dsr_r();
	uint8_t input_matrix_read();
	void misc_w(uint8_t data);
	void sound_w(uint8_t data);
	void pb_w(uint8_t data);
	void shr_w(uint8_t data);
	void palette(palette_device &palette) const;
	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	INTERRUPT_GEN_MEMBER(irq_gen);
	void prg_map(address_map &map);
	void io_map(address_map &map);
};


/*************************************
 *
 *  Convert the palette PROM into
 *  a real palette
 *
 *************************************/

void dribling_state::palette(palette_device &palette) const
{
	uint8_t const *const prom = memregion("proms")->base() + 0x400;

	for (int i = 0; i < 256; i++)
	{
		int r = (~prom[i] >> 0) & 1;    // 220
		int g = (~prom[i] >> 1) & 3;    // 820 + 560 (332 max)
		int b = (~prom[i] >> 3) & 1;    // 220

		r *= 0xff;
		g *= 0x55;
		b *= 0xff;

		palette.set_pen_color(i, rgb_t(r, g, b));
	}
}



/*************************************
 *
 *  Color control writes
 *
 *************************************/

void dribling_state::colorram_w(offs_t offset, uint8_t data)
{
	// it is very important that we mask off the two bits here
	m_colorram[offset & 0x1f9f] = data;
}



/*************************************
 *
 *  Video update routine
 *
 *************************************/

uint32_t dribling_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	// loop over rows
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		uint16_t *const dst = &bitmap.pix(y);

		// loop over columns
		for (int x = cliprect.min_x; x <= cliprect.max_x; x++)
		{
			int const b7 = BIT(m_proms[(x >> 3) | ((y >> 3) << 5)], 0);
			int const b6 = m_abca;
			int const b5 = BIT(x, 3);
			int const b4 = BIT(m_gfxroms[(x >> 3) | (y << 5)], x & 7);
			int const b3 = BIT(m_videoram[(x >> 3) | (y << 5)], x & 7);
			int const b2_0 = m_colorram[(x >> 3) | ((y >> 2) << 7)] & 7;

			// assemble the various bits into a palette PROM index
			dst[x] = (b7 << 7) | (b6 << 6) | (b5 << 5) | (b4 << 4) | (b3 << 3) | b2_0;
		}
	}
	return 0;
}



/*************************************
 *
 *  Interrupt generation
 *
 *************************************/

INTERRUPT_GEN_MEMBER(dribling_state::irq_gen)
{
	if (m_di)
		device.execute().set_input_line(0, ASSERT_LINE);
}



/*************************************
 *
 *  PPI inputs
 *
 *************************************/

uint8_t dribling_state::dsr_r()
{
	// return DSR0-7
	return (m_ds << m_sh) | (m_dr >> (8 - m_sh));
}


uint8_t dribling_state::input_matrix_read()
{
	// low value in the given bit selects
	u8 result = 0xff;
	if (!BIT(m_input_sel, 0))
		result &= m_matrix[0]->read();
	if (!BIT(m_input_sel, 1))
		result &= m_matrix[1]->read();
	if (!BIT(m_input_sel, 2))
		result &= m_matrix[2]->read();
	return result;
}



/*************************************
 *
 *  PPI outputs
 *
 *************************************/

void dribling_state::misc_w(uint8_t data)
{
	LOGMISC("%s:misc_w(%02X)\n", machine().describe_context(), data);

	// bit 7 = di
	m_di = BIT(data, 7);
	if (!m_di)
		m_maincpu->set_input_line(0, CLEAR_LINE);

	// bit 6 = parata (save by goalkeeper)
	m_i_parata->write_line(BIT(data, 6));

	// bit 5 = ab. campo (field enable)
	m_abca = BIT(data, 5);

	// bit 4 = ab. a.b.f.
	m_i_enable->write_line(BIT(data, 4));

	// bit 3 = n/c
	// bit 2 = (9) = PC2
	// bit 1 = (10) = PC1
	// bit 0 = (32) = PC0
	m_input_sel = data & 7;
}


void dribling_state::sound_w(uint8_t data)
{
	LOGSOUND("%s:sound_w(%02X)\n", machine().describe_context(), data);

	m_i_stop_palla->write_line(BIT(data, 7)); // stop palla (ball stop)
	m_i_contrasto->write_line(BIT(data, 6));  // contrasto (tackle)
	m_i_calcio_a->write_line(BIT(data, 5));   // calcio a (kick a)
	m_i_fischio->write_line(BIT(data, 4));    // fischio (whistle)
	m_i_calcio_b->write_line(BIT(data, 3));   // calcio b (kick b)
	m_i_folla_a->write_line(BIT(data, 2));    // folla a (crowd a)
	m_i_folla_m->write_line(BIT(data, 1));    // folla m (crowd m)
	m_i_folla_b->write_line(BIT(data, 0));    // folla b (crowd b)
}


void dribling_state::pb_w(uint8_t data)
{
	LOGSOUND("%s:pb_w(%02X)\n", machine().describe_context(), data);

	for (int i = 0; i < 8; i++)
	{
		m_i_pb[i]->write_line(BIT(data, i));
	}
}


void dribling_state::shr_w(uint8_t data)
{
	// bit 3 = watchdog
	m_watchdog->reset_line_w(BIT(~data, 3));

	// bit 2-0 = SH0-2
	m_sh = data & 0x07;
}



/*************************************
 *
 *  PPI accessors
 *
 *************************************/

uint8_t dribling_state::ioread(offs_t offset)
{
	if (offset & 0x08)
		return m_ppi8255[0]->read(offset & 3);
	else if (offset & 0x10)
		return m_ppi8255[1]->read(offset & 3);
	return 0xff;
}


void dribling_state::iowrite(offs_t offset, uint8_t data)
{
	if (offset & 0x08)
		m_ppi8255[0]->write(offset & 3, data);
	else if (offset & 0x10)
		m_ppi8255[1]->write(offset & 3, data);
	else if (offset & 0x40)
	{
		m_dr = m_ds;
		m_ds = data;
	}
}


/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

void dribling_state::prg_map(address_map &map)
{
	map(0x0000, 0x1fff).rom();
	map(0x2000, 0x3fff).ram().share(m_videoram);
	map(0x4000, 0x7fff).rom();
	map(0xc000, 0xdfff).ram().w(FUNC(dribling_state::colorram_w)).share(m_colorram);
}


void dribling_state::io_map(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0xff).rw(FUNC(dribling_state::ioread), FUNC(dribling_state::iowrite));
}



/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( dribling )
	PORT_START("MUX0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN ) PORT_PLAYER(1)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP ) PORT_PLAYER(1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN ) PORT_PLAYER(1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP ) PORT_PLAYER(1)

	PORT_START("MUX1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP ) PORT_PLAYER(2)

	PORT_START("MUX2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN0")
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x40, 0x40, "Game Duration" ) PORT_DIPLOCATION( "SW:2" ) // timer always starts at 90, but decrements quicker / slower depending on dip setting
	PORT_DIPSETTING(    0x00, "1:50" ) // actually circa 1:25 emulated. Bug?
	PORT_DIPSETTING(    0x40, "2:30" ) // actually circa 1:40 emulated. Bug?
	PORT_DIPNAME( 0x80, 0x80, "New game with one coin" ) PORT_DIPLOCATION( "SW:1" ) // according to the manual, for this to work both (unimplemented) coinage dip banks must be set to 1 coin 1 play
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
INPUT_PORTS_END



/*************************************
 *
 *  Machine driver
 *
 *************************************/

void dribling_state::machine_start()
{
	save_item(NAME(m_abca));
	save_item(NAME(m_di));
	save_item(NAME(m_dr));
	save_item(NAME(m_ds));
	save_item(NAME(m_sh));
	save_item(NAME(m_input_sel));
}

void dribling_state::machine_reset()
{
	m_abca = 0;
	m_di = 0;
	m_dr = 0;
	m_ds = 0;
	m_sh = 0;
	m_input_sel = 0;
}


void dribling_state::dribling(machine_config &config)
{
	// basic machine hardware
	Z80(config, m_maincpu, 20_MHz_XTAL / 4); // XTAL and divider verified
	m_maincpu->set_addrmap(AS_PROGRAM, &dribling_state::prg_map);
	m_maincpu->set_addrmap(AS_IO, &dribling_state::io_map);
	m_maincpu->set_vblank_int("screen", FUNC(dribling_state::irq_gen));

	I8255A(config, m_ppi8255[0]);
	m_ppi8255[0]->in_pa_callback().set(FUNC(dribling_state::dsr_r));
	m_ppi8255[0]->in_pb_callback().set(FUNC(dribling_state::input_matrix_read));
	m_ppi8255[0]->out_pc_callback().set(FUNC(dribling_state::misc_w));

	I8255A(config, m_ppi8255[1]);
	m_ppi8255[1]->out_pa_callback().set(FUNC(dribling_state::sound_w));
	m_ppi8255[1]->out_pb_callback().set(FUNC(dribling_state::pb_w));
	m_ppi8255[1]->in_pc_callback().set_ioport("IN0");
	m_ppi8255[1]->out_pc_callback().set(FUNC(dribling_state::shr_w));

	WATCHDOG_TIMER(config, m_watchdog);

	// video hardware
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_RASTER));
	screen.set_video_attributes(VIDEO_UPDATE_BEFORE_VBLANK);
	screen.set_raw(20_MHz_XTAL / 4, 320, 0, 256, 262, 40, 256);
	screen.set_screen_update(FUNC(dribling_state::screen_update));
	screen.set_palette("palette");

	PALETTE(config, "palette", FUNC(dribling_state::palette), 256);

	// sound hardware
	SPEAKER(config, "mono").front_center();

	NETLIST_SOUND(config, "snd_nl", 48000)
		.set_source(NETLIST_NAME(dribling))
		.add_route(ALL_OUTPUTS, "mono", 1.0);

	NETLIST_LOGIC_INPUT(config, m_i_pb[0],      "I_PB0.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[1],      "I_PB1.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[2],      "I_PB2.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[3],      "I_PB3.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[4],      "I_PB4.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[5],      "I_PB5.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[6],      "I_PB6.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_pb[7],      "I_PB7.IN",        0);
	NETLIST_LOGIC_INPUT(config, m_i_folla_b,    "I_FOLLA_B.IN",    0);
	NETLIST_LOGIC_INPUT(config, m_i_folla_m,    "I_FOLLA_M.IN",    0);
	NETLIST_LOGIC_INPUT(config, m_i_folla_a,    "I_FOLLA_A.IN",    0);
	NETLIST_LOGIC_INPUT(config, m_i_calcio_a,   "I_CALCIO_A.IN",   0);
	NETLIST_LOGIC_INPUT(config, m_i_fischio,    "I_FISCHIO.IN",    0);
	NETLIST_LOGIC_INPUT(config, m_i_calcio_b,   "I_CALCIO_B.IN",   0);
	NETLIST_LOGIC_INPUT(config, m_i_contrasto,  "I_CONTRASTO.IN",  0);
	NETLIST_LOGIC_INPUT(config, m_i_stop_palla, "I_STOP_PALLA.IN", 0);
	NETLIST_LOGIC_INPUT(config, m_i_parata,     "I_PARATA.IN",     0);
	NETLIST_LOGIC_INPUT(config, m_i_enable,     "ENABLE_SOUND.IN", 0);
	NETLIST_STREAM_OUTPUT(config, "snd_nl:cout0", 0, "OUTPUT").set_mult_offset(1.0, 0);
}


/*************************************
 *
 *  ROM definitions
 *
 *************************************/

ROM_START( dribling )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "5p.bin",  0x0000, 0x1000, CRC(0e791947) SHA1(57bc4f4e9e1fe3fbac1017601c9c75029b2601a4) )
	ROM_LOAD( "5n.bin",  0x1000, 0x1000, CRC(bd0f223a) SHA1(f9fbc5670a8723c091d61012e545774d315eb18f) )
	ROM_LOAD( "5l.bin",  0x4000, 0x1000, CRC(1fccfc85) SHA1(c0365ad54144414218f52209173b858b927c9626) )
	ROM_LOAD( "5k.bin",  0x5000, 0x1000, CRC(737628c4) SHA1(301fda413388c26da5b5150aec2cefc971801749) )
	ROM_LOAD( "5h.bin",  0x6000, 0x1000, CRC(30d0957f) SHA1(52135e12094ee1c8828a48c355bdd565aa5895de) )

	ROM_REGION( 0x2000, "gfx", 0 )
	ROM_LOAD( "3p.bin",  0x0000, 0x1000, CRC(208971b8) SHA1(f91f3ea04d75beb58a61c844472b4dba53d84c0f) )
	ROM_LOAD( "3n.bin",  0x1000, 0x1000, CRC(356c9803) SHA1(8e2ce52f32b33886f4747dadf3aeb78148538173) )

	ROM_REGION( 0x600, "proms", 0 )
	ROM_LOAD( "93453-d9.3c",  0x0000, 0x0400, CRC(b045d005) SHA1(7e3ac10a99aa37f6348b3a57a747116b7025103e) )
	ROM_LOAD( "63s140-d8.3e", 0x0400, 0x0100, CRC(8f1a9908) SHA1(12c513c589757f1282e9979d3589f9b49d30ec0f) )
	ROM_LOAD( "tbp24s10.2d",  0x0500, 0x0100, CRC(a17d6956) SHA1(81724daf2e2d319f55cc34cc881b6a9a4e64e7ac) )
ROM_END

// the only difference from the parent is the following routine has been removed:
// 1FCF: 3A 0A 23 ld   a,($230A)
// 1FD2: A7       and  a
// 1FD3: C2 68 1F jp   nz,$1F68
// 1FD6: C3 6E 1F jp   $1F6E
// the code directly jumps to $1F6E instead
ROM_START( driblinga )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "dribling-type01-mem.n.1.5p",  0x0000, 0x1000, CRC(0e791947) SHA1(57bc4f4e9e1fe3fbac1017601c9c75029b2601a4) )
	ROM_LOAD( "dribling-type01-mem.n.2.5n",  0x1000, 0x1000, CRC(2ad86cca) SHA1(e71bf46b8d7f1a93f8288a5c81493de9413f3d5d) ) // only different ROM
	ROM_LOAD( "dribling-type01-mem.n.3.5l",  0x4000, 0x1000, CRC(1fccfc85) SHA1(c0365ad54144414218f52209173b858b927c9626) )
	ROM_LOAD( "dribling-type01-mem.n.4.5k",  0x5000, 0x1000, CRC(737628c4) SHA1(301fda413388c26da5b5150aec2cefc971801749) )
	ROM_LOAD( "dribling-type01-mem.n.5.5h",  0x6000, 0x1000, CRC(30d0957f) SHA1(52135e12094ee1c8828a48c355bdd565aa5895de) )

	ROM_REGION( 0x2000, "gfx", 0 )
	ROM_LOAD( "3p.bin",  0x0000, 0x1000, CRC(208971b8) SHA1(f91f3ea04d75beb58a61c844472b4dba53d84c0f) )
	ROM_LOAD( "3n.bin",  0x1000, 0x1000, CRC(356c9803) SHA1(8e2ce52f32b33886f4747dadf3aeb78148538173) )

	ROM_REGION( 0x600, "proms", 0 )
	ROM_LOAD( "93453pc-d9.3c", 0x0000, 0x0400, CRC(b045d005) SHA1(7e3ac10a99aa37f6348b3a57a747116b7025103e) )
	ROM_LOAD( "63s140n-d8.3e", 0x0400, 0x0100, CRC(8f1a9908) SHA1(12c513c589757f1282e9979d3589f9b49d30ec0f) )
	ROM_LOAD( "tbp24s10n.2d",  0x0500, 0x0100, CRC(a17d6956) SHA1(81724daf2e2d319f55cc34cc881b6a9a4e64e7ac) )
ROM_END


ROM_START( driblingo )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "5p.bin",       0x0000, 0x1000, CRC(0e791947) SHA1(57bc4f4e9e1fe3fbac1017601c9c75029b2601a4) )
	ROM_LOAD( "dribblng.5n",  0x1000, 0x1000, CRC(5271e620) SHA1(ebed8e31057bb8492840a6e3b8bc453f7cb67243) )
	ROM_LOAD( "5l.bin",       0x4000, 0x1000, CRC(1fccfc85) SHA1(c0365ad54144414218f52209173b858b927c9626) )
	ROM_LOAD( "dribblng.5j",  0x5000, 0x1000, CRC(e535ac5b) SHA1(ba13298378f1e5b2b40634874097ad29c402fdea) )
	ROM_LOAD( "dribblng.5h",  0x6000, 0x1000, CRC(e6af7264) SHA1(a015120d85461e599c4bb9626ebea296386a31bb) )

	ROM_REGION( 0x2000, "gfx", 0 )
	ROM_LOAD( "3p.bin",  0x0000, 0x1000, CRC(208971b8) SHA1(f91f3ea04d75beb58a61c844472b4dba53d84c0f) )
	ROM_LOAD( "3n.bin",  0x1000, 0x1000, CRC(356c9803) SHA1(8e2ce52f32b33886f4747dadf3aeb78148538173) )

	ROM_REGION( 0x600, "proms", 0 )
	ROM_LOAD( "93453-d9.3c",  0x0000, 0x0400, CRC(b045d005) SHA1(7e3ac10a99aa37f6348b3a57a747116b7025103e) )
	ROM_LOAD( "63s140-d8.3e", 0x0400, 0x0100, CRC(8f1a9908) SHA1(12c513c589757f1282e9979d3589f9b49d30ec0f) )
	ROM_LOAD( "tbp24s10.2d",  0x0500, 0x0100, CRC(a17d6956) SHA1(81724daf2e2d319f55cc34cc881b6a9a4e64e7ac) )
ROM_END

// Original Model Racing PCB (EF00284)
ROM_START( driblingam )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "2532.5p",   0x0000, 0x1000, CRC(35d97f4f) SHA1(c82b1d2a91e25cf3e3f049e0127d300572f0f54c) )
	ROM_LOAD( "2532.5n",   0x1000, 0x1000, CRC(bd0f223a) SHA1(f9fbc5670a8723c091d61012e545774d315eb18f) )
	ROM_LOAD( "2532.5l",   0x4000, 0x1000, CRC(1fccfc85) SHA1(c0365ad54144414218f52209173b858b927c9626) )
	ROM_LOAD( "2532.5k",   0x5000, 0x1000, CRC(737628c4) SHA1(301fda413388c26da5b5150aec2cefc971801749) )
	ROM_LOAD( "2532.5h",   0x6000, 0x1000, CRC(f1d6925e) SHA1(858fd13aad2c684a73b7458f18a759923b1defae) )

	ROM_REGION( 0x2000, "gfx", 0 )
	ROM_LOAD( "2532.3p",   0x0000, 0x1000, CRC(208971b8) SHA1(f91f3ea04d75beb58a61c844472b4dba53d84c0f) )
	ROM_LOAD( "2532.3n",   0x1000, 0x1000, CRC(356c9803) SHA1(8e2ce52f32b33886f4747dadf3aeb78148538173) )

	ROM_REGION( 0x600, "proms", 0 )
	ROM_LOAD( "74s476.3c", 0x0000, 0x0400, CRC(b045d005) SHA1(7e3ac10a99aa37f6348b3a57a747116b7025103e) )
	ROM_LOAD( "93427.3e",  0x0400, 0x0100, CRC(8f1a9908) SHA1(12c513c589757f1282e9979d3589f9b49d30ec0f) )
	ROM_LOAD( "93427.2d",  0x0500, 0x0100, CRC(a17d6956) SHA1(81724daf2e2d319f55cc34cc881b6a9a4e64e7ac) )
ROM_END

ROM_START( driblingvm )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "1",  0x0000, 0x1000, CRC(35d97f4f) SHA1(c82b1d2a91e25cf3e3f049e0127d300572f0f54c) )
	ROM_LOAD( "2",  0x1000, 0x1000, CRC(bd0f223a) SHA1(f9fbc5670a8723c091d61012e545774d315eb18f) )
	ROM_LOAD( "3",  0x4000, 0x1000, CRC(1fccfc85) SHA1(c0365ad54144414218f52209173b858b927c9626) )
	ROM_LOAD( "4",  0x5000, 0x1000, CRC(3ed4073a) SHA1(dec36e9dda07ea5f50163b98051955783131773d) )
	ROM_LOAD( "5",  0x6000, 0x1000, CRC(c21a1d32) SHA1(6e919f1416e6c4df133d3140f7331f65f65d4942) )

	ROM_REGION( 0x2000, "gfx", 0 )
	ROM_LOAD( "6",  0x0000, 0x1000, CRC(208971b8) SHA1(f91f3ea04d75beb58a61c844472b4dba53d84c0f) )
	ROM_LOAD( "7",  0x1000, 0x1000, CRC(356c9803) SHA1(8e2ce52f32b33886f4747dadf3aeb78148538173) )

	ROM_REGION( 0x600, "proms", 0 )
	ROM_LOAD( "93453-d9.3c",  0x0000, 0x0400, CRC(b045d005) SHA1(7e3ac10a99aa37f6348b3a57a747116b7025103e) )
	ROM_LOAD( "63s140-d8.3e", 0x0400, 0x0100, CRC(8f1a9908) SHA1(12c513c589757f1282e9979d3589f9b49d30ec0f) )
	ROM_LOAD( "tbp24s10.2d",  0x0500, 0x0100, CRC(a17d6956) SHA1(81724daf2e2d319f55cc34cc881b6a9a4e64e7ac) )
ROM_END

} // anonymous namespace


/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1983, dribling,   0,        dribling, dribling, dribling_state, empty_init, ROT0, "Model Racing",                    "Dribbling (set 1)",                    MACHINE_SUPPORTS_SAVE)
GAME( 1983, driblinga,  dribling, dribling, dribling, dribling_state, empty_init, ROT0, "Model Racing",                    "Dribbling (set 2)",                    MACHINE_SUPPORTS_SAVE)
GAME( 1983, driblingo,  dribling, dribling, dribling, dribling_state, empty_init, ROT0, "Model Racing (Olympia license)",  "Dribbling (Olympia)",                  MACHINE_SUPPORTS_SAVE)
GAME( 1983, driblingam, dribling, dribling, dribling, dribling_state, empty_init, ROT0, "Model Racing (Automave license)", "Dribbling (Automave)",                 MACHINE_SUPPORTS_SAVE )
GAME( 1983, driblingvm, dribling, dribling, dribling, dribling_state, empty_init, ROT0, "bootleg (Videomac)",              "Dribbling (Videomac bootleg, Brazil)", MACHINE_SUPPORTS_SAVE)
