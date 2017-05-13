/*
* This file is part of a Hercules Plugin library.
* http://herc.ws - http://github.com/HerculesWS/Hercules
*  __                 _
* / _\_ __ ___   ___ | | _______  ___   _ ____
* \ \| '_ ` _ \ / _ \| |/ / _ \ \/ / | | |_  /
* _\ \ | | | | | (_) |   <  __/>  <| |_| |/ /
* \__/_| |_| |_|\___/|_|\_\___/_/\_\\__, /___|
*                                   |___/
*
* Copyright (c) 2017 Smokexyz (sagunkho@hotmail.com)
* 
* Hercules is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License <http://www.gnu.org/licenses/> for
* more details.
* * * * * * * * * * * * * * * * * * * * * * * * *
* Hercules Battlegrounds Plugin by [Smokexyz]
* * * * * * * * * * * * * * * * * * * * * * * * */

#include "common/hercules.h" /* Should always be the first Hercules file included! */

#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "common/nullpo.h"
#include "common/timer.h"
#include "common/utils.h"
#include "common/sql.h"

#include "map/chrif.h"
#include "map/atcommand.h"
#include "map/clif.h"
#include "map/pc.h"
#include "map/script.h"
#include "map/npc.h"
#include "map/party.h"
#include "map/mapreg.h"
#include "map/guild.h"
#include "map/chat.h"
#include "map/quest.h"
#include "map/mob.h"
#include "map/map.h"
#include "map/pet.h"
#include "map/homunculus.h"
#include "map/mercenary.h"
#include "map/elemental.h"
#include "map/skill.h"
#include "map/battleground.h"

#include "char/mapif.h"
#include "char/inter.h"

#include "plugins/HPMHooking.h" /* Hooking Macros */
#include "common/HPMDataCheck.h" /* should always be the last Hercules file included! */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    Utility Functions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define add2limit(a, b, max) \
	do { \
		if ((max - a) < b) { \
			a = max; \
		} else { \
			a += b; \
		} \
	} while(0)

#define sub2limit(a, b, min) \
	do { \
		if ((b + min) > a) { \
			a = min; \
		} else { \
			a -= b; \
		} \
	} while(0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    HPM Structure Definition
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
HPExport struct hplugin_info pinfo = {
	"Hercules Battlegrounds",    // Plugin name
	SERVER_TYPE_MAP | SERVER_TYPE_CHAR, // Server Type
	"1.0a",       // Plugin version
	HPM_VERSION, // HPM Version (automatically updated)
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    Global Variables
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define MAX_BATTLEGROUND_TEAMS 13
#define BLUE_SKULL 8965
#define RED_SKULL 8966
#define GREEN_SKULL 8967

/* Queue Database */
static struct DBMap* hBG_queue_db;
/* Battleground Guild Storage */
struct guild bg_guild[MAX_BATTLEGROUND_TEAMS];
/* Battleground Guild Colors */
const unsigned int bg_colors[MAX_BATTLEGROUND_TEAMS] = {
	0x0000FF, // Blue
	0xFF0000, // Red
	0x00FF00, // Green
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF,
	0xFFFFFF
};

/* Counter to check the next battleground ID */
static unsigned int bg_team_counter = 0;
/* hBG Queue Counter */
static unsigned int hBG_queue_counter = 0;

/**
 * Battle Configuration Variables
 */
int hBG_idle_announce,
	hBG_idle_autokick,
	hBG_reward_rates,
	hBG_ranked_mode,
	hBG_reportafk_leaderonly,
	hBG_balanced_queue,
	hBG_ip_check,
	hBG_from_town_only,
	hBG_enabled,
	hBG_xy_interval,
	hBG_leader_change;

/**
 * BG Fame list types
 */
enum bg_fame_list_types
{
	BG_NORMAL,
	BG_RANKED
};

/**
 * Inter-server communication packet IDs.
 */
enum inter_packet_types {
	PACKET_INTER_BG_STATS_SAVE =  0x9000,
	PACKET_INTER_BG_STATS_REQ,
	PACKET_MAP_BG_STATS_GET,
};

enum bg_mob_types {
	BARRICADE_        = 1906,
	OBJ_NEUTRAL       = 1911,
	E_BAPHOMET2       = 2100,
	E_LORD_OF_DEATH2,
	E_DARK_LORD,
	E_KTULLANUX,
	E_DARK_SNAKE_LORD,
	E_TURTLE_GENERAL,
	E_APOCALIPS_H,
	E_FALLINGBISHOP,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    Structure Definitions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * BGD Structure Appendant
 */
struct hBG_data
{
	time_t created_at; // Creation of this Team
	int leader_char_id;
	unsigned int color;
	// BG Guild Link
	struct guild *g;
	bool reveal_pos, reveal_pos2, reveal_flag;
	// Score Board
	int team_score;
};

/**
 * hBG Queue Member Linked List
 */
struct hBG_queue_member
{
	int position;
	struct map_session_data *sd;
	struct hBG_queue_member *next;
};

struct hBG_stats
{
	unsigned int
	total_damage_done,
	total_damage_received;
	
	unsigned short
	best_damage,
	// Triple Inferno
	ti_wins,
	ti_lost,
	ti_tie,
	// Tierra EoS
	eos_flags,
	eos_bases,
	eos_wins,
	eos_lost,
	eos_tie,
	// Tierra Bossnia
	boss_killed,
	boss_flags,
	boss_wins,
	boss_lost,
	boss_tie,
	boss_damage,
	// Tierra Domination
	dom_bases,
	dom_off_kills,
	dom_def_kills,
	dom_wins,
	dom_lost,
	dom_tie,
	// Flavius TD
	td_kills,
	td_deaths,
	td_wins,
	td_lost,
	td_tie,
	// Flavius SC
	sc_stolen,
	sc_captured,
	sc_dropped,
	sc_wins,
	sc_lost,
	sc_tie,
	// Flavius CTF
	ctf_taken,
	ctf_captured,
	ctf_dropped,
	ctf_wins,
	ctf_lost,
	ctf_tie,
	// Conquest
	emperium_kills,
	barricade_kills,
	guardian_stone_kills,
	conquest_wins,
	conquest_losses,
	// Rush
	ru_captures,
	ru_wins,
	ru_lost,
	ru_skulls;
	
	unsigned int // Ammo
	sp_heal_potions,
	hp_heal_potions,
	yellow_gemstones,
	red_gemstones,
	blue_gemstones,
	poison_bottles,
	acid_demostration,
	acid_demostration_fail,
	support_skills_used,
	healing_done,
	wrong_support_skills_used,
	wrong_healing_done,
	sp_used,
	zeny_used,
	spiritb_used,
	ammo_used;
	
	unsigned short
	kill_count,
	death_count,
	wins, losses, ties,
	wins_as_leader,
	losses_as_leader,
	ties_as_leader,
	total_deserted,
	ranked_games;
	
	int score,
	points,
	ranked_points;
};

/**
 * MSD Structure Appendant (class 0)
 */
struct hBG_queue_data
{
	unsigned int q_id, users, min_level;
	struct hBG_queue_member *first, *last;
	char queue_name[50], join_event[EVENT_NAME_LENGTH];
};

/**
 * MSD Structure Appendant (class 1)
 */
struct hBG_map_session_data
{
	unsigned int bg_kills;
	struct {
		unsigned afk : 1;
	} state;
	
	struct hBG_stats stats;
};

/**
 * MOBD Structure Appendant
 */
struct hBG_mob_data
{
	struct {
		unsigned immunity: 1;
	} state;
};

/**
 * MAPD Structure Appendant
 */
struct hBG_mapflag
{
	unsigned hBG_topscore : 1; // No Detect NoMapFlag
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    Function Forward Declarations
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int hBG_countlogin(struct map_session_data *sd, bool check_bat_room);
int hBG_config_get(const char *key);
struct guild* hBG_get_guild(int bg_id);
struct map_session_data* hBG_getavailablesd(struct battleground_data *bgd);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    Packet Sending Functions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * Updates HP bar of a camp member.
 * Packet Ver < 20140613: 02e0 <account id>.L <name>.24B <hp>.W <max hp>.W (ZC_BATTLEFIELD_NOTIFY_HP).
 * Packet Ver >= 20140613: 0a0e <account id>.L <hp>.L <max hp>.L (ZC_BATTLEFIELD_NOTIFY_HP2)
 *
 * @param sd map_session_data to send the packet to.
 * @return void.
 */
void hBG_send_hp_area(struct map_session_data *sd)
{
	unsigned char buf[34];
	
	// packet version can be wrong,
	// due to inconsistend data on other servers.
#if PACKETVER < 20140613
	const int cmd = 0x2e0;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	if (packet == NULL)
		return;

	WBUFW(buf, 0) = cmd;
	WBUFL(buf, 2) = sd->status.account_id;
	memcpy(WBUFP(buf, 6), sd->status.name, NAME_LENGTH);

	if (sd->battle_status.max_hp > INT16_MAX) { // To correctly display the %hp bar. [Skotlex]
		WBUFW(buf, 30) = sd->battle_status.hp / (sd->battle_status.max_hp / 100);
		WBUFW(buf, 32) = 100;
	}
	else {
		WBUFW(buf, 30) = sd->battle_status.hp;
		WBUFW(buf, 32) = sd->battle_status.max_hp;
	}
	
	clif->send(buf, packet->len, &sd->bl, BG_AREA_WOS);
#else
	const int cmd = 0xa0e;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	if (packet == NULL)
		return;

	WBUFW(buf, 0) = cmd;
	WBUFL(buf, 2) = sd->status.account_id;
	if (sd->battle_status.max_hp > INT32_MAX) {
		WBUFL(buf, 6) = sd->battle_status.hp / (sd->battle_status.max_hp / 100);
		WBUFL(buf, 10) = 100;
	} else {
		WBUFL(buf, 6) = sd->battle_status.hp;
		WBUFL(buf, 10) = sd->battle_status.max_hp;
	}
	
	clif->send(buf, packet->len, &sd->bl, BG_AREA_WOS);
#endif
}

/**
 * Notify a singly client of HP change.
 *
 * @param fd  socket fd to write
 * @param sd  map_session_data containing information to be sent.
 */
void hBG_send_hp_single(int fd, struct map_session_data* sd)
{
	const int cmd = 0x2e0;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	if (packet == NULL)
		return;

	WFIFOHEAD(fd, packet->len);
	WFIFOW(fd, 0) = cmd;
	WFIFOL(fd, 2) = sd->bl.id;
	memcpy(WFIFOP(fd,6),sd->status.name, NAME_LENGTH);
	if (sd->battle_status.max_hp > INT16_MAX) {
		WFIFOW(fd, 30) = sd->battle_status.hp/(sd->battle_status.max_hp/100);
		WFIFOW(fd, 32) = 100;
	} else {
		WFIFOW(fd, 30) = sd->battle_status.hp;
		WFIFOW(fd, 32) = sd->battle_status.max_hp;
	}

	WFIFOSET(fd, packet->len);
}

/**
 * Character Guild Name Information Packet
 *
 * @param sd map_session_data to send the packet to.
 * @return void
 */
void hBG_send_guild_info(struct map_session_data *sd)
{
	int fd;
	int cmd = 0x16c;
	struct guild *g;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);

	if (!sd->bg_id || (g = hBG_get_guild(sd->bg_id)) == NULL || packet == NULL)
		return;

	fd = sd->fd;
	WFIFOHEAD(fd, packet->len);
	WFIFOW(fd,0) = 0x16c;
	WFIFOL(fd,2) = g->guild_id;
	WFIFOL(fd,6) = g->emblem_id;
	WFIFOL(fd,10) = 0;
	WFIFOB(fd,14) = 0;
	WFIFOL(fd,15) = 0;
	memcpy(WFIFOP(fd,19), g->name, NAME_LENGTH);

	WFIFOSET(fd,  packet->len);
}

/** 
 * Sends guild skills (ZC_GUILD_SKILLINFO).
 * 0162 <packet len>.W <skill points>.W { <skill id>.W <type>.L <level>.W <sp cost>.W <atk range>.W <skill name>.24B <upgradeable>.B }*
 */
void hBG_send_guild_skillinfo(struct map_session_data* sd)
{
	int fd;
	struct guild* g;
	int i,c;
	
	nullpo_retv(sd);
	
	if (!sd->bg_id || (g = hBG_get_guild(sd->bg_id)) == NULL)
		return;
	
	fd = sd->fd;
	WFIFOHEAD(fd, 6 + MAX_GUILDSKILL*37);
	WFIFOW(fd,0) = 0x0162;
	WFIFOW(fd,4) = g->skill_point;
	for (i = 0, c = 0; i < MAX_GUILDSKILL; i++) {
		if(g->skill[i].id > 0 && guild->check_skill_require(g, g->skill[i].id)) {
			int id = g->skill[i].id;
			int p = 6 + c*37;
			WFIFOW(fd,p+0) = id;
			WFIFOL(fd,p+2) = skill->get_inf(id);
			WFIFOW(fd,p+6) = g->skill[i].lv;
			if ( g->skill[i].lv) {
				WFIFOW(fd, p + 8) = skill->get_sp(id, g->skill[i].lv);
				WFIFOW(fd, p + 10) = skill->get_range(id, g->skill[i].lv);
			} else {
				WFIFOW(fd, p + 8) = 0;
				WFIFOW(fd, p + 10) = 0;
			}
			safestrncpy(WFIFOP(fd,p+12), skill->get_name(id), NAME_LENGTH);
			WFIFOB(fd,p+36)= (g->skill[i].lv < guild->skill_get_max(id) && sd == g->member[0].sd) ? 1 : 0;
			c++;
		}
	}
	WFIFOW(fd,2) = 6 + c*37;
	WFIFOSET(fd,WFIFOW(fd,2));
}

/**
 * Sends Guild Window Information
 * 01b6 <guild id>.L <level>.L <member num>.L <member max>.L <exp>.L <max exp>.L <points>.L <honor>.L <virtue>.L <emblem id>.L <name>.24B <master name>.24B <manage land>.16B <zeny>.L (ZC_GUILD_INFO2)
 * @param sd map_session_data to send the packet to.
 * @return void
 */
void hBG_guild_window_info(struct map_session_data *sd)
{
	int fd;
	int cmd = 0x1b6;
	struct guild *g;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	fd = sd->fd;
	
	if ((g = hBG_get_guild(sd->bg_id)) == NULL || packet == NULL)
		return;
	
	WFIFOHEAD(fd, packet->len); // Hardcoded Length
	WFIFOW(fd, 0)= cmd;
	WFIFOL(fd, 2)= g->guild_id;
	WFIFOL(fd, 6)= g->guild_lv;
	WFIFOL(fd,10)= g->connect_member;
	WFIFOL(fd,14)= g->max_member;
	WFIFOL(fd,18)= g->average_lv;
	WFIFOL(fd,22)= (uint32)cap_value(g->exp,0,INT32_MAX);
	WFIFOL(fd,26)= g->next_exp;
	WFIFOL(fd,30)= 0; // Tax Points
	WFIFOL(fd,34)= 0; // Honor: (left) Vulgar [-100,100] Famed (right)
	WFIFOL(fd,38)= 0; // Virtue: (down) Wicked [-100,100] Righteous (up)
	WFIFOL(fd,42)= g->emblem_id;
	memcpy(WFIFOP(fd,46), g->name, NAME_LENGTH);
	memcpy(WFIFOP(fd,70), g->master, NAME_LENGTH);
	
	//safestrncpy(WFIFOP(fd,94), 0, 0); // "'N' castles"
	WFIFOL(fd, 110) = 0;  // zeny
	
	WFIFOSET(fd, packet->len);
}

/**
 * Sends Guild Emblem to a player
 * @param sd   map_session_data to send the packet to
 * @param g    guild data
 * @return void
 */
void hBG_send_emblem(struct map_session_data *sd, struct guild *g)
{
	int fd;

	nullpo_retv(sd);
	nullpo_retv(g);

	if (g->emblem_len <= 0)
		return;

	fd = sd->fd;
	WFIFOHEAD(fd, g->emblem_len+12);
	WFIFOW(fd,0) = 0x152;
	WFIFOW(fd,2) = g->emblem_len+12;
	WFIFOL(fd,4) = g->guild_id;
	WFIFOL(fd,8) = g->emblem_id;
	memcpy(WFIFOP(fd,12),g->emblem_data,g->emblem_len);

	WFIFOSET(fd, WFIFOW(fd,2));
}

/**
 * Sends guild member list 
 * @param sd  map_session_data to send the packet to
 * @return void
 */
void hBG_send_guild_member_list(struct map_session_data *sd)
{
	int fd, i, c;
	struct battleground_data *bgd;
	struct map_session_data *psd;
	struct hBG_data *hBGd;
	
	nullpo_retv(sd);

	if ((fd = sd->fd) == 0)
		return;
	if (!sd->bg_id || (bgd = bg->team_search(sd->bg_id)) == NULL)
		return;
	else if ((hBGd = getFromBGDATA(bgd, 0)) == NULL)
		return;

	WFIFOHEAD(fd, bgd->count * 104 + 4);
	WFIFOW(fd, 0) = 0x154;

	for (i = 0, c = 0; i < bgd->count; i++) {
		struct hBG_map_session_data *hBGsd;

		if ((psd = bgd->members[i].sd) == NULL)
			continue;

		if ((hBGsd = getFromMSD(psd, 1)) == NULL)
			continue;

		WFIFOL(fd,c*104+ 4) = psd->status.account_id;
		WFIFOL(fd,c*104+ 8) = psd->status.char_id;
		WFIFOW(fd,c*104+12) = psd->status.hair;
		WFIFOW(fd,c*104+14) = psd->status.hair_color;
		WFIFOW(fd,c*104+16) = psd->status.sex;
		WFIFOW(fd,c*104+18) = psd->status.class;
		WFIFOW(fd,c*104+20) = psd->status.base_level;
		WFIFOL(fd,c*104+22) = hBGsd->bg_kills; // Exp slot used to show kills
		WFIFOL(fd,c*104+26) = 1; // Online
		WFIFOL(fd,c*104+30) = hBGd->leader_char_id == psd->status.char_id ? 0 : 1; // Position
		memset(WFIFOP(fd,c*104+34),0,50); // Position Name
		memcpy(WFIFOP(fd,c*104+84),psd->status.name,NAME_LENGTH); // Player Name
		c++;
	}

	WFIFOW(fd, 2)=c*104+4;

	WFIFOSET(fd,WFIFOW(fd,2));
}

/**
 * Send guild leave packet
 * @param sd map_session_data to send the packet to
 * @param *name contains name of the character.
 * @param *mes  contains leave message from player.
 * @return void
 */
void hBG_send_leave_single(struct map_session_data *sd, const char *name, const char *mes)
{
	unsigned char buf[128];
	int cmd = 0x15a;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	if (packet == NULL)
		return;
	
	WBUFW(buf, 0) = cmd;
	memcpy(WBUFP(buf, 2), name, NAME_LENGTH);
	memcpy(WBUFP(buf,26), mes, 40);
	
	clif->send(buf, packet->len, &sd->bl, SELF);
}

/**
 * Notifies clients of a battleground message ZC_BATTLEFIELD_CHAT
 * 02dc <packet len>.W <account id>.L <name>.24B <message>.?B
 *
 * @param bgd battleground_data to send the message to
 * @param src_id id of the source of the message.
 * @param name contains the name of the character.
 * @param mes contains the message to be sent.
 * @param len contains the length of the message.
 * @return void.
 */
void hBG_send_chat_message(struct battleground_data *bgd, int src_id, const char *name, const char *mes, int len)
{
	struct map_session_data *sd;
	unsigned char *buf;

	nullpo_retv(bgd);
	nullpo_retv(name);
	nullpo_retv(mes);

	if ((sd = hBG_getavailablesd(bgd)) == NULL)
		return;

	len = (int)strlen(mes);
	Assert_retv(len <= INT16_MAX - NAME_LENGTH - 8);
	buf = (unsigned char*)aMalloc((len + NAME_LENGTH + 8)*sizeof(unsigned char));

	WBUFW(buf, 0) = 0x2dc;

	WBUFW(buf, 2) = len + NAME_LENGTH + 8;
	WBUFL(buf, 4) = src_id;
	memcpy(WBUFP(buf, 8), name, NAME_LENGTH);
	memcpy(WBUFP(buf, 32), mes, len); // [!] no NULL terminator

	clif->send(buf, WBUFW(buf, 2), &sd->bl, BG);

	if (buf)
		aFree(buf);
}

/**
 * Notifies the client of a guild expulsion.
 *
 * @param sd   map_session_data to send the packet to
 * @param *name  contains the name of the character.
 * @param *mes   contains the expulsion message.
 */
void hBG_send_expulsion(struct map_session_data *sd, const char *name, const char *mes)
{
	int fd;
	int cmd = 0x15c;
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	if (packet == NULL)
		return;

	fd = sd->fd;
	WFIFOHEAD(fd, packet->len);
	WFIFOW(fd,0) = cmd;
	safestrncpy((char*)WFIFOP(fd,2), name, NAME_LENGTH);
	safestrncpy((char*)WFIFOP(fd,26), mes, 40);
	safestrncpy((char*)WFIFOP(fd,66), "", NAME_LENGTH);
	WFIFOSET(fd, packet->len);
}

/**
 * Notifies a single client of BG score updates
 * 
 * @param sd map_session_data to send the packet to
 * @return void
 */
void hBG_update_score_single(struct map_session_data *sd)
{
	int fd;
	int cmd = 0x2de;
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	struct hBG_mapflag *hBG_mf = getFromMAPD(&map->list[sd->bl.m], 0);
	const struct s_packet_db *packet = clif->packet(cmd);
	
	nullpo_retv(sd);
	
	if (packet == NULL)
		return;
	
	fd = sd->fd;

	WFIFOHEAD(fd, packet->len);
	WFIFOW(fd,0) = cmd;

	if (map->list[sd->bl.m].flag.battleground == 2) { // Score Board on Map. Team vs Team
		WFIFOW(fd,2) = map->list[sd->bl.m].bgscore_lion;
		WFIFOW(fd,4) = map->list[sd->bl.m].bgscore_eagle;
	} else if (map->list[sd->bl.m].flag.battleground == 3
			&& (bgd = bg->team_search(sd->bg_id)) != NULL
			&& (hBGd = getFromBGDATA(bgd, 0)) != NULL) { // Score Board Multiple. Team vs Best Score
		WFIFOW(fd,2) = hBGd->team_score;
		WFIFOL(fd,4) = hBG_mf->hBG_topscore;
	}
	WFIFOSET(fd, packet->len);
}

/**
 * Notifies all clients in a Battleground Team
 *
 * @param bgd battleground_data containing the map_session_datas to send the packet to
 * @return void
 */
void hBG_update_score_team(struct battleground_data *bgd)
{
	unsigned char buf[6];
	struct map_session_data *sd;
	int i, m, cmd = 0x2de;
	struct hBG_data *hBGd = getFromBGDATA(bgd, 0);
	struct hBG_mapflag *hBG_mf;
	const struct s_packet_db *packet = clif->packet(cmd);

	nullpo_retv(bg);

	if ((m = map->mapindex2mapid(bgd->mapindex)) < 0
		|| hBGd == NULL
		|| (hBG_mf = getFromMAPD(&map->list[m], 0)) == NULL
		|| packet == NULL)
		return;

	WBUFW(buf,0) = cmd;
	WBUFW(buf,2) = hBGd->team_score;
	WBUFW(buf,4) = hBG_mf->hBG_topscore;

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((sd = bgd->members[i].sd) == NULL || sd->bl.m != m)
			continue;

		clif->send(buf, packet->len, &sd->bl, SELF);
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                    Queue System Functions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Searches hBG_queue_db by q_id
 * @param q_id contains the id of the queue to be searched.
 * @return hBG_queue_data * pointer to the memory location or null if not found.
 */
struct hBG_queue_data* hBG_queue_search(int q_id)
{ // Search a Queue using q_id
	if (!q_id)
		return NULL;
	
	return (struct hBG_queue_data *) idb_get(hBG_queue_db, q_id);
}

/**
 * Creates a Queue
 * @param queue_name contains the name of the queue.
 * @param join_event contains the event run on joining the queue.
 * @param min_level contains the minimum level to be able to join.
 * @return queue_id id of the queue created.
 */
int hBG_queue_create(const char* queue_name, const char* join_event, int min_level)
{
	struct hBG_queue_data *hBGqd;
	
	if (++hBG_queue_counter <= 0)
		hBG_queue_counter = 1;

	CREATE(hBGqd, struct hBG_queue_data, 1);
	hBGqd->q_id = hBG_queue_counter;
	
	safestrncpy(hBGqd->queue_name, queue_name, sizeof(hBGqd->queue_name));
	safestrncpy(hBGqd->join_event, join_event, sizeof(hBGqd->join_event));
	
	hBGqd->first = hBGqd->last = NULL; // First and Last Queue Members
	hBGqd->users = 0;
	hBGqd->min_level = min_level;
	
	idb_put(hBG_queue_db, hBG_queue_counter, hBGqd);

	return hBGqd->q_id;
}

/**
 * Remove all queue members from the queue and free the links.
 * @param hBGqd pointer to the queue data.
 * @return void.
 */
void hBG_queue_members_finalize(struct hBG_queue_data *hBGqd)
{
	struct hBG_queue_member *head, *next;
	
	nullpo_retv(hBGqd);

	head = hBGqd->first;
	while (head) {
		next = head->next;
		
		aFree(head);
		
		head = next;
	}

	hBGqd->first = hBGqd->last = NULL;
	hBGqd->users = 0;
}

/**
 * Add a player to the queue.
 * @param hBGqd pointer to the queue data.
 * @param sd    player to be added to the queue.
 * @return postition of the player in queue.
 */
int hBG_queue_member_add(struct hBG_queue_data *hBGqd, struct map_session_data *sd)
{
	struct hBG_queue_member *hBGqm;

	nullpo_retr(0, hBGqd);
	nullpo_retr(0, sd);

	hBGqd->users++;
	
	CREATE(hBGqm, struct hBG_queue_member, 1);
	
	hBGqm->sd = sd;
	hBGqm->position = hBGqd->users;
	hBGqm->next = NULL;
	
	if (getFromMSD (sd, 0) == NULL)
		addToMSD(sd, hBGqd, 0, false);
	
	if (hBGqd->last == NULL) {
		hBGqd->first = hBGqd->last = hBGqm; // Attach to first position
	} else { // Attach at the end of the queue
		hBGqd->last->next = hBGqm;
		hBGqd->last = hBGqm;
	}
	
	return hBGqm->position;
}

/**
 * Get a member of the queue from position n.
 * @param hBGqd    pointer to the queue data
 * @param position position of the player in queue
 * @return queue_member data or NULL if not found.
 */
struct hBG_queue_member* hBG_queue_member_get(struct hBG_queue_data *hBGqd, int position)
{
	struct hBG_queue_member *head;
	if (!hBGqd) return NULL;

	head = hBGqd->first;
	while (head != NULL) {
		if (head->sd && head->position == position)
			return head;

		head = head->next;
	}

	return NULL;
}

/**
 * Clear a queue and remove from memory.
 * @param hBGqd pointer to queue_data
 * return 1 on success, 0 on failure.
 */
int hBG_queue_destroy(struct hBG_queue_data *hBGqd)
{
	nullpo_ret(hBGqd);
	
	hBG_queue_members_finalize(hBGqd);
	
	idb_remove(hBG_queue_db, hBGqd->q_id);
	
	return 1;
}

/**
 * Remove a member from a queue
 * @param hBGqd pointer to queue data.
 * @param id    block list ID of player in queue.
 * @return position of a player in the queue or 0 if not found.
 */
int hBG_queue_member_remove(struct hBG_queue_data *hBGqd, int id)
{
	struct hBG_queue_member *head, *previous;
	int i;
	
	nullpo_retr(0, hBGqd);

	head = hBGqd->first;
	previous = NULL;

	while (head != NULL) {
		if (head->sd && head->sd->bl.id == id) {
			struct hBG_queue_member *next;
			
			next = head->next;
			i = head->position;
			
			hBGqd->users--;
			
			// De-attach target from the main queue
			if (previous) {
				previous->next = head->next;
			} else {
				hBGqd->first = head->next; // Deleted is on first position
			}
			
			if (head->next == NULL)
				hBGqd->last = previous; // Deleted is on last position
			
			while (next != NULL) {
				next->position--; // Decrement positions of the next in queue.
				next = next->next;
			}
			
			aFree(head);
			
			return i;
		}

		previous = head;
		head = head->next;
	}

	return 0;
}

/**
 * Search a member in the queue by Block List ID.
 * @param hBGqd pointer to queue data.
 * @param id    block list ID of a player in queue.
 * @return position of a player in the queue or 0 if not found.
 */
int hBG_queue_member_search(struct hBG_queue_data *hBGqd, int id)
{
	struct hBG_queue_member *head;
	
	nullpo_retr(0,hBGqd);

	head = hBGqd->first;

	while (head != NULL) {
		if (head->sd && head->sd->bl.id == id)
			return head->position;

		head = head->next;
	}

	return 0; // Not Found
}

/**
 * Add a player to the queue.
 * @param sd pointer to player to be added in queue.
 * @param q_id ID of the queue to be appended.
 * @return 0 on failure, 1 on success.
 */
int hBG_queue_join(struct map_session_data *sd, int q_id)
{
	char output[128];
	struct hBG_queue_data *hBGqd;
	int i = 0;
	int login_ip_count = hBG_config_get("battle_configuration/hBG_ip_check");
	
	nullpo_ret(sd);
	
	if (hBG_config_get("battle_configuration/hBG_from_town_only") && map->list[sd->bl.m].flag.town == 0) {
		clif->message(sd->fd,"You only can join BG queues from Towns or BG Waiting Room.");
		return 0;
	} else if (sd->bg_id) {
		clif->message(sd->fd,"You cannot join queues when already playing Battlegrounds.");
		return 0;
	} else if (sd->sc.data[SC_JAILED]) {
		clif->message(sd->fd,"You cannot join queues when jailed.");
		return 0;
	}
	// Get Queue by ID
	if ((hBGqd = hBG_queue_search(q_id)) == NULL) {
		return 0; // Current Queue don't exists
	} else if ((i = hBG_queue_member_search(hBGqd, sd->bl.id))) { // You cannot join a Queue if you are already in one.
		sprintf(output, "You are already in %s queue at position %d.", hBGqd->queue_name, i);
		clif->message(sd->fd, output);
		return 0;
	} else if (hBGqd && hBGqd->min_level && sd->status.base_level < hBGqd->min_level) {
		sprintf(output,"You cannot join %s queue. Required min level is %ud.", hBGqd->queue_name, hBGqd->min_level);
		clif->message(sd->fd,output);
		return 0;
	} else if (login_ip_count && hBG_countlogin(sd,false) > login_ip_count) {
		sprintf(output,"You cannot join %s queue. A max of %d characters are using your IP address.", hBGqd->queue_name, login_ip_count);
		clif->message(sd->fd,output);
		return 0;
	}

	i = hBG_queue_member_add(hBGqd, sd);
	
	sprintf(output,"You have joined %s queue at position %d.", hBGqd->queue_name, i);
	
	clif->message(sd->fd,output);

	if (hBGqd->join_event[0])
		npc->event_do(hBGqd->join_event);

	return i;
}

/**
 * Process player leaving a queue.
 * @param sd pointer to player leaving the queue.
 * @param q_id ID of the queue to be left.
 * @return 0 on failure, 1 on success.
 */
int hBG_queue_leave(struct map_session_data *sd, int q_id)
{
	char output[128];
	struct hBG_queue_data *qd;

	if ((qd = hBG_queue_search(q_id)) == NULL)
		return 0;

	if (!hBG_queue_member_remove(qd,sd->bl.id)) {
		sprintf(output,"You are not in the %s queue.", qd->queue_name);
		clif->message(sd->fd,output);
		return 0;
	}
	
	sprintf(output, "You have been removed from the %s queue.", qd->queue_name);
	clif->message(sd->fd, output);

	return 1;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     Battleground Functions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * Return any available player in a Battleground
 * @param bgd pointer to battleground queue.
 * @return map_session_data *sd pointer to player data or NULL if not found.
 */
struct map_session_data* hBG_getavailablesd(struct battleground_data *bgd)
{
	int i;
	nullpo_retr(NULL, bgd);
	ARR_FIND(0, MAX_BG_MEMBERS, i, bgd->members[i].sd != NULL);
	return(i < MAX_BG_MEMBERS) ? bgd->members[i].sd : NULL;
}

/**
 * Count the number of players with the same IP in battlegrounds.
 * @param sd pointer to map session data.
 * @param check_bat_room boolean to include checks in bat_room.
 * @return amount of accounts online.
 */
int hBG_countlogin(struct map_session_data *sd, bool check_bat_room)
{
	int c = 0, m = map->mapname2mapid("bat_room");
	struct map_session_data* pl_sd;
	struct s_mapiterator* iter;
	nullpo_ret(sd);

	iter = mapit_getallusers();
	for (pl_sd = (TBL_PC*)mapit->first(iter); mapit->exists(iter); pl_sd = (TBL_PC*)mapit->next(iter)) {
		if (!(getFromMSD(sd, 0) || map->list[pl_sd->bl.m].flag.battleground || (check_bat_room && pl_sd->bl.m == m)))
			continue;
		if (sockt->session[sd->fd]->client_addr == sockt->session[pl_sd->fd]->client_addr)
			c++;
	}
	mapit->free(iter);
	return c;
}

/**
 * Performs initialization tasks on new battlegrounds.
 * @param m map Index
 * @param rx respawn X co-ordinate
 * @param ry respawn Y co-ordinate
 * @param guild_index Index of the BG guild to be used
 * @param *ev NPC Script event to be called when player logs out.
 * @param *dev NPC Script event to be called when player dies.
 * @return ID of the battleground.
 */
int hBG_create(unsigned short m, short rx, short ry, int guild_index, const char *ev, const char *dev)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;

	CREATE(bgd, struct battleground_data, 1);
	CREATE(hBGd, struct hBG_data, 1);

	if (++bg_team_counter <= 0)
		bg_team_counter = 1;

	bgd->bg_id = bg_team_counter;
	bgd->count = 0;
	bgd->mapindex = m;
	bgd->x = rx;
	bgd->y = ry;

	safestrncpy(bgd->logout_event, ev, sizeof(bgd->logout_event));
	safestrncpy(bgd->die_event, dev, sizeof(bgd->die_event));

	memset(&bgd->members, 0, sizeof(bgd->members));

	hBGd->color = bg_colors[guild_index];
	hBGd->created_at = 0;
	hBGd->g = &bg_guild[guild_index];

	addToBGDATA(bgd, hBGd, 0, true);

	idb_put(bg->team_db, bg_team_counter, bgd);

	return bgd->bg_id;
}

/**
 * Add a player to the Battleground Team
 * @param bg_id Id of the battleground
 * @param sd pointer to the player's session data.
 * @return 0 on failure, 1 on success.
 */
int hBG_team_join(int bg_id, struct map_session_data *sd)
{ // Player joins team
	int i;
	struct battleground_data *bgd = bg->team_search(bg_id);
	struct map_session_data *pl_sd;
	struct hBG_map_session_data *hBGsd;
	struct hBG_data *hBGd;

	if (bgd == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL || sd == NULL || sd->bg_id)
		return 0;

	ARR_FIND(0, MAX_BG_MEMBERS, i, bgd->members[i].sd == NULL);
	
	if (i == MAX_BG_MEMBERS)
		return 0; // No free slots

	// Handle Additional Map Session BG data
	if ((hBGsd = getFromMSD(sd, 1)) == NULL) {
		CREATE(hBGsd, struct hBG_map_session_data, 1);
		addToMSD(sd, hBGsd, 1, false);
	}
	
	hBGsd->bg_kills = 0;
	hBGsd->state.afk = 0;

	// Update player's idle item.
	pc->update_idle_time(sd, BCIDLE_WALK);

	sd->bg_id = bg_id;

	// Battleground Member data
	bgd->members[i].sd = sd;
	bgd->members[i].x = sd->bl.x;
	bgd->members[i].y = sd->bl.y;
	bgd->count++;
	
	// Guild Member Data simulation
	hBGd->g->member[i].sd = sd;

	// Creation Tick = First member joined.
	if (hBGd->created_at == 0)
		hBGd->created_at = sockt->last_tick;

	// First Join = Team Leader
	if (hBGd->leader_char_id == 0)
		hBGd->leader_char_id = sd->status.char_id;

	guild->send_dot_remove(sd);

	hBG_send_guild_info(sd); // Send Guild Name/Emblem under character
	
	clif->charnameupdate(sd); // Update character's nameplate
	
	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((pl_sd = bgd->members[i].sd) == NULL)
			continue;

		// Simulate Guild Information
		hBG_guild_window_info(pl_sd); // Main Guild window information.
		hBG_send_emblem(pl_sd, hBGd->g); // Emblems
		hBG_send_guild_member_list(pl_sd); // Member list
		hBG_send_guild_skillinfo(sd); // Send Guild skill information.

		if (pl_sd != sd)
			hBG_send_hp_single(sd->fd,pl_sd);
	}

	clif->guild_emblem_area(&sd->bl);

	clif->bg_hp(sd);
	clif->bg_xy(sd);

	return 1;
}

/**
 * Reveal a player's position to another player.
 * @param bl pointer to block list.
 * @param ap list of arguments
 * @return 0.
 */
int hBG_reveal_pos(struct block_list *bl, va_list ap)
{
	struct map_session_data *pl_sd, *sd = NULL;
	int flag, color;

	pl_sd = (struct map_session_data *)bl;
	sd = va_arg(ap,struct map_session_data *); // Source
	flag = va_arg(ap,int);
	color = va_arg(ap,int);

	if (pl_sd->bg_id == sd->bg_id)
		return 0; // Same Team

	clif->viewpoint(pl_sd,sd->bl.id,flag,sd->bl.x,sd->bl.y,sd->bl.id,color);
	return 0;
}

/**
 * Remove minimap indicator for player.
 * @param sd pointer to session data.
 * @return 0
 */
int hBG_send_dot_remove(struct map_session_data *sd)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int m;

	if (sd && sd->bg_id && (bgd = bg->team_search(sd->bg_id)) != NULL
	   && (hBGd = getFromBGDATA(bgd, 0)) != NULL) {
		clif->bg_xy_remove(sd);
		if (hBGd->reveal_pos && (m = map->mapindex2mapid(bgd->mapindex)) == sd->bl.m)
			map->foreachinmap(hBG_reveal_pos, m,BL_PC,sd,2,0xFFFFFF);
	}
	return 0;
}

/**
 * Searches and removes battleground game specific 
 * items from the player's inventory.
 * @param sd as struct map_session_data
 */
void hBG_member_remove_bg_items(struct map_session_data *sd)
{
	int n;
	
	nullpo_retv(sd);
	
	if ((n = pc->search_inventory(sd,BLUE_SKULL)) >= 0)
		pc->delitem(sd, n, sd->status.inventory[n].amount, 0, 2, LOG_TYPE_CONSUME);
	
	if ((n = pc->search_inventory(sd,RED_SKULL)) >= 0)
		pc->delitem(sd, n, sd->status.inventory[n].amount, 0, 2,LOG_TYPE_CONSUME);
	
	if ((n = pc->search_inventory(sd,GREEN_SKULL)) >= 0)
		pc->delitem(sd, n, sd->status.inventory[n].amount, 0, 2,LOG_TYPE_CONSUME);
}

/**
 * Remove a player from a team.
 * @param sd pointer to session data.
 * @param flag type of leave.
 * @return Amount of player in the BG or 0 on failure.
 */
int hBG_team_leave(struct map_session_data *sd, int flag)
{ // Single Player leaves team
	int i;
	struct battleground_data *bgd;
	struct hBG_map_session_data *hBGsd;
	struct hBG_data *hBGd;
	struct map_session_data *pl_sd;
	struct guild *g;

	nullpo_ret(sd);
	
	if (!sd->bg_id)
		return 0;
	else if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return 0;
	else if ((bgd = bg->team_search(sd->bg_id)) == NULL)
		return 0;
	else if ((hBGd = getFromBGDATA(bgd, 0)) == NULL)
		return 0;

	// Packets
	hBG_send_dot_remove(sd);
	
	// Reset information.
	sd->bg_id = 0;
	hBGsd->bg_kills = 0;
	
	// Remove Battleground member data
	removeFromMSD(sd, 1);
	
	// Remove battleground items if any.
	hBG_member_remove_bg_items(sd);

	// Remove Guild Skill Buffs
	status_change_end(&sd->bl, SC_GUILDAURA, INVALID_TIMER);
	status_change_end(&sd->bl, SC_GDSKILL_BATTLEORDER, INVALID_TIMER);
	status_change_end(&sd->bl, SC_GDSKILL_REGENERATION, INVALID_TIMER);

	// Refresh Guild Information
	if (sd->status.guild_id && (g = guild->search(sd->status.guild_id)) != NULL) {
		clif->guild_belonginfo(sd, g);
		clif->guild_basicinfo(sd);
		clif->guild_allianceinfo(sd);
		clif->guild_memberlist(sd);
		clif->guild_skillinfo(sd);
		clif->guild_emblem(sd, g);
	} else {
		hBG_send_leave_single(sd, sd->status.name, "Leaving Battle...");
	}

	clif->charnameupdate(sd);
	clif->guild_emblem_area(&sd->bl);

	ARR_FIND(0, MAX_BG_MEMBERS, i, bgd->members[i].sd == sd);

	if (i < MAX_BG_MEMBERS) // Removes member from BG
		memset(&bgd->members[i], 0, sizeof(struct battleground_member_data));
	
	ARR_FIND(0, MAX_BG_MEMBERS, i, hBGd->g->member[i].sd == sd);
	
	if (i < MAX_BG_MEMBERS) // removes member from BG Guild
		memset(&hBGd->g->member[i].sd, 0, sizeof(hBGd->g->member[i].sd));
	
	if (hBGd->leader_char_id == sd->status.char_id)
		hBGd->leader_char_id = 0;
	
	if (--bgd->count > 0) {
		for (i = 0; i < MAX_BG_MEMBERS; i++) { // Update other BG members
			if ((pl_sd = bgd->members[i].sd) == NULL)
				continue;
			
			if (!hBGd->leader_char_id) { // Set new Leader first on the list
				hBGd->leader_char_id = pl_sd->status.char_id;
				clif->charnameupdate(pl_sd);
			}
			
			switch (flag) {
				case 3: hBG_send_expulsion(pl_sd, sd->status.name, "Kicked by AFK Status..."); break;
				case 2: hBG_send_expulsion(pl_sd, sd->status.name, "Kicked by AFK Report..."); break;
				case 1: hBG_send_expulsion(pl_sd, sd->status.name, "User has quit the game..."); break;
				case 0: hBG_send_leave_single(pl_sd, sd->status.name, "Leaving Battle..."); break;
			}
			
			hBG_guild_window_info(pl_sd);
			hBG_send_emblem(pl_sd, hBGd->g);
			hBG_send_guild_member_list(pl_sd);
		}
	}

/* This condition prevents to running Logout Event on logout, anyways leaving bg always calls this Event
	if (bgd && bgd->logout_event[0] && flag) */
		npc->event(sd, bgd->logout_event, 0);
	
	return bgd->count;
}

/**
 * Get guild data of the Battleground
 * @param bg_id ID of the battleground
 * @return guild of the battleground or NULL.
 */
struct guild* hBG_get_guild(int bg_id)
{
	struct battleground_data *bgd = bg->team_search(bg_id);
	struct hBG_data *hBGd;

	if (bgd == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL)
		return NULL;

	return hBGd->g;
}

/**
 * Remove all members from a BG Team.
 * @param bg_id ID of the battleground.
 * @param remove Boolean for removal of BG from team_db.
 * @return 0 on failure, 1 on success.
 */
int hBG_team_finalize(int bg_id, bool remove)
{ // Deletes BG Team from db
	int i;
	struct map_session_data *sd;
	struct battleground_data *bgd = bg->team_search(bg_id);
	struct hBG_data *hBGd;
	struct guild *g;

	if (bgd == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL)
		return 0;

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((sd = bgd->members[i].sd) == NULL)
			continue;

		hBG_send_dot_remove(sd);
		sd->bg_id = 0;

		// Remove Guild Skill Buffs
		status_change_end(&sd->bl, SC_GUILDAURA, INVALID_TIMER);
		status_change_end(&sd->bl, SC_GDSKILL_BATTLEORDER, INVALID_TIMER);
		status_change_end(&sd->bl, SC_GDSKILL_REGENERATION, INVALID_TIMER);

		if (sd->status.guild_id && (g = guild->search(sd->status.guild_id)) != NULL) {
			clif->guild_belonginfo(sd,g);
			clif->guild_basicinfo(sd);
			clif->guild_allianceinfo(sd);
			clif->guild_memberlist(sd);
			clif->guild_skillinfo(sd);
		} else {
			hBG_send_leave_single(sd, sd->status.name, "Leaving Battle...");
		}

		clif->charnameupdate(sd);
		clif->guild_emblem_area(&sd->bl);
	}

	if (remove) {
		idb_remove(bg->team_db, bg_id);
	} else {
		bgd->count = 0;
		hBGd->leader_char_id = 0;
		hBGd->team_score = 0;
		hBGd->created_at = 0;
		memset(&bgd->members, 0, sizeof(bgd->members));
	}

	return 1;
}

/**
 * Give items to the Battleground Team.
 * @param bg_id ID of the battleground.
 * @param nameid ID of the item.
 * @param amount Amount of the Item to be given.
 * @return void;
 */
void hBG_team_getitem(int bg_id, int nameid, int amount)
{
	struct battleground_data *bgd;
	struct map_session_data *sd;
	struct item_data *id;
	struct item it;
	int reward_rate = hBG_config_get("battle_configuration/bg_reward_rates");
	int get_amount, j, flag;

	if (amount < 1 || (bgd = bg->team_search(bg_id)) == NULL || (id = itemdb->exists(nameid)) == NULL)
		return;
	if (nameid != 7828 && nameid != 7829 && nameid != 7773)
		return;
	if (reward_rate != 100)
		amount = amount * reward_rate / 100;

	memset(&it, 0, sizeof(it));
	it.nameid = nameid;
	it.identify = 1;

	for (j = 0; j < MAX_BG_MEMBERS; j++) {
		if ((sd = bgd->members[j].sd) == NULL)
			continue;

		get_amount = amount;

		if ((flag = pc->additem(sd,&it,get_amount,LOG_TYPE_SCRIPT)))
			clif->additem(sd,0,0,flag);
	}
}

/**
 * Give kafra points to the team.
 * @param bg_id ID of the battleground.
 * @param amount Amount of kafra points to be given.
 */
void hBG_team_get_kafrapoints(int bg_id, int amount)
{
	struct battleground_data *bgd;
	struct map_session_data *sd;
	int i, get_amount, reward_rate = hBG_config_get("battle_configuration/bg_reward_rates");

	if ((bgd = bg->team_search(bg_id)) == NULL)
		return;

	if (reward_rate != 100)
		amount = amount * reward_rate/ 100;

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((sd = bgd->members[i].sd) == NULL)
			continue;

		get_amount = amount;
		pc->getcash(sd,0,get_amount);
	}
}

void hBG_add_rank_points(struct map_session_data *sd, int ranktype, int count)
{
	struct hBG_map_session_data *hBGsd;
	char message[100];
	
	nullpo_retv(sd);
	
	if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return;
	
	if (ranktype == BG_RANKED) {
		add2limit(hBGsd->stats.ranked_points, count, MAX_FAME);
		sprintf(message, "[Battlegrounds Ranked] Your ranking has increased by %d.", count);
		clif->disp_message(&sd->bl, message, SELF);
		hookStop();
	} else if (ranktype == BG_NORMAL) {
		add2limit(hBGsd->stats.points, count, MAX_FAME);
		sprintf(message, "[Battlegrounds Normal] Your ranking has increased by %d.", count);
		clif->disp_message(&sd->bl, message, SELF);
		hookStop();
	}
}

/* ==============================================================
 bg_arena (0 EoS | 1 Boss | 2 TI | 3 CTF | 4 TD | 5 SC | 6 CON | 7 RUSH | 8 DOM)
 bg_result (0 Won | 1 Tie | 2 Lost)
 ============================================================== */
void hBG_team_rewards(int bg_id, int nameid, int amount, int kafrapoints, int quest_id, const char *var, int add_value, int bg_arena, int bg_result)
{
	struct battleground_data *bgd = NULL;
	struct map_session_data *sd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;
	struct hBG_data *hBGd = NULL;
	struct item_data *id;
	struct item it;
	int j, flag, get_amount,
	reward_rate = hBG_config_get("battle_configuration/hBG_reward_rates"), fame = 0, type = 0;
	
	if (amount < 1 || (bgd = bg->team_search(bg_id)) == NULL || (id = itemdb->exists(nameid)) == NULL)
		return;

	if (reward_rate != 100) { // BG Reward Rates
		amount = amount * reward_rate / 100;
		kafrapoints = kafrapoints * reward_rate / 100;
	}
	
	memset(&it,0,sizeof(it));
	
	bg_result = cap_value(bg_result, 0, 2);
	
	if (nameid == 7828 || nameid == 7829 || nameid == 7773) {
		it.nameid = nameid;
		it.identify = 1;
	} else {
		nameid = 0;
	}

	for (j = 0; j < MAX_BG_MEMBERS; j++) {
		if ((sd = bgd->members[j].sd) == NULL)
			continue;
		else if ((hBGsd = getFromMSD(sd, 1)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL)
			continue;

		if (quest_id)
			quest->add(sd, quest_id);
		
		pc_setglobalreg(sd, script->add_str(var), pc_readglobalreg(sd,script->add_str(var)) + add_value);

		if (kafrapoints > 0) {
			get_amount = kafrapoints;
			pc->getcash(sd,0,get_amount);
		}

		if (nameid && amount > 0) {
			get_amount = amount;

			if ((flag = pc->additem(sd,&it,get_amount,LOG_TYPE_SCRIPT)))
				clif->additem(sd,0,0,flag);
		}
		
		type = hBG_config_get("battle_configuration/hBG_ranked_mode")?BG_RANKED:BG_NORMAL;
		
		switch (bg_result) {
			case 0: // Won
				add2limit(hBGsd->stats.wins,1,USHRT_MAX);
				fame = 100;
				if (sd->status.char_id == hBGd->leader_char_id) {
					add2limit(hBGsd->stats.wins_as_leader,1,USHRT_MAX);
					fame += 25;
				}
				
				hBG_add_rank_points(sd, fame, type);
				
				switch (bg_arena) {
					case 0:
						add2limit(hBGsd->stats.eos_wins,1,USHRT_MAX);
						break;
					case 1:
						add2limit(hBGsd->stats.boss_wins,1,USHRT_MAX);
						break;
					case 2:
						add2limit(hBGsd->stats.ti_wins,1,USHRT_MAX);
						break;
					case 3:
						add2limit(hBGsd->stats.ctf_wins,1,USHRT_MAX);
						break;
					case 4:
						add2limit(hBGsd->stats.td_wins,1,USHRT_MAX);
						break;
					case 5:
						add2limit(hBGsd->stats.sc_wins,1,USHRT_MAX);
						break;
					case 6:
						add2limit(hBGsd->stats.conquest_wins,1,USHRT_MAX);
						break;
					case 7:
						add2limit(hBGsd->stats.ru_wins,1,USHRT_MAX);
						break;
					case 8:
						add2limit(hBGsd->stats.dom_wins,1,USHRT_MAX);
						break;
				}
				break;
			case 1: // Tie
				add2limit(hBGsd->stats.ties,1,USHRT_MAX);
				fame = 75;
				if (sd->status.char_id == hBGd->leader_char_id) {
					add2limit(hBGsd->stats.ties_as_leader,1,USHRT_MAX);
					fame += 10;
				}
				hBG_add_rank_points(sd, fame, type);
				switch (bg_arena) {
					case 0: add2limit(hBGsd->stats.eos_tie,1,USHRT_MAX); break;
					case 1: add2limit(hBGsd->stats.boss_tie,1,USHRT_MAX); break;
					case 2: add2limit(hBGsd->stats.ti_tie,1,USHRT_MAX); break;
					case 3: add2limit(hBGsd->stats.ctf_tie,1,USHRT_MAX); break;
					case 4: add2limit(hBGsd->stats.td_tie,1,USHRT_MAX); break;
					case 5: add2limit(hBGsd->stats.sc_tie,1,USHRT_MAX); break;
						// No Tie for Conquest or Rush
					case 8: add2limit(hBGsd->stats.dom_tie,1,USHRT_MAX); break;
				}
				break;
			case 2: // Lost
				add2limit(hBGsd->stats.losses,1,USHRT_MAX);
				
				fame = 50;
				
				if (sd->status.char_id == hBGd->leader_char_id)
					add2limit(hBGsd->stats.losses_as_leader,1,USHRT_MAX);
				
				hBG_add_rank_points(sd, fame, type);
				
				switch (bg_arena) {
					case 0: add2limit(hBGsd->stats.eos_lost,1,USHRT_MAX); break;
					case 1: add2limit(hBGsd->stats.boss_lost,1,USHRT_MAX); break;
					case 2: add2limit(hBGsd->stats.ti_lost,1,USHRT_MAX); break;
					case 3: add2limit(hBGsd->stats.ctf_lost,1,USHRT_MAX); break;
					case 4: add2limit(hBGsd->stats.td_lost,1,USHRT_MAX); break;
					case 5: add2limit(hBGsd->stats.sc_lost,1,USHRT_MAX); break;
					case 6: add2limit(hBGsd->stats.conquest_losses,1,USHRT_MAX); break;
					case 7: add2limit(hBGsd->stats.ru_lost,1,USHRT_MAX); break;
					case 8: add2limit(hBGsd->stats.dom_lost,1,USHRT_MAX); break;
				}
				break;
		}
	}
}

/**
 * Build BG Guild Emulation data.
 * @params (void)
 * @return void.
 */
void hBG_build_guild_data(void)
{
	int i, j, k, gskill;
	memset(&bg_guild, 0, sizeof(bg_guild));

	for (i = 1; i <= MAX_BATTLEGROUND_TEAMS; i++) { // Emblem Data - Guild ID's
		FILE* fp = NULL;
		char gpath[256];

		j = i - 1;
		bg_guild[j].emblem_id = 1; // Emblem Index
		bg_guild[j].guild_id = SHRT_MAX - j;
		bg_guild[j].guild_lv = 1;
		bg_guild[j].max_member = MAX_BG_MEMBERS;

		// Skills
		if (j < 3) { // Clan Skills
			for (k = 0; k < MAX_GUILDSKILL; k++) {
				gskill = k + GD_SKILLBASE;
				bg_guild[j].skill[k].id = gskill;
				switch (gskill) {
					case GD_GLORYGUILD:
						bg_guild[j].skill[k].lv = 0;
						break;
					case GD_APPROVAL:
					case GD_KAFRACONTRACT:
					case GD_GUARDRESEARCH:
					case GD_BATTLEORDER:
					case GD_RESTORE:
					case GD_EMERGENCYCALL:
					case GD_DEVELOPMENT:
						bg_guild[j].skill[k].lv = 1;
						break;
					case GD_GUARDUP:
					case GD_REGENERATION:
						bg_guild[j].skill[k].lv = 3;
						break;
					case GD_LEADERSHIP:
					case GD_GLORYWOUNDS:
					case GD_SOULCOLD:
					case GD_HAWKEYES:
						bg_guild[j].skill[k].lv = 5;
						break;
					case GD_EXTENSION:
						bg_guild[j].skill[k].lv = 10;
						break;
				}
			}
		} else { // Other Data
			snprintf(bg_guild[j].name, NAME_LENGTH, "Team %d", i - 3); // Team 1, Team 2 ... Team 10
			strncpy(bg_guild[j].master, bg_guild[j].name, NAME_LENGTH);
			snprintf(bg_guild[j].position[0].name, NAME_LENGTH, "%s Leader", bg_guild[j].name);
			strncpy(bg_guild[j].position[1].name, bg_guild[j].name, NAME_LENGTH);
		}

		sprintf(gpath, "%s/%s/bg_%d.ebm", "plugins","hBG", i);
		if ((fp = fopen(gpath, "rb")) != NULL) {
			fseek(fp, 0, SEEK_END);
			bg_guild[j].emblem_len = (int)ftell(fp);
			fseek(fp, 0, SEEK_SET);
			fread(&bg_guild[j].emblem_data, 1, bg_guild[j].emblem_len, fp);
			fclose(fp);
		} else {
			ShowWarning("Error reading '"CL_WHITE"%s"CL_RESET"' emblem data file.\n", gpath);
		}
	}
	
	ShowStatus("Done reading '"CL_WHITE"%s"CL_RESET"' v%s emblem data files. [By Smokexyz]\n", pinfo.name, pinfo.version);

	// Guild Data - Guillaume
	strncpy(bg_guild[0].name, "Blue Team", NAME_LENGTH);
	strncpy(bg_guild[0].master, "General Guillaume", NAME_LENGTH);
	strncpy(bg_guild[0].position[0].name, "Blue Team Leader", NAME_LENGTH);
	strncpy(bg_guild[0].position[1].name, "Blue Team", NAME_LENGTH);

	// Guild Data - Croix
	strncpy(bg_guild[1].name, "Red Team", NAME_LENGTH);
	strncpy(bg_guild[1].master, "Prince Croix", NAME_LENGTH);
	strncpy(bg_guild[1].position[0].name, "Red Team Leader", NAME_LENGTH);
	strncpy(bg_guild[1].position[1].name, "Red Team", NAME_LENGTH);

	// Guild Data - Traitors
	strncpy(bg_guild[2].name, "Green Team", NAME_LENGTH);
	strncpy(bg_guild[2].master, "Mercenary", NAME_LENGTH);
	strncpy(bg_guild[2].position[0].name, "Green Team Leader", NAME_LENGTH);
	strncpy(bg_guild[2].position[1].name, "Green Team", NAME_LENGTH);
}

/**
 * Timer function for revealing/hiding mini map positions.
 * Also handles player AFK mechanic.
 * @see DBApply
 * @see hBG_send_xy_timer
 * @param data battleground data pointer.
 * @return int
 */
int hBG_send_xy_timer_sub(union DBKey key, struct DBData *data, va_list ap)
{
	struct battleground_data *bgd = DB->data2ptr(data);
	struct hBG_data *hBGd = getFromBGDATA(bgd, 0);
	char output[128];
	int i, m, idle_announce = hBG_config_get("battle_configuration/hBG_idle_announce"),
		idle_autokick = hBG_config_get("battle_configuration/hBG_idle_autokick");

	nullpo_ret(bgd);
	nullpo_ret(hBGd);

	m = map->mapindex2mapid(bgd->mapindex);
	hBGd->reveal_flag = !hBGd->reveal_flag; // Switch

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		struct map_session_data *sd = bgd->members[i].sd;
		struct hBG_map_session_data *hBGsd = NULL;

		if (sd == NULL || (hBGsd = getFromMSD(sd, 1)) == NULL)
			continue;

		if (idle_autokick && DIFF_TICK(sockt->last_tick, sd->idletime) >= idle_autokick
			&& hBGd->g && map->list[sd->bl.m].flag.battleground)
		{
			sprintf(output, "[Battlegrounds] %s has been kicked for being AFK.", sd->status.name);
			clif->broadcast2(&sd->bl, output, (int)strlen(output)+1, hBGd->color, 0x190, 20, 0, 0, BG);

			hBG_team_leave(sd,3);

			clif->message(sd->fd, "You have been kicked from the battleground because of your AFK status.");
			pc->setpos(sd, sd->status.save_point.map, sd->status.save_point.x, sd->status.save_point.y, 3);
			continue;
		} else if (sd->bl.x != bgd->members[i].x || sd->bl.y != bgd->members[i].y) { // xy update
			bgd->members[i].x = sd->bl.x;
			bgd->members[i].y = sd->bl.y;
			clif->bg_xy(sd);
		}
		
		if (hBGd->reveal_pos && hBGd->reveal_flag && sd->bl.m == m)
			map->foreachinmap(hBG_reveal_pos, m, BL_PC, sd, 1, hBGd->color);
		
		// Message for AFK Idling
		if (idle_announce && DIFF_TICK(sockt->last_tick, sd->idletime) >= idle_announce && !hBGsd->state.afk && hBGd->g)
		{ // Set AFK status and announce to the team.
			hBGsd->state.afk = 1;
			sprintf(output, "%s : %s seems to be away. AFK Warning - Can be kicked out with @reportafk.", hBGd->g->name, sd->status.name);
			hBG_send_chat_message(bgd, sd->bl.id, sd->status.name, output, (int)strlen(output) + 1);
		}
	}

	return 0;
}

/**
 * Timer function for revealing/hiding mini map positions.
 * Also handles player AFK time.
 * @see hBG_send_xy_timer_sub
 */
int hBG_send_xy_timer(int tid, int64 tick, int id, intptr_t data)
{
	bg->team_db->foreach(bg->team_db, hBG_send_xy_timer_sub, tick);
	return 0;
}

/**
 * Add an item to the floor at m (x,y)
 * @param bl pointer to the block list.
 * @param m map index
 * @param x map x co-ordinate
 * @param y map y co-ordinate
 * @param nameid ID of the item to be dropped.
 * @param amount Amount of the item to be dropped.
 * @return count of the item dropped.
 */
int hBG_addflooritem_area(struct block_list* bl, int16 m, int16 x, int16 y, int nameid, int amount)
{
	struct item item_tmp;
	int count, range, i;
	short mx, my;
	
	memset(&item_tmp, 0, sizeof(item_tmp));
	item_tmp.nameid = nameid;
	item_tmp.identify = 1;
	
	if (bl != NULL) m = bl->m;
	
	count = 0;
	range = (int)sqrt((float)amount) +2;
	
	for ( i = 0; i < amount; i++) {
		if (bl != NULL)
			map->search_freecell(bl, 0, &mx, &my, range, range, 0);
		else {
			mx = x; my = y;
			map->search_freecell(NULL, m, &mx, &my, range, range, 1);
		}
		
		count += (map->addflooritem(bl, &item_tmp, 1, m, mx, my, 0, 0, 0, 4) != 0) ? 1 : 0;
	}
	
	return count;
}

// @TODO
void hBG_bg_ranking_display(struct map_session_data *sd, bool ranked)
{
	struct hBG_map_session_data *hBGsd = NULL;
	
	nullpo_retv(sd);
	
	if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return;
	
	// print shit.
	
}

void hBG_record_mobkills(struct map_session_data *sd, struct mob_data *md)
{
	struct battleground_data *bgd = NULL;
	struct hBG_data *hBGd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;

	nullpo_retv(sd);
	nullpo_retv(md);

	if (map->list[sd->bl.m].flag.battleground && sd->bg_id) {
		int i;
		if ((bgd = bg->team_search(sd->bg_id)) == NULL)
			return;
		if ((hBGd = getFromBGDATA(bgd, 0)) == NULL)
			return;
		if ((hBGsd = getFromMSD(sd, 1)) == NULL)
			return;

		ARR_FIND(0, MAX_BG_MEMBERS, i, bgd->members[i].sd == sd);

		if (i >= MAX_BG_MEMBERS)
			return;

		switch ( md->class_)
		{
		case E_BAPHOMET2:
		case E_LORD_OF_DEATH2:
		case E_DARK_LORD:
		case E_KTULLANUX:
		case E_DARK_SNAKE_LORD:
			add2limit(hBGsd->stats.boss_killed, 1, USHRT_MAX);
			break;
		case E_TURTLE_GENERAL:
		case E_APOCALIPS_H:
			add2limit(hBGsd->stats.guardian_stone_kills, 1, USHRT_MAX);
			break;
		case E_FALLINGBISHOP:
			if (map->list[sd->bl.m].flag.battleground == 2)
				add2limit(hBGsd->stats.ru_captures, 1, USHRT_MAX);
			break;
		case OBJ_NEUTRAL:
			if (strcmpi(map->list[sd->bl.m].name, "bat_a03") == 0)
				add2limit(hBGsd->stats.boss_flags, 1, USHRT_MAX);
			break;
		case BARRICADE_:
			if (strcmpi(map->list[sd->bl.m].name, "bat_a01") == 0)
				add2limit(hBGsd->stats.barricade_kills, 1, USHRT_MAX);
			break;
		}
	}
}

void hBG_record_damage(struct block_list *src, struct block_list *target, int damage)
{
	struct block_list *s_bl = NULL;
	struct map_session_data *sd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;

	if (src == NULL || target == NULL || src == target || damage <= 0)
		return;

	if ((s_bl = battle->get_master(src)) == NULL)
		s_bl = src;

	if (s_bl->type != BL_PC)
		return;

	if ((sd = BL_UCAST(BL_PC, s_bl)) == NULL)
		return;

	if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return;

	switch ( target->type)
	{
	case BL_PC:
		{
			struct map_session_data *tsd = NULL;
			struct hBG_map_session_data *hBGtsd = NULL;

			if ((tsd = BL_UCAST(BL_PC, target)) == NULL)
				break;
			if ((hBGtsd = getFromMSD(tsd, 1)) == NULL)
				break;

			if (map->list[src->m].flag.battleground && sd->bg_id) {
				add2limit(hBGsd->stats.total_damage_done, damage, UINT_MAX);
				add2limit(hBGtsd->stats.total_damage_received, damage, UINT_MAX);

				if (hBGsd->stats.best_damage < damage)
					hBGsd->stats.best_damage = damage;
			}
		}
		break;
	case BL_MOB:
		{
			struct mob_data *md = BL_UCAST(BL_MOB, target);
			if (map->list[src->m].flag.battleground && sd->bg_id && md->class_ >= E_BAPHOMET2 && md->class_ <= E_FALLINGBISHOP)
				add2limit(hBGsd->stats.boss_damage, damage, USHRT_MAX);
		}
		break;
	}
}

/**
 * Warps a Team
 * @see hBG_warp
 */
int hBG_team_warp(int bg_id, unsigned short mapindex, short x, short y)
{ // Warps a Team
	int i;
	struct battleground_data *bgd = bg->team_search(bg_id);
	if( bgd == NULL ) return false;
	if( mapindex == 0 )
	{
		mapindex = bgd->mapindex;
		x = bgd->x;
		y = bgd->y;
	}

	for( i = 0; i < MAX_BG_MEMBERS; i++ )
		if( bgd->members[i].sd != NULL ) pc->setpos(bgd->members[i].sd, mapindex, x, y, CLR_TELEPORT);
	return true;
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     @Commands
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * Display battleground rankings.
 */
ACMD(bgrank)
{
	char mode[7];
	char atcmd_output[256];
	
	memset(atcmd_output, '\0', sizeof(atcmd_output));
	
	if (!*message || sscanf(message, "%7s", mode) < 1) {
		sprintf(atcmd_output, "Please, enter a battleground mode (usage: @bgrank <ranked/normal>).");
		clif->message(fd, atcmd_output);
		return false;
	}
	
	if (strncmpi(mode, "ranked", 7) == 0) {
		hBG_bg_ranking_display(sd, true);
	} else if (strncmpi(mode, "normal", 7) == 0) {
		hBG_bg_ranking_display(sd, true);
	} else {
		sprintf(atcmd_output, "Please, enter a battleground mode (usage: @bgrank <ranked/normal>).");
		clif->message(fd, atcmd_output);
		return false;
	}
	
	return true;
}

ACMD(reportafk)
{
	struct map_session_data *pl_sd = NULL;
	struct hBG_data *hBGd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;
	struct hBG_map_session_data *hBGpl_sd = NULL;
	struct battleground_data *bgd = NULL;
	
	if ((hBGsd = getFromMSD(sd, 1)) == NULL) {
		clif->message(fd, "You are not in battlegrounds.");
		return false;
	}
	
	if ((bgd = bg->team_search(sd->bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL)
		clif->message(fd, "This command is reserved for Battlegrounds Only.");
	else if (!(hBGd->leader_char_id == sd->status.char_id) && hBG_config_get("battle_configuration/hBG_reportafk_leaderonly"))
		clif->message(fd, "This command is reserved for Team Leaders Only.");
	else if (!*message)
		clif->message(fd, "Please, enter the character name (usage: @reportafk <name>).");
	else if ((pl_sd = map->nick2sd(message)) == NULL)
		clif->message(fd, msg_txt(3)); // Character not found.
	else if ((hBGpl_sd = getFromMSD(pl_sd, 0)) == NULL)
		clif->message(fd, "Destination Player is not in battlegrounds.");
	else if (sd->bg_id != pl_sd->bg_id)
		clif->message(fd, "Destination Player is not in your Team.");
	else if (sd == pl_sd)
		clif->message(fd, "You cannot kick yourself.");
	else if (!hBGpl_sd->state.afk)
		clif->message(fd, "The player is not AFK on this Battleground.");
	else
	{ // Everything is fine!
		char atcmd_output[256];
		
		 // Kick the player and send a message.
		hBG_team_leave(pl_sd, 2);
		clif->message(pl_sd->fd, "You have been kicked from Battleground because of your AFK status.");
		pc->setpos(pl_sd, pl_sd->status.save_point.map, pl_sd->status.save_point.x, pl_sd->status.save_point.y, 3);
		// Message the source player and announce to team.
		sprintf(atcmd_output, "%s has been successfully kicked.", pl_sd->status.name);
		clif->broadcast2(&sd->bl, atcmd_output, (int)strlen(atcmd_output)+1, hBGd->color, 0x190, 20, 0, 0, BG);
		return true;
	}
	
	return false;
}

/**
 * Change Team Leader.
 */
ACMD(leader)
{
	struct map_session_data *pl_sd = NULL;
	struct hBG_data *hBGd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;
	struct hBG_map_session_data *hBGpl_sd = NULL;
	struct battleground_data *bgd = NULL;
	
	if ((hBGsd = getFromMSD(sd, 1)) == NULL) {
		clif->message(fd, "You are not in battlegrounds.");
		return false;
	}
	
	if ((bgd = bg->team_search(sd->bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL)
		clif->message(fd, "This command is reserved for Battlegrounds Only.");
	if( sd->ud.skilltimer != INVALID_TIMER )
		clif->message(fd, "Command not allow while casting a skill.");
	else if (!(hBG_config_get("battle_configuration/hBG_leader_change")))
		clif->message(fd, "This command is disabled.");
	else if (!(hBGd->leader_char_id == sd->status.char_id))
		clif->message(fd, "This command is reserved for Team Leaders Only.");
	else if (!*message)
		clif->message(fd, "Please, enter the character name (usage: @leader <name>).");
	else if ((pl_sd = map->nick2sd(message)) == NULL)
		clif->message(fd, msg_txt(3)); // Character not found.
	else if ((hBGpl_sd = getFromMSD(pl_sd, 0)) == NULL)
		clif->message(fd, "Destination Player is not in battlegrounds.");
	else if (sd->bg_id != pl_sd->bg_id)
		clif->message(fd, "Destination Player is not in your Team.");
	else if (sd == pl_sd)
		clif->message(fd, "You are already the Team Leader.");
	else
	{ // Everytest OK!
		char atcmd_output[256];
		sprintf(atcmd_output, "Team Leader transfered to [%s]", pl_sd->status.name);
		clif->broadcast2(&sd->bl, atcmd_output, (int)strlen(atcmd_output)+1, hBGd->color, 0x190, 20, 0, 0, BG);
		hBGd->leader_char_id = pl_sd->status.char_id;
		clif->charnameupdate(sd);
		clif->charnameupdate(pl_sd);
		return true;
	}
	return false;
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     Script Commands
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Send out a battleground announcement.
 * @param mes
 * @param fontColor
 * @param fontType
 * @param fontSize
 * @param fontAlign
 * @param fontY
 */
BUILDIN(hBG_announce)
{
	const char *mes       = script_getstr(st,2);
	const char *fontColor = script_hasdata(st,3) ? script_getstr(st,3) : NULL;
	int         fontType  = script_hasdata(st,4) ? script_getnum(st,4) : 0x190; // default fontType (FW_NORMAL)
	int         fontSize  = script_hasdata(st,5) ? script_getnum(st,5) : 12;    // default fontSize
	int         fontAlign = script_hasdata(st,6) ? script_getnum(st,6) : 0;     // default fontAlign
	int         fontY     = script_hasdata(st,7) ? script_getnum(st,7) : 0;     // default fontY

	clif->broadcast2(NULL, mes, (int)strlen(mes)+1, (unsigned int)strtol(fontColor, (char **)NULL, 0), fontType, fontSize, fontAlign, fontY, ALL_CLIENT);
	return true;
}

/**
 * Count the number of logins per IP in battleground
 * @return count of accounts on same ip.
 */
BUILDIN(hBG_logincount)
{
	struct map_session_data *sd = script->rid2sd(st);
	int i = 0;

	if (sd)
		i = hBG_countlogin(sd,true);

	script_pushint(st,i);
	return true;
}

/**
 * Create a BG Team.
 * @param map_name Respawn Map Name
 * @param map_x Respawn Map X
 * @param map_y Respawn Map Y
 * @param guild_index BG Guild Index
 * @param ev Logout Event
 * @param dev Die Event
 */
BUILDIN(hBG_team_create)
{
	const char *map_name, *ev = "", *dev = "";
	int x, y, m = 0, guild_index, bg_id;

	map_name = script_getstr(st, 2);
	if (strcmp(map_name,"-") != 0 && (m = mapindex->name2id(map_name)) == 0) {
		script_pushint(st, 0);
		return false;
	}

	x = script_getnum(st, 3);
	y = script_getnum(st, 4);
	guild_index = script_getnum(st, 5);
	ev = script_getstr(st, 6); // Logout Event
	dev = script_getstr(st, 7); // Die Event

	guild_index = cap_value(guild_index, 0, 12);
	bg_id = hBG_create(m, x, y, guild_index, ev, dev);

	script_pushint(st, bg_id);
	return true;
}

/**
 * Create a Queue
 * @param queue_name Name of the Queue
 * @param jev Join Event
 * @param min_level Minimum level to join the queue.
 * @return queue Id
 */
BUILDIN(hBG_queue_create)
{
	const char *queue_name, *jev;
	int min_level = 0;

	queue_name = script_getstr(st, 2);
	jev = script_getstr(st, 3);
	
	if (script_hasdata(st, 4))
		min_level = script_getnum(st, 4);

	script_pushint(st, hBG_queue_create(queue_name, jev, min_level));

	return true;
}


/**
 * Changes/Sets the Queue's Join Event.
 * @param queue_id
 * @param jev On Join Event
 */
BUILDIN(hBG_queue_event)
{
	struct hBG_queue_data *hBGqd;
	const char *join_event;
	int q_id;

	q_id = script_getnum(st,2);

	if ((hBGqd = hBG_queue_search(q_id)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	join_event = script_getstr(st,3);

	safestrncpy(hBGqd->join_event, join_event, sizeof(hBGqd->join_event));

	script_pushint(st, 1);
	return true;
}

/**
 * Makes a player join a queue.
 * @param Queue ID
 */
BUILDIN(hBG_queue_join)
{
	int q_id;
	struct map_session_data *sd = script->rid2sd(st);

	nullpo_retr(false, sd);

	q_id = script_getnum(st,2);

	script_pushint(st, hBG_queue_join(sd, q_id));

	return true;
}

/**
 * Makes party join a queue.
 * @param Party ID
 * @param Queue ID
 */
BUILDIN(hBG_queue_partyjoin)
{
	int q_id, i, party_id;
	struct map_session_data *sd;
	struct party_data *p;

	party_id = script_getnum(st,2);

	if (party_id <= 0 || (p = party->search(party_id)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	q_id = script_getnum(st,3);

	if (hBG_queue_search(q_id) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	for (i = 0; i < MAX_PARTY; i++) {
		if ((sd = p->data[i].sd) == NULL)
			continue;
		hBG_queue_join(sd,q_id);
	}

	script_pushint(st, 1);

	return true;
}

/**
 * Makes a player leave a queue.
 * @param Queue ID
 */
BUILDIN(hBG_queue_leave)
{
	int q_id;
	struct map_session_data *sd = script->rid2sd(st);

	nullpo_retr(false, sd);

	q_id = script_getnum(st,2);

	script_pushint(st, hBG_queue_leave(sd, q_id));

	return true;
}

/**
 * Request queue information
 * @param Queue ID
 * @param Information Type
 *        0) Users
 *        1) Copy user list to $@qmembers$ variable and return count.
 */
BUILDIN(hBG_queue_data)
{
	struct hBG_queue_data *hBGqd;
	int q_id = script_getnum(st,2),
	type = script_getnum(st,3);

	if ((hBGqd = hBG_queue_search(q_id)) == NULL) {
		script_pushint(st,0);
		return false;
	}

	switch (type) {
		case 0:
			script_pushint(st, hBGqd->users);
			return true;
		case 1: // User List
		{
			int j = 0;
			struct map_session_data *sd;
			struct hBG_queue_member *head;
			head = hBGqd->first;
			while (head) {
				if ((sd = head->sd) != NULL) {
					mapreg->setregstr(reference_uid(script->add_str("$@qmembers$"),j),sd->status.name);
					j++;
				}
				head = head->next;
			}
			script_pushint(st,j);
		}
			return true;
		default:
			ShowError("script:hBG_queue_data: unknown queue data type %d.\n", type);
			break;
	}

	script_pushint(st, 0);
	return true;
}

/**
 * Adds all members in queue to a BG team.
 * @param Queue ID
 * @param Max Team Members
 * @param Respawn Map Name
 * @param Respawn Map X
 * @param Respawn Map Y
 * @param BG Guild Index
 * @param Logout Event
 * @param Die Event
 * @return Battleground Id
 */
BUILDIN(hBG_queue2team)
{
	struct hBG_queue_data *hBGqd;
	struct hBG_queue_member *qm;
	const char *map_name, *ev = "", *dev = "";
	int q_id, max, x, y, i, m=0, guild_index, bg_id;

	q_id = script_getnum(st,2);
	if ((hBGqd = hBG_queue_search(q_id)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	max = script_getnum(st,3);
	map_name = script_getstr(st,4);

	if (strcmp(map_name,"-") != 0 && (m = mapindex->name2id(map_name)) == 0) {
		script_pushint(st, 0);
		return false;
	}

	x = script_getnum(st,5);
	y = script_getnum(st,6);
	guild_index = script_getnum(st,7);
	ev = script_getstr(st,8); // Logout Event
	dev = script_getstr(st,9); // Die Event

	guild_index = cap_value(guild_index, 0, 12);

	if ((bg_id = hBG_create(m, x, y, guild_index, ev, dev)) == 0) {
		script_pushint(st, 0);
		return false;
	}

	i = 0; // Counter
	while ((qm = hBGqd->first) != NULL && i < max && i < MAX_BG_MEMBERS) {
		if (qm->sd && hBG_team_join(bg_id, qm->sd)) {
			mapreg->setreg(reference_uid(script->add_str("$@arenamembers"), i), qm->sd->bl.id);
			hBG_queue_member_remove(hBGqd, qm->sd->bl.id);
			i++;
		} else {
			break; // Failed? Should not. Anyway, to avoid a infinite loop
		}
	}

	mapreg->setreg(script->add_str("$@arenamembersnum"), i);

	script_pushint(st, bg_id);

	return true;
}

/**
 * Joins the first player in the queue to the given team and warps him.
 * @param Queue ID
 * @param Battleground ID
 * @param Spawn Map Name
 * @param Spawn Map X Co-ordinate
 * @param Spawn Map Y Co-ordinate
 */
BUILDIN(hBG_queue2team_single)
{
	const char* map_name;
	struct hBG_queue_data *hBGqd;
	struct map_session_data *sd;
	int x, y, m, bg_id, q_id;

	q_id = script_getnum(st,2);

	if ((hBGqd = hBG_queue_search(q_id)) == NULL || !hBGqd->first || !hBGqd->first->sd) {
		script_pushint(st, 0);
		return false;
	}

	bg_id = script_getnum(st, 3);
	map_name = script_getstr(st, 4);

	if ((m = mapindex->name2id(map_name)) == 0) {
		script_pushint(st, 0);
		return false;
	}

	x = script_getnum(st, 5);
	y = script_getnum(st, 6);
	sd = hBGqd->first->sd;

	if (hBG_team_join(bg_id, sd)) {
		hBG_queue_member_remove(hBGqd, sd->bl.id);
		pc->setpos(sd, m, x, y, CLR_TELEPORT);
		script_pushint(st, 1);
	}

	return true;
}
/**
 * Check if the given BG Queue can start a BG in the given mode.
 * @param Queue ID
 * @param Type
 * @param Teams
 * @param Required Minimum Players
 * @return 1 can start, 0 cannot start.
 */
BUILDIN(hBG_queue_checkstart)
{
	int q_id, result = 0;
	struct hBG_queue_data *hBGqd;

	q_id = script_getnum(st,2);

	if ((hBGqd = hBG_queue_search(q_id)) != NULL) {
		int type, req_min, teams;

		type = script_getnum(st,3);
		teams = script_getnum(st,4);
		req_min = script_getnum(st,5);

		switch (type) {
			case 0: // Lineal, as they Join
			case 1: // Random
				if (hBGqd->users >= (req_min * teams))
					result = 1;
				break;
			default:
				result = 0;
				break;
		}
	}

	script_pushint(st, result);

	return true;
}

/**
 * Build BG Teams from one Queue
 * @param Queue ID
 * @param Minimum Players
 * @param Maximum Players per Team
 * @param Type
 * @param ... Team ID
 */
BUILDIN(hBG_queue2teams)
{ // Send Users from Queue to Teams. Requires previously created teams.
	struct hBG_queue_data *hBGqd;
	int t, bg_id = 0, total_teams = 0, q_id, min, max, type, limit = 0;
	int arg_offset = 6;
	struct map_session_data *sd;

	q_id = script_getnum(st,2); // Queue ID
	if ((hBGqd = hBG_queue_search(q_id)) == NULL) {
		ShowError("buildin_hBG_queue2teams: Non existant queue id received %d.\n", q_id);
		script_pushint(st, 0);
		return false;
	}

	min = script_getnum(st,3); // Min Members per Team
	max = script_getnum(st,4); // Max Members per Team
	type = script_getnum(st,5); // Team Building Method

	t = arg_offset; // Team ID's to build
	while (script_hasdata(st,t) && t < MAX_BATTLEGROUND_TEAMS+arg_offset) {
		bg_id = script_getnum(st,t);
		if (bg->team_search(bg_id) == NULL) {
			ShowError("buildin_hBG_queue2teams: Non existant team Id received %d.\n", bg_id);
			script_pushint(st, 0);
			return false;
		}
		t++;
	}
	total_teams = t - arg_offset;

	if (total_teams < 2) {
		ShowError("buildin_hBG_queue2teams: Less than 2 teams received to build members.\n");
		script_pushint(st, 0);
		return false;
	}

	if (hBGqd->users < min) {
		ShowError("buildin_hBG_queue2teams: Less than minimum %d queue members received (%d).\n", min, (int) hBGqd->users);
		script_pushint(st, 0);
		return false;
	}
	// How many players are we going to take from the Queue
	limit = min(max * total_teams, hBGqd->users);

	switch (type) {
		case 0: // Lineal - Maybe to keep party together
		{
			t = 0;
			int i = 0;
			for (i = 0; i < limit; i++) {
				if ((i%(limit/total_teams)) == 0) // Switch Team
					bg_id = script_getnum(st,t+arg_offset);
				
				if (!hBGqd->first || (sd = hBGqd->first->sd) == NULL)
					break; // No more people to join Teams
				
				hBG_team_join(bg_id, sd);
				hBG_queue_member_remove(hBGqd, sd->bl.id);
				
				// Increment Team counter or break
				if (t++ >= total_teams)
					break;
			}
		}
			break;
		default:
		case 1: // Random
		{
			t = 0;
			int pos = 0, i=0;
			struct hBG_queue_member *qm;

			for (i=0; t < limit; i++) {
				if ((i%(limit/total_teams)) == 0) // Switch Team
					bg_id = script_getnum(st,t+arg_offset);

				pos = 1 + rand() % (limit - i);
				
				if ((qm = hBG_queue_member_get(hBGqd, pos)) == NULL || (sd = qm->sd) == NULL)
					break;

				hBG_team_join(bg_id, sd);
				hBG_queue_member_remove(hBGqd, sd->bl.id);
				
				// Increment Team Counter or break
				if (t++ >= total_teams)
					break;
			}
		}
			break;
	}

	script_pushint(st, 1);

	return true;
}

/**
 * Fill teams with members from the given Queue.
 * @param Queue ID
 * @param Max Players Per Team
 * @param Balanceing method
 * @param Team 1 ... Team 13
 */
BUILDIN(hBG_balance_teams)
{
	struct hBG_queue_data *hBGqd;
	struct hBG_queue_member *head;
	struct battleground_data *bgd, *p_bg;
	int i, c, q_id, bg_id, m_bg_id = 0, max, min, type;
	struct map_session_data *sd;
	bool balanced;

	q_id = script_getnum(st,2);

	if ((hBGqd = hBG_queue_search(q_id)) == NULL || hBGqd->users <= 0) {
		ShowError("buildin_hBG_balance_teams: Non existant Queue Id or the queue is empty.\n");
		script_pushint(st, 0);
		return false;
	}

	max = script_getnum(st,3);
	if (max > MAX_BG_MEMBERS)
		max = MAX_BG_MEMBERS;
	min = MAX_BG_MEMBERS + 1;
	type = script_getnum(st, 4);

	i = 5; // Team IDs to build
	
	while (script_hasdata(st, i) && i-5 < 13) {
		bg_id = script_getnum(st, i);
		if ((bgd = bg->team_search(bg_id)) == NULL) {
			ShowError("buildin_hBG_balance_teams: Non existant team id received %d.\n", bg_id);
			script_pushint(st, 0);
			return false;
		}

		if (bgd->count < min)
			min = bgd->count;
		i++;
	}

	c = i - 5; // Teams Found

	if (c < 2 || min >= max)
		return true; // No Balance Required

	if (type < 3) {
		while ((head = hBGqd->first) != NULL && (sd = head->sd) != NULL) {
			p_bg = NULL;
			balanced = true;
			min = MAX_BG_MEMBERS + 1;

			// Search the Current Minimum and Balance status
			for (i = 0; i < c; i++) {
				bg_id = script_getnum(st,i+5);
				
				if ((bgd = bg->team_search(bg_id)) == NULL)
					break; // Should not happen. Teams already check
				
				if (p_bg && p_bg->count != bgd->count)
					balanced = false; // Teams still with different member count

				if (bgd->count < min) {
					m_bg_id = bg_id;
					min = bgd->count;
				}
				p_bg = bgd;
			}

			if (min >= max) break; // Balance completed

			if (hBG_config_get("battle_configuration/hBG_balanced_queue") && balanced && hBGqd->users < c)
				break; // No required users on queue to keep balance

			hBG_team_join(m_bg_id, sd);
			hBG_queue_member_remove(hBGqd, sd->bl.id);

			if ((bgd = bg->team_search(m_bg_id)) != NULL && bgd->mapindex)
				pc->setpos(sd, bgd->mapindex, bgd->x, bgd->y, CLR_TELEPORT); // Joins and Warps
		}
	} else {
		ShowError("buildin_hBG_balance_teams: Invalid type %d given for argument #3.\n", type);
		script_pushint(st, 0);
		return false;
	}

	script_pushint(st, 1);

	return true;
}

/**
 * Waiting Room to Battleground Teams
 * @param Map Name
 * @param Map X
 * @param Map Y
 * @param BG Guild Index
 * @param Logout Event
 * @param Die Event
 */
BUILDIN(hBG_waitingroom2bg)
{
	struct npc_data *nd;
	struct chat_data *cd;
	const char *map_name, *ev = "", *dev = "";
	int x, y, i, m=0, guild_index, bg_id;
	struct map_session_data *sd;

	nd = (struct npc_data *)map->id2bl(st->oid);

	if (nd == NULL || (cd = (struct chat_data *)map->id2bl(nd->chat_id)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	map_name = script_getstr(st,2);

	if (strcmp(map_name, "-") != 0 && (m = mapindex->name2id(map_name)) == 0) {
		script_pushint(st, 0);
		return false;
	}

	x = script_getnum(st, 3);
	y = script_getnum(st, 4);
	guild_index = script_getnum(st, 5);
	ev = script_getstr(st, 6); // Logout Event
	dev = script_getstr(st, 7); // Die Event

	guild_index = cap_value(guild_index, 0, 12);

	if ((bg_id = hBG_create(m, x, y, guild_index, ev, dev)) == 0) { // Creation failed
		script_pushint(st, 0);
		return false;
	}

	for (i = 0; i < cd->users && i < MAX_BG_MEMBERS; i++) {
		if ((sd = cd->usersd[i]) != NULL && hBG_team_join(bg_id, sd))
			mapreg->setreg(reference_uid(script->add_str("$@arenamembers"), i), sd->bl.id);
		else
			mapreg->setreg(reference_uid(script->add_str("$@arenamembers"), i), 0);
	}

	mapreg->setreg(script->add_str("$@arenamembersnum"), i);

	script_pushint(st, bg_id);

	return true;
}

/**
 * Adds the first player from the given NPC's
 * waiting room to BG Team.
 * @param Battleground Id
 * @param Map Name
 * @param Map X
 * @param Map Y
 * @param NPC Name
 */
BUILDIN(hBG_waitingroom2bg_single)
{
	const char* map_name;
	struct npc_data *nd;
	struct chat_data *cd;
	struct map_session_data *sd;
	int x, y, m, bg_id;

	bg_id = script_getnum(st,2);
	map_name = script_getstr(st,3);

	if ((m = mapindex->name2id(map_name)) == 0) {
		script_pushint(st, 0);
		return false; // Invalid Map
	}

	x = script_getnum(st,4);
	y = script_getnum(st,5);
	nd = npc->name2id(script_getstr(st,6));

	if (nd == NULL || (cd = (struct chat_data *)map->id2bl(nd->chat_id)) == NULL || cd->users <= 0) {
		script_pushint(st, 0);
		return false;
	}

	if ((sd = cd->usersd[0]) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	if (hBG_team_join(bg_id, sd)) {
		pc->setpos(sd, m, x, y, CLR_TELEPORT);
		script_pushint(st,1);
	} else {
		script_pushint(st,0);
	}

	return true;
}

/**
 * Set the Respawn Location of a BG team
 * @param Battleground Id
 * @param Map X
 * @param Map Y
 */
BUILDIN(hBG_team_setxy)
{
	struct battleground_data *bgd;
	int bg_id;

	bg_id = script_getnum(st,2);

	if ((bgd = bg->team_search(bg_id)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	bgd->x = script_getnum(st,3);
	bgd->y = script_getnum(st,4);

	script_pushint(st, 1);

	return true;
}

/**
 * Reveal the location of a BG Team on the minimap.
 * @param Battleground ID
 */
BUILDIN(hBG_team_reveal)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int bg_id;

	bg_id = script_getnum(st,2);

	if ((bgd = bg->team_search(bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	hBGd->reveal_pos = true; // Reveal Position Mode

	script_pushint(st, 1);

	return true;
}

/**
 * Conceal the location of a BG Team from the minimap.
 * @param Battleground ID
 */
BUILDIN(hBG_team_conceal)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	struct map_session_data *sd;
	int bg_id,i=0;

	bg_id = script_getnum(st,2);

	if ((bgd = bg->team_search(bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((sd = bgd->members[i].sd) == NULL)
			continue;

		hBG_send_dot_remove(sd);
	}

	hBGd->reveal_pos = false; // Conceal Position Mode

	script_pushint(st, 1);

	return true;
}

/**
 * Set Quest for a BG Team
 * @param Battleground ID
 * @param Quest ID
 */
BUILDIN(hBG_team_setquest)
{
	struct battleground_data *bgd;
	struct map_session_data *sd;
	int i, bg_id, qid;

	bg_id = script_getnum(st,2);
	qid = script_getnum(st,3);

	if (bg_id <= 0 || (bgd = bg->team_search(bg_id)) == NULL) {
		ShowError("buildin_hBG_team_setquest: Invalid Team ID %d given.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	if (qid <= 0 || quest->db(qid) == NULL) {
		ShowError("buildin_hBG_team_setquest: Invalid Quest ID %d given.\n", qid);
		script_pushint(st, 0);
		return false;
	}

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((sd = bgd->members[i].sd) == NULL)
			continue;

		quest->add(sd,qid);
	}

	script_pushint(st, 1);

	return true;
}

/**
 * Set Viewpoint for a player.
 * @see hBG_viewpointmap
 */
int hBG_viewpointmap_sub(struct block_list *bl, va_list ap)
{
	struct map_session_data *sd;
	int npc_id, type, x, y, id, color;
	npc_id = va_arg(ap,int);
	type = va_arg(ap,int);
	x = va_arg(ap,int);
	y = va_arg(ap,int);
	id = va_arg(ap,int);
	color = va_arg(ap,int);
	sd = (struct map_session_data *)bl;

	clif->viewpoint(sd,npc_id,type,x,y,id,color);

	return 0;
}

/**
 * Set Viewpoint on minimap for a player.
 * @param Map Name
 * @param Type
 *         0 = display mark for 15 seconds
 *         1 = display mark until dead or teleported
 *         2 = remove mark
 * @param Map X
 * @param Map Y
 * @param ID (Unique ID of the viewpoint)
 * @param Color
 */
BUILDIN(hBG_viewpointmap)
{
	int type,x,y,id,color,m;
	const char *map_name;

	map_name = script_getstr(st,2);

	if ((m = map->mapname2mapid(map_name)) < 0) {
		script_pushint(st, 0);
		return false; // Invalid Map
	}

	type=script_getnum(st,3);
	x=script_getnum(st,4);
	y=script_getnum(st,5);
	id=script_getnum(st,6);
	color=script_getnum(st,7);

	map->foreachinmap(hBG_viewpointmap_sub,m,BL_PC,st->oid,type,x,y,id,color);

	script_pushint(st, 1);

	return true;
}

/**
 * Reveal a monster's location on minimap
 * @param Monster Id
 * @param Type
 *         0 = display mark for 15 seconds
 *         1 = display mark until dead or teleported
 *         2 = remove mark
 * @param Color
 */
BUILDIN(hBG_monster_reveal)
{
	struct block_list *mbl;
	int id = script_getnum(st,2),
	flag = script_getnum(st,3),
	color = script_getnum(st,4);

	if (id == 0 || (mbl = map->id2bl(id)) == NULL || mbl->type != BL_MOB) {
		script_pushint(st, 0);
		return false;
	}

	map->foreachinmap(hBG_viewpointmap_sub, mbl->m, BL_PC, st->oid, flag, mbl->x, mbl->y, mbl->id, color);

	script_pushint(st, 1);

	return true;
}

/**
 * Set monster as an ally to a BG Team.
 * @param Monster ID
 * @param Battleground ID
 */
BUILDIN(hBG_monster_set_team)
{
	struct mob_data *md;
	struct block_list *mbl;
	int id = script_getnum(st,2),
	bg_id = script_getnum(st,3);

	if (id == 0 || (mbl = map->id2bl(id)) == NULL || mbl->type != BL_MOB) {
		script_pushint(st, 0);
		return false;
	}

	md = (TBL_MOB *)mbl;
	md->bg_id = bg_id;

	mob_stop_attack(md);
	mob_stop_walking(md, 0);

	md->target_id = md->attacked_id = 0;

	clif->charnameack(0, &md->bl);

	script_pushint(st, 1);

	return true;
}

/**
 * Set immunity flag to a monster.
 * (making it immune to damage).
 * @param Monster ID
 * @param Flag (0 = Off | 1 = On)
 */
BUILDIN(hBG_monster_immunity)
{
	struct mob_data *md;
	struct block_list *mbl;
	struct hBG_mob_data *hBGmd;

	int id = script_getnum(st,2),
	flag = script_getnum(st,3);

	if (id == 0 || (mbl = map->id2bl(id)) == NULL || mbl->type != BL_MOB) {
		script_pushint(st, 0);
		return false;
	}

	md = (TBL_MOB *)mbl;

	if ((hBGmd = getFromMOBDATA(md, 0)) == NULL){
		CREATE(hBGmd, struct hBG_mob_data, 1);	
		addToMOBDATA(md, hBGmd, 0, true);
	}
	hBGmd->state.immunity = flag;

	return true;
}

/**
 * Leave a Battleground Team
 */
BUILDIN(hBG_leave)
{
	struct map_session_data *sd = script->rid2sd(st);

	if (sd == NULL || sd->bg_id == 0) {
		script_pushint(st, 0);
		return false;
	}

	hBG_team_leave(sd,0);

	script_pushint(st, 1);

	return true;
}

/**
 * Finalize battlegrounds and remove
 * teams from the db.
 */
BUILDIN(hBG_destroy)
{
	int bg_id = script_getnum(st, 2);

	if (bg_id <= 0 || bg->team_search(bg_id) == NULL) {
		ShowError("buildin_hBG_destroy: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	hBG_team_finalize(bg_id, true);

	script_pushint(st, 1);

	return true;
}

/**
 * Clean Battlegrounds without removing
 * the teams from the db.
 */
BUILDIN(hBG_clean)
{
	int bg_id = script_getnum(st, 2);

	if (bg_id <= 0 || bg->team_search(bg_id) == NULL) {
		ShowError("buildin_hBG_clean: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	hBG_team_finalize(bg_id, false);

	script_pushint(st, 1);

	return true;
}

/**
 * Get user count within an area in a map.
 * @param Battleground ID
 * @param Map Name
 * @param X1
 * @param Y1
 * @param X2
 * @param Y2
 */
BUILDIN(hBG_getareausers)
{
	struct battleground_data *bgd = NULL;
	struct map_session_data *sd = NULL;
	const char *map_name;
	int m, x0, y0, x1, y1, bg_id;
	int i = 0, c = 0;

	bg_id = script_getnum(st,2);
	map_name = script_getstr(st,3);

	if ((bgd = bg->team_search(bg_id)) == NULL || (m = map->mapname2mapid(map_name)) < 0) {
		script_pushint(st,0);
		return false;
	}

	x0 = script_getnum(st,4);
	y0 = script_getnum(st,5);
	x1 = script_getnum(st,6);
	y1 = script_getnum(st,7);

	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		if ((sd = bgd->members[i].sd) == NULL)
			continue;
		else if (sd->bl.m != m || sd->bl.x < x0 || sd->bl.y < y0 || sd->bl.x > x1 || sd->bl.y > y1)
			continue;
		c++;
	}

	script_pushint(st, c);
	
	return true;
}

/**
 * Update the score on a battleground map.
 * @param Map Name
 * @param Lion Score
 * @param Eagle Score
 */
BUILDIN(hBG_updatescore)
{
	const char *map_name;
	int m;

	map_name = script_getstr(st,2);

	if ((m = map->mapname2mapid(map_name)) < 0) {
		script_pushint(st, 0);
		return false;
	}

	map->list[m].bgscore_lion = script_getnum(st,3);
	map->list[m].bgscore_eagle = script_getnum(st,4);

	clif->bg_updatescore(m);

	script_pushint(st, 1);

	return true;
}

/**
 * Update Score for a Battleground Team.
 * @param Battleground ID
 * @param Score
 */
BUILDIN(hBG_update_score_team)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int bg_id = script_getnum(st,2);
	int score = script_getnum(st,3);

	if (bg_id <= 0) {
		ShowError("buildin_hBG_update_score_team: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	if ((bgd = bg->team_search(bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	hBGd->team_score = score;
	hBG_update_score_team(bgd);

	script_pushint(st, 1);

	return true;
}

/**
 * Get a Team's Guild Index
 * @param Battleground ID
 * @return Guild Index
 */
BUILDIN(hBG_get_team_gid)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int bg_id = script_getnum(st,2), guild_id = 0;

	if (bg_id <= 0) {
		ShowError("buildin_hBG_get_team_gid: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	if ((bgd = bg->team_search(bg_id)) != NULL
		&& (hBGd = getFromBGDATA(bgd, 0)) != NULL)
		guild_id = hBGd->g->guild_id;

	script_pushint(st, guild_id);

	return true;
}

/**
 * Get data from a Battleground
 * @param Battleground ID
 * @param Type
 *         0 = User Count
 *         1 = Fill $@bgmembers$ array with user list and return user count.
 *         2 = BG Guild Name
 *         3 = BG Guild Master Name
 *         4 = BG Color
 */
BUILDIN(hBG_get_data)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int bg_id = script_getnum(st,2);
	int type = script_getnum(st,3);

	if (bg_id <= 0) {
		ShowError("buildin_hBG_get_data: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	if ((bgd = bg->team_search(bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL) {
		script_pushint(st, 0);
		return false;
	}

	switch (type)
	{
		case 0:
			script_pushint(st, bgd->count);
			break;
		case 1: // Users and List
		{
			int i, j = 0;
			struct map_session_data *sd;
			for (i = 0; i < bgd->count; i++) {
				if ((sd = bgd->members[i].sd) == NULL)
					continue;
				mapreg->setregstr(reference_uid(script->add_str("$@bgmembers$"),j),sd->status.name);
				j++;
			}
			script_pushint(st, j);
		}
			break;
		case 2:
			script_pushconststr(st,hBGd->g ? hBGd->g->name : "");
			break;
		case 3:
			script_pushconststr(st,hBGd->g ? hBGd->g->master : "");
			break;
		case 4:
			script_pushint(st,hBGd->color);
			break;

		default:
			ShowError("script:bg_get_data: unknown data identifier %d\n", type);
			break;
	}

	script_pushint(st, 0);

	return true;
}

/**
 * Battleground Get Item
 * @param Battleground ID
 * @param Item ID
 * @param Item Amount
 */
BUILDIN(hBG_getitem)
{
	int bg_id, nameid, amount;

	bg_id = script_getnum(st,2);
	nameid = script_getnum(st,3);
	amount = script_getnum(st,4);


	if (bg_id <= 0 || bg->team_search(bg_id) == NULL) {
		ShowError("buildin_hBG_getitem: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	} else if (nameid <= 0 || itemdb->exists(nameid) == NULL) {
		ShowError("buildin_hBG_getitem: Invalid Item Id %d provided.\n", nameid);
		script_pushint(st, 0);
		return false;
	} else if (amount <= 0) {
		ShowError("buildin_hBG_getitem: Invalid Item amount %d provided.\n", amount);
		script_pushint(st, 0);
		return false;
	}

	hBG_team_getitem(bg_id, nameid, amount);

	script_pushint(st, amount);

	return true;
}

/**
 * Battleground Get Kafra Points
 * @param Battleground ID
 * @param Amount of KP
 */
BUILDIN(hBG_getkafrapoints)
{
	int bg_id, amount;

	bg_id = script_getnum(st, 2);
	amount = script_getnum(st, 3);

	if (bg_id <= 0 || bg->team_search(bg_id) == NULL) {
		ShowError("buildin_hBG_getkafrapoints: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	} else if (amount <= 0) {
		ShowError("buildin_hBG_getkafrapoints: Invalid Kafra Points %d provided.\n", amount);
		script_pushint(st, 0);
		return false;
	}

	hBG_team_get_kafrapoints(bg_id, amount);

	script_pushint(st, amount);

	return true;
}

/**
 * Battleground get Rewards
 * @param Battleground ID
 * @param Item ID
 * @param Item Amount
 * @param Kafra Points
 * @param Quest ID
 * @param Custom Variable (#KAFRAPOINTS/#CASHPOINTS etc..)
 * @param Custom Variable Add Value
 * @param Battleground Arena (0 EoS | 1 Boss | 2 TI | 3 CTF | 4 TD | 5 SC | 6 CON | 7 RUSH | 8 DOM)
 * @param Battleground Result (0 Won | 1 Tie | 2 Lost)
 */
BUILDIN(hBG_reward)
{
	int bg_id, nameid, amount, kafrapoints, quest_id, add_value, bg_arena, bg_result;
	const char *var;

	bg_id = script_getnum(st,2);
	nameid = script_getnum(st,3);
	amount = script_getnum(st,4);
	kafrapoints = script_getnum(st,5);
	quest_id = script_getnum(st,6);
	var = script_getstr(st,7);
	add_value = script_getnum(st,8);
	bg_arena = script_getnum(st,9);
	bg_result = script_getnum(st,10);

	if (bg_id <= 0 || bg->team_search(bg_id) == NULL) {
		ShowError("buildin_hBG_reward: Invalid BG Id %d provided.\n", bg_id);
		script_pushint(st, 0);
		return false;
	}

	if (nameid <= 0 || itemdb->exists(nameid) == NULL) {
		ShowError("buildin_hBG_reward: Invalid Item Id %d provided.\n", nameid);
		script_pushint(st, 0);
		return false;
	}

	if (nameid > 0 && amount <= 0) {
		ShowError("buildin_hBG_reward: Invalid Item amount %d provided.\n", nameid);
		script_pushint(st, 0);
		return false;
	}

	if (kafrapoints < 0) {
		ShowError("buildin_hBG_reward: Invalid Kafra Points %d provided.\n", kafrapoints);
		script_pushint(st, 0);
		return false;
	}

	if (quest_id < 0 || quest->db(quest_id) == NULL) {
		ShowError("buildin_hBG_reward: Invalid Quest ID %d provided.\n", quest_id);
		script_pushint(st, 0);
		return false;
	}

	if (bg_arena < 0) {
		ShowError("buildin_hBG_reward: Invalid BG Arena %d provided. \n", bg_arena);
		script_pushint(st, 0);
		return false;
	}

	if (bg_result < 0 || bg_result > 2) {
		ShowError("buildin_hBG_reward: Invalid BG result type %d provided. (types - 0 Won | 1 Tie | 2 Lost)", bg_result);
		script_pushint(st, 0);
		return false;
	}

	hBG_team_rewards(bg_id, nameid, amount, kafrapoints, quest_id, var, add_value, bg_arena, bg_result);

	script_pushint(st, 1);

	return true;
}

/**
 * Add Item to XY co-ordinates on Floor.
 * @param Map Name
 * @param Map X
 * @param Map Y
 * @param Item ID
 * @param Item Amount
 */
BUILDIN(hBG_flooritem2xy)
{
	int nameid, amount, m, x, y;
	const char *mapname;
	
	mapname = script_getstr(st,2);
	
	if ((m = map->mapname2mapid(mapname)) < 0) {
		// error message is thrown through mapindex->name2id()
		script_pushint(st, 0);
		return false;
	}
	
	x = script_getnum(st,3);
	y = script_getnum(st,4);
	nameid = script_getnum(st,5);
	
	if (itemdb->search(nameid) == NULL) {
		ShowError("buildin_hBG_flooritem2xy: Invalid Item Id %d provided.\n", nameid);
		script_pushint(st, 0);
		return false;
	}
	
	amount = script_getnum(st,6);
	
	if (amount < 1) {
		ShowError("buildin_hBG_flooritem2xy: Invalid Item amount %d provided.\n", amount);
		script_pushint(st, 0);
		return false;
	}
	
	hBG_addflooritem_area(NULL, m, x, y, nameid, amount);

	script_pushint(st, amount);

	return true;
}
/**
 * Warps BG Team to destination or Respawn Point
 * @param BG Team
 * @param Map Name
 * @param Map X
 * @param Map Y
 */
BUILDIN(hBG_warp)
{
	int x, y, mapindex, bg_id;
	const char* map_name;

	bg_id = script_getnum(st,2);
	map_name = script_getstr(st,3);
	if( !strcmp(map_name,"RespawnPoint") ) // Cementery Zone
		mapindex = 0;
	else if( (mapindex = script->mapindexname2id(st,map_name)) == 0 )
		return 0; // Invalid Map
	x = script_getnum(st,4);
	y = script_getnum(st,5);
	bg_id = hBG_team_warp(bg_id, mapindex, x, y);
	return true;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     Map Server Function Pre-Hooks
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * NPC Pre-Hooks
 */
void npc_parse_unknown_mapflag_pre(const char **name, const char **w3, const char **w4, const char **start, const char **buffer, const char **filepath, int **retval)
{
	int16 m = map->mapname2mapid(*name);

	if (strcmpi(*w3, "hBG_topscore") == 0) {
		struct hBG_mapflag *hBG_mf;

		if ((hBG_mf = getFromMAPD(&map->list[m], 0)) == NULL) {
			CREATE(hBG_mf, struct hBG_mapflag, 1);
			hBG_mf->hBG_topscore = 1;
			addToMAPD(&map->list[m], hBG_mf, 0, true);
		}

		hBG_mf->hBG_topscore = 1;

		hookStop();
	}

	return;
}

/**
 * Clif Pre-Hooks
 */
void clif_charnameupdate_pre(struct map_session_data **sd)
{
	unsigned char buf[103];
	int cmd = 0x195, ps;
	struct battleground_data *bgd = bg->team_search((*sd)->bg_id);
	struct hBG_data *hBGd;

	nullpo_retv((*sd));

	if ((*sd)->fakename[0] || bgd == NULL ||
		(hBGd = getFromBGDATA(bgd, 0)) == NULL || hBGd->g == NULL)
		return; //No need to update as the guild was not displayed anyway.

	WBUFW(buf,0) = cmd;
	WBUFL(buf,2) = (*sd)->bl.id;
	memcpy(WBUFP(buf,6), (*sd)->status.name, NAME_LENGTH);
	WBUFB(buf,30) = 0;
	memcpy(WBUFP(buf,54), hBGd->g->name, NAME_LENGTH);
	
	ps = (hBGd->leader_char_id == (*sd)->status.char_id)?0:1;
	memcpy(WBUFP(buf,78), hBGd->g->position[ps].name, NAME_LENGTH);

	// Update nearby clients
	clif->send(buf, 102, &(*sd)->bl, AREA);

	hookStop();
}
//Prevent update Guild Info if you're in BG
void clif_parse_GuildRequestInfo_pre(int *fd, struct map_session_data **sd)
{
	if ((*sd) && (*sd)->bg_id)
		hookStop();
	return;
}

/**
 * Skill Pre-Hooks.
 */
int skill_check_condition_castbegin_pre(struct map_session_data **sd, uint16 *skill_id, uint16 *skill_lv)
{
	nullpo_ret(sd);
	
	if (map->list[(*sd)->bl.m].flag.battleground && (*skill_id >= GD_SKILLBASE && *skill_id <= GD_DEVELOPMENT))
		hookStop(); // Prevent original function from running after return from here.
	
	return 1;
}

int skillnotok_pre(uint16 *skill_id, struct map_session_data **sd)
{
	int16 idx, m;
	nullpo_retr(1, *sd);
	m = (*sd)->bl.m;
	idx = skill->get_index(*skill_id);
 
	if (map->list[m].flag.battleground && (*skill_id >= GD_SKILLBASE && *skill_id <= GD_DEVELOPMENT)) {
	
		if (pc_has_permission(*sd, PC_PERM_DISABLE_SKILL_USAGE)) {
			hookStop();
			return 1;
		}

		if (pc_has_permission(*sd, PC_PERM_SKILL_UNCONDITIONAL)) {
			hookStop();
			return 0; // can do any damn thing they want
		}

		if ((*sd)->blockskill[idx]) {
			clif->skill_fail((*sd), *skill_id, USESKILL_FAIL_SKILLINTERVAL, 0);
			hookStop();
			return 1;
		}
		hookStop();
	}
	return 0;
}

/**
 * Skill cast end.
 * @param src = source block list.
 * @param bl = target block list
 */
int skill_castend_nodamage_id_pre(struct block_list **src, struct block_list **bl, uint16 *skill_id, uint16 *skill_lv, int64 *tick, int *flag)
{
	struct map_session_data *sd, *dstsd;
	struct status_change *tsc;
	struct status_data *sstatus;
	struct hBG_map_session_data *hBGsd = NULL;
	
	nullpo_retr(1, bl);
	nullpo_retr(1, src);

	if (!map->list[(*src)->m].flag.battleground || *skill_id != GD_EMERGENCYCALL)
		return 0;
	
	sd = BL_CAST(BL_PC, *src);
	dstsd = BL_CAST(BL_PC, *bl);

	if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return 0;

	tsc = status->get_sc(*bl);

	sstatus = status->get_status_data(*src);
	
	switch (*skill_id) {
		case GD_EMERGENCYCALL:
		{
			int dx[9]={-1, 1, 0, 0,-1, 1,-1, 1, 0};
			int dy[9]={ 0, 0, 1,-1, 1,-1,-1, 1, 0};
			int i = 0, j = 0;
			struct guild *g;
			
			// i don't know if it actually summons in a circle, but oh well. ;P
			if (sd && (g = hBG_get_guild(sd->bg_id)) != NULL) {
				clif->skill_nodamage(*src, *bl, *skill_id, *skill_lv, 1);
				
				for (i = 0; i < g->max_member; i++, j++) {
					if (j>8) j=0;
					if ((dstsd = g->member[i].sd) != NULL && sd != dstsd && !dstsd->state.autotrade && !pc_isdead(dstsd)) {
						if (map->getcell((*src)->m, *src, (*src)->x + dx[j], (*src)->y + dy[j], CELL_CHKNOREACH))
							dx[j] = dy[j] = 0;
						pc->setpos(dstsd, map_id2index((*src)->m), (*src)->x+dx[j], (*src)->y+dy[j], CLR_RESPAWN);
					}
				}
				guild->block_skill(sd, skill->get_time2(*skill_id, *skill_lv));
			}
		}
			break;
		case HLIF_HEAL:
		case AL_HEAL:
		{
			struct mob_data *dstmd = BL_UCAST(BL_MOB, *bl);
			int heal = skill->calc_heal(*src, *bl, *skill_id, *skill_lv, true);

			if (status->isimmune(*bl) || (dstmd && dstmd->class_ == MOBID_EMPELIUM))
				heal = 0;

			if (dstmd && mob_is_battleground(dstmd))
				heal = 1;

			if (sd && dstsd && sd->status.partner_id == dstsd->status.char_id && (sd->job&MAPID_UPPERMASK) == MAPID_SUPER_NOVICE && sd->status.sex == 0)
				heal = heal*2;

			if (tsc && tsc->count)
			{
				if (tsc->data[SC_KAITE] && !(sstatus->mode&MD_BOSS))
				{ //Bounce back heal
					if (src == bl)
						heal=0; //When you try to heal yourself under Kaite, the heal is voided.
					else {
						bl = src;
						dstsd = sd;
					}
				} else if (tsc->data[SC_BERSERK]) {
						heal = 0; //Needed so that it actually displays 0 when healing.
				}
			}

			if (sd && dstsd && heal > 0 && sd != dstsd)
			{
				if (map->list[(*src)->m].flag.battleground && sd->bg_id && dstsd->bg_id)
				{
					if (sd->bg_id == dstsd->bg_id)
						add2limit(hBGsd->stats.healing_done, heal, UINT_MAX);
					else
						add2limit(hBGsd->stats.wrong_healing_done, heal, UINT_MAX);
				}
			}
		}
			break;
	}
	
	hookStop();
	return 0;
}

/**
 * Status Pre-Hooks
 */
int status_get_guild_id_pre(const struct block_list **bl)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int bg_id;

	nullpo_ret((*bl));

	if ((*bl)->type == BL_PC
		&& (bg_id = bg->team_get_id((struct block_list *)*bl)) > 0
		&& (bgd = bg->team_search(bg_id)) != NULL
		&& (hBGd = getFromBGDATA(bgd, 0)) != NULL
		&& hBGd->g)
	{
		hookStop();
		return hBGd->g->guild_id;
	}

	return 0;
}

int status_get_emblem_id_pre(const struct block_list **bl)
{
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	int bg_id;
	
	nullpo_ret(bl);

	if ((*bl)->type == BL_PC
		&& (bg_id = bg->team_get_id((struct block_list *)(*bl))) > 0
		&& (bgd = bg->team_search(bg_id)) != NULL
		&& (hBGd = getFromBGDATA(bgd, 0)) != NULL && hBGd->g)
	{
		hookStop();
		return hBGd->g->emblem_id;
	}

	return 0;
}

/**
 * Battleground Pre-Hooks
 */
int bg_team_leave_pre(struct map_session_data **sd, enum bg_team_leave_type *flag)
{
	struct hBG_data *hBGd;
	struct battleground_data *bgd;
	
	nullpo_ret(sd);
	
	if ((bgd = bg->team_search((*sd)->bg_id)) != NULL
		&& (hBGd = getFromBGDATA(bgd, 0)) != NULL)
		hookStop();
	
	return 0;
}

/**
 * Guild Pre-Hooks
 */
// Check if guild is null and don't run BCT checks if true.
bool guild_isallied_pre(int *guild_id, int *guild_id2)
{
	struct guild *g = guild->search(*guild_id);
	
	if (g == NULL) {
		hookStop();
		return false;
	}
	
	return false;
}

/**
 * Unit Pre-Hooks
 */
int unit_free_pre(struct block_list **bl, clr_type *clrtype)
{
	nullpo_retr(0, (*bl));

	if ((*bl)->type == BL_PC) {
		struct map_session_data *sd = BL_UCAST(BL_PC, (*bl));
		struct hBG_queue_data *hBGqd = NULL;
		struct battleground_data *bgd = NULL;
		struct hBG_data *hBGd = NULL;
		struct hBG_map_session_data *hBGsd = NULL;

		if ((hBGqd = getFromMSD(sd, 0)) && hBG_queue_member_search(hBGqd, sd->bl.id))
			hBG_queue_member_remove(hBGqd, sd->bl.id);

		if (sd->bg_id != 0
			&& (bgd = bg->team_search(sd->bg_id)) != NULL
			&& (hBGd = getFromBGDATA(bgd, 0)) != NULL
			&& (hBGsd = getFromMSD(sd, 1)) != NULL) {
			//hBG_team_leave(sd, 0); // Now launched in map_quit
			hBGsd->stats.total_deserted++;
		}
	}

	return 0;
}
/**
 * Map Pre-Hooks
 */
int map_quit_pre(struct map_session_data **sd) {
	nullpo_ret(*sd);
	if( (*sd)->bg_id)
		hBG_team_leave(*sd, 0);
	return 0;
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     Map Server Function Post-Hooks
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Battle Post-Hooks
 */
void battle_consume_ammo(struct map_session_data *sd, int skill_id, int lv)
{
	int qty = 1;
	struct hBG_map_session_data *hBGsd = getFromMSD(sd, 1);

	if (battle->bc->arrow_decrement == 0 || hBGsd == NULL)
		return;

	if (skill)
		qty = max(1, skill->get_ammo_qty(skill_id, lv));

	if (sd->equip_index[EQI_AMMO] >= 0) {
		if (sd->bg_id && map->list[sd->bl.m].flag.battleground)
			add2limit(hBGsd->stats.ammo_used, qty, UINT_MAX);
	}
}
// Check target immunity
int battle_check_target_post( int retVal, struct block_list *src, struct block_list *target, int flag ) {

	if ( retVal == 1 && target->type == BL_MOB ) {
		struct hBG_mob_data *hBGmd;
		if (( hBGmd = getFromMOBDATA( (TBL_MOB*)target, 0 ) ))
		if ( hBGmd && hBGmd->state.immunity ){
			hookStop();
			return -1;
		}

        }

        return retVal;

}

/**
 * Clif Post-Hooks
 */
void clif_parse_LoadEndAck_post(int fd, struct map_session_data *sd)
{
	clif->charnameupdate(sd);
	return;
}
//Send charname_update every time you see someone in BG
void clif_getareachar_unit_post(struct map_session_data *sd, struct block_list *bl)
{
	if (bl->type == BL_PC) {
		struct map_session_data *tsd = BL_CAST(BL_PC, bl);
		clif->charnameupdate(tsd);
		return;
	}
}
void clif_parse_UseSkillToId_post(int fd, struct map_session_data *sd)
{
	uint16 skill_id;
	/* uint16 skill_lv; */
	int target_id;
	const struct s_packet_db *packet = clif->packet(RFIFOW(fd,0));
	struct battleground_data *bgd;
	struct hBG_data *hBGd;
	
	if (!sd->bg_id)
		return;
	else if ((bgd = bg->team_search(sd->bg_id)) == NULL || (hBGd = getFromBGDATA(bgd, 0)) == NULL)
		return;

	/* skill_lv = RFIFOW(fd,packet->pos[0]); */
	skill_id = RFIFOW(fd,packet->pos[1]);
	target_id = RFIFOL(fd,packet->pos[2]);
	
	if (skill_id >= GD_SKILLBASE && skill_id < GD_MAX && hBGd->leader_char_id == sd->status.char_id)
			unit->skilluse_id(&sd->bl, target_id, skill_id, guild->checkskill(hBG_get_guild(sd->bg_id), skill_id));
}

/**
 * Server tells 'sd' player client the abouts of 'dstsd' player
 */
void clif_getareachar_pc_post(struct map_session_data *sd,struct map_session_data *dstsd)
{
	if (sd->bg_id && dstsd->bg_id && sd->bg_id == dstsd->bg_id)
			clif->hpmeter_single(sd->fd, dstsd->bl.id, dstsd->battle_status.hp, dstsd->battle_status.max_hp);
}

/**
 * PC Post hooks
 */
void pc_update_idle_time_post(struct map_session_data* sd, enum e_battle_config_idletime type)
{
	struct hBG_map_session_data *hBGsd = NULL;
	struct battleground_data *bgd = bg->team_search(sd->bg_id);
	struct hBG_data *hBGd = NULL;
	
	nullpo_retv(sd);
	
	if (bgd && (hBGd = getFromBGDATA(bgd, 0)) && (hBGsd = getFromMSD(sd, 1)) && hBGsd->state.afk) {
		char output[256];
		/* Reset AFK status */
		sprintf(output, "%s : %s is no longer AFK.", sd->status.name, hBGd->g->name);
		hBG_send_chat_message(bgd, sd->bl.id, sd->status.name, output, (int)strlen(output) + 1);
		hBGsd->state.afk = 0;
	}
}

bool pc_authok_post(bool ret, struct map_session_data *sd, int login_id2, time_t expiration_time, int group_id, const struct mmo_charstatus *st, bool changing_mapservers)
{
	if (sd) {
		WFIFOHEAD(chrif->fd, 14);
		WFIFOW(chrif->fd, 0) = PACKET_INTER_BG_STATS_REQ;
		WFIFOL(chrif->fd, 2) = sd->status.account_id;
		WFIFOL(chrif->fd, 6) = sd->status.char_id;
		WFIFOL(chrif->fd, 10) = sd->fd;
		WFIFOSET(chrif->fd, 14);
	}
	
	return ret;
}

/**
 * Character Interface Post-Hooks
 */
/**
 * Requests saving of hBG Statistics and sends data to char-server.
 *
 * @param sd pointer to map session data.
 * @param flag as an indicator to tell char-server if character is quitting.
 * @return boolean.
 */
bool chrif_save_post(bool ret, struct map_session_data *sd, int flag)
{
	struct hBG_map_session_data *hBGsd = NULL;
	int len = 13 + sizeof(struct hBG_stats);
	
	nullpo_retr(false, sd);
	
	if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return ret;
	
	WFIFOHEAD(chrif->fd, len);
	WFIFOW(chrif->fd, 0) = PACKET_INTER_BG_STATS_SAVE; // 0x9000 Packet ID
	WFIFOL(chrif->fd, 2) = sd->status.account_id; // Account Id
	WFIFOL(chrif->fd, 6) = sd->status.char_id; // Char Id
	WFIFOB(chrif->fd, 12) = (flag==1); //Flag to tell char-server this character is quitting.
	memcpy(WFIFOP(chrif->fd, 13), &hBGsd->stats, sizeof(struct hBG_stats)); // hBG statistics.
	WFIFOSET(chrif->fd, len);
	
	return ret;
}

/**
 * Status Post Hooks
 */
int status_damage_post(int ret, struct block_list *src, struct block_list *target,int64 in_hp, int64 in_sp, int walkdelay, int flag)
{
	struct map_session_data *sd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;

	nullpo_retr(ret, target);

	if (src == NULL)
		return ret;

	if (src->type != BL_PC || (sd = BL_UCAST(BL_PC, src)) == NULL)
		return ret;
	if (map->list[src->m].flag.battleground == 0)
		return ret;
	if ((hBGsd = getFromMSD(sd, 1)) == NULL)
		return ret;

	hBG_record_damage(src, target, (int) in_hp);

	return ret;
}

/**
 * Receives and allocates map session data with bg statistics
 * from char-server.
 *
 * @param fd as socket descriptor handle
 */
void hBG_statistics_parsefromchar(int fd)
{
	struct map_session_data *sd = NULL;
	struct hBG_map_session_data *hBGsd = NULL;
	struct hBG_stats *stats = NULL;
	/* int account_id = RFIFOL(fd, 2), char_id = RFIFOL(fd, 6); */
 	int char_fd = RFIFOL(fd,10);
	
	nullpo_retv(sockt->session[char_fd]);

	if ((sd = sockt->session[char_fd]->session_data) == NULL)
		return;

	if ((hBGsd = getFromMSD(sd, 1)) == NULL) {
		CREATE(hBGsd, struct hBG_map_session_data, 1);
		addToMSD(sd, hBGsd, 1, false);
	}
	
	if ((stats = getFromSession(sockt->session[fd], 0)) == NULL)
		memcpy(&hBGsd->stats, RFIFOP(fd, 14), sizeof(struct hBG_stats));
	else
		memcpy(&hBGsd->stats, stats, sizeof(struct hBG_stats));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     Char Server Functions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * Character Server Saving of hBG Statistics
 *
 * @param fd socket descriptor handle
 */
void char_bgstats_tosql(int fd)
{
	struct hBG_stats pstats = {0}, *stats = NULL;
	int account_id = 0, char_id = 0;
	/* int flag = 0; */
	
	nullpo_retv(sockt->session[fd]);
	
	account_id = RFIFOL(fd, 2);
	char_id = RFIFOL(fd, 6);
	/* flag = RFIFOB(fd, 12); */
	
	if ((stats = getFromSession(sockt->session[fd], 0)) == NULL) {
		CREATE(stats, struct hBG_stats, 1);
		memset(stats, 0, sizeof(struct hBG_stats));
		addToSession(sockt->session[fd], stats, 0, true);
	}
	
	memcpy(&pstats, RFIFOP(fd, 13), sizeof(struct hBG_stats));
	
	if (memcmp(stats, &pstats, sizeof(struct hBG_stats))) {
		if (SQL_ERROR == SQL->Query(inter->sql_handle,
			"REPLACE INTO `char_bg_stats` ("
			"`char_id`, "
			"`best_damage`, `total_damage_done`, `total_damage_received`, "
			"`ti_wins`, `ti_lost`, `ti_tie`, "
			"`eos_flags`, `eos_bases`, `eos_wins`, `eos_lost`, `eos_tie`, "
			"`boss_killed`, `boss_damage`, `boss_flags`, `boss_wins`, `boss_lost`, `boss_tie`, "
			"`dom_bases`, `dom_off_kills`, `dom_def_kills`, `dom_wins`, `dom_lost`, `dom_tie`, "
			"`td_kills`, `td_deaths`, `td_wins`, `td_lost`, `td_tie`, "
			"`sc_stolen`, `sc_captured`, `sc_dropped`, `sc_wins`, `sc_lost`, `sc_tie`, "
			"`ctf_taken`, `ctf_captured`, `ctf_dropped`, `ctf_wins`, `ctf_lost`, `ctf_tie`, "
			"`emperium_kills`, `barricade_kills`, `guardian_stone_kills`, `conquest_wins`, `conquest_losses`, "
			"`ru_captures`, `ru_wins`, `ru_lost`, `ru_skulls`,"
			"`kill_count`, `death_count`, `wins`, `losses`, `ties`, `wins_as_leader`, `losses_as_leader`, `ties_as_leader`, `total_deserted`, `score`, `points`, `ranked_points`, `ranked_games`,"
			"`sp_heal_potions`, `hp_heal_potions`, `yellow_gemstones`, `red_gemstones`, `blue_gemstones`, `poison_bottles`, `acid_demostration`, `acid_demostration_fail`, "
			"`support_skills_used`, `healing_done`, `wrong_support_skills_used`, `wrong_healing_done`, "
			"`sp_used`, `zeny_used`, `spiritb_used`, `ammo_used`)"
			" VALUES "
			"('%d',"
			"'%d','%u','%u',"
			"'%d','%d','%d','%d',"
			"'%d','%d','%d','%d','%d',"
			"'%u','%d','%d','%d','%d','%d',"
			"'%d','%d','%d','%d','%d','%d',"
			"'%d','%d','%d','%d','%d',"
			"'%d','%d','%d','%d','%d','%d',"
			"'%d','%d','%d','%d','%d','%d',"
			"'%d','%d','%d','%d','%d',"
			"'%d','%d','%d',"
			"'%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d',"
			"'%u','%u','%u','%u','%u','%u','%u','%u',"
			"'%u','%u','%u','%u',"
			"'%u','%u','%u','%u')",
			char_id,
			pstats.best_damage, pstats.total_damage_done, pstats.total_damage_received,
			pstats.ti_wins,pstats.ti_lost,pstats.ti_tie,
			pstats.eos_flags,pstats.eos_bases,pstats.eos_wins,pstats.eos_lost,pstats.eos_tie,
			pstats.boss_killed,pstats.boss_damage,pstats.boss_flags,pstats.boss_wins,pstats.boss_lost,pstats.boss_tie,
			pstats.dom_bases,pstats.dom_off_kills,pstats.dom_def_kills,pstats.dom_wins,pstats.dom_lost,pstats.dom_tie,
			pstats.td_kills,pstats.td_deaths,pstats.td_wins,pstats.td_lost,pstats.td_tie,
			pstats.sc_stolen,pstats.sc_captured,pstats.sc_dropped,pstats.sc_wins,pstats.sc_lost,pstats.sc_tie,
			pstats.ctf_taken,pstats.ctf_captured,pstats.ctf_dropped,pstats.ctf_wins,pstats.ctf_lost,pstats.ctf_tie,
			pstats.emperium_kills,pstats.barricade_kills,pstats.guardian_stone_kills,pstats.conquest_wins,pstats.conquest_losses,
			pstats.ru_captures,pstats.ru_wins,pstats.ru_lost, pstats.ru_skulls,
			pstats.kill_count,pstats.death_count,pstats.wins,pstats.losses,pstats.ties,pstats.wins_as_leader,pstats.losses_as_leader,
			pstats.ties_as_leader,pstats.total_deserted,pstats.score,pstats.points,pstats.ranked_points,pstats.ranked_games,
			pstats.sp_heal_potions, pstats.hp_heal_potions, pstats.yellow_gemstones, pstats.red_gemstones,
			pstats.blue_gemstones, pstats.poison_bottles, pstats.acid_demostration, pstats.acid_demostration_fail,
			pstats.support_skills_used, pstats.healing_done, pstats.wrong_support_skills_used, pstats.wrong_healing_done,
			pstats.sp_used, pstats.zeny_used, pstats.spiritb_used, pstats.ammo_used))
		{
			Sql_ShowDebug(inter->sql_handle);
		} else {
			memcpy(stats, &pstats, sizeof(struct hBG_stats));
			ShowInfo("Saved char (AID/CID: %d/%d) - BG Statistics [by Smokexyz].\n", account_id, char_id);
		}
	}
}

void char_bgstats_fromsql(int fd)
{
	struct hBG_stats temp_stats = { 0 }, *stats = NULL;
	int account_id = 0, char_id = 0, char_fd = 0, len = 0;
	struct SqlStmt *stmt = SQL->StmtMalloc(inter->sql_handle);

	if (stmt == NULL) {
		SqlStmt_ShowDebug(stmt);
		return;
	}
	
	account_id = RFIFOL(fd,2);
	char_id = RFIFOL(fd, 6);
	char_fd = RFIFOL(fd, 10);
	
	if (SQL_ERROR == SQL->StmtPrepare(stmt, "SELECT "
		"`best_damage`,`total_damage_done`,`total_damage_received`,`ru_skulls`,`ti_wins`,`ti_lost`,`ti_tie`,`eos_flags`,`eos_bases`,`eos_wins`," // 0-9
		"`eos_lost`,`eos_tie`,`boss_killed`,`boss_damage`,`boss_flags`,`boss_wins`,`boss_lost`,`boss_tie`,`td_kills`,`td_deaths`," //10-19
		"`td_wins`,`td_lost`,`td_tie`,`sc_stolen`,`sc_captured`,`sc_dropped`,`sc_wins`,`sc_lost`,`sc_tie`,`ctf_taken`," //20-29
		"`ctf_captured`,`ctf_dropped`,`ctf_wins`,`ctf_lost`,`ctf_tie`,`emperium_kills`,`barricade_kills`,`guardian_stone_kills`,`conquest_wins`,`conquest_losses`,"//30-39
		"`kill_count`,`death_count`,`wins`,`losses`,`ties`,`wins_as_leader`,`losses_as_leader`,`ties_as_leader`,`total_deserted`,`score`,"//40-49
		"`points`,`sp_heal_potions`,`hp_heal_potions`,`yellow_gemstones`,`red_gemstones`,`blue_gemstones`,`poison_bottles`,`acid_demostration`,`acid_demostration_fail`,`support_skills_used`,"//50-59
		"`healing_done`,`wrong_support_skills_used`,`wrong_healing_done`,`sp_used`,`zeny_used`,`spiritb_used`,`ammo_used`,`ranked_points`,`ranked_games`,`ru_wins`,"//60-69
		"`ru_lost`,`ru_captures`,`dom_bases`,`dom_off_kills`,`dom_def_kills`,`dom_wins`,`dom_lost`,`dom_tie`"//70-79
		" FROM `char_bg_stats` WHERE `char_id` = ?")
		|| SQL_ERROR == SQL->StmtBindParam(stmt, 0, SQLDT_INT, &char_id, 0)
		|| SQL_ERROR == SQL->StmtExecute(stmt)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  0, SQLDT_UINT,   &temp_stats.best_damage, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  1, SQLDT_UINT,   &temp_stats.total_damage_done, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  2, SQLDT_UINT,   &temp_stats.total_damage_received, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  3, SQLDT_USHORT, &temp_stats.ru_skulls, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  4, SQLDT_USHORT, &temp_stats.ti_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  5, SQLDT_USHORT, &temp_stats.ti_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  6, SQLDT_USHORT, &temp_stats.ti_tie, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  7, SQLDT_USHORT, &temp_stats.eos_flags, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  8, SQLDT_USHORT, &temp_stats.eos_bases, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt,  9, SQLDT_USHORT, &temp_stats.eos_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 10, SQLDT_USHORT, &temp_stats.eos_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 11, SQLDT_USHORT, &temp_stats.eos_tie, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 12, SQLDT_USHORT, &temp_stats.boss_killed, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 13, SQLDT_UINT,   &temp_stats.boss_damage, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 14, SQLDT_USHORT, &temp_stats.boss_flags, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 15, SQLDT_USHORT, &temp_stats.boss_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 16, SQLDT_USHORT, &temp_stats.boss_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 17, SQLDT_USHORT, &temp_stats.boss_tie, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 18, SQLDT_USHORT, &temp_stats.td_kills, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 19, SQLDT_USHORT, &temp_stats.td_deaths, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 20, SQLDT_USHORT, &temp_stats.td_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 21, SQLDT_USHORT, &temp_stats.td_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 22, SQLDT_USHORT, &temp_stats.td_tie, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 23, SQLDT_USHORT, &temp_stats.sc_stolen, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 24, SQLDT_USHORT, &temp_stats.sc_captured, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 25, SQLDT_USHORT, &temp_stats.sc_dropped, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 26, SQLDT_USHORT, &temp_stats.sc_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 27, SQLDT_USHORT, &temp_stats.sc_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 28, SQLDT_USHORT, &temp_stats.sc_tie, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 29, SQLDT_USHORT, &temp_stats.ctf_taken, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 30, SQLDT_USHORT, &temp_stats.ctf_captured, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 31, SQLDT_USHORT, &temp_stats.ctf_dropped, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 32, SQLDT_USHORT, &temp_stats.ctf_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 33, SQLDT_USHORT, &temp_stats.ctf_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 34, SQLDT_USHORT, &temp_stats.ctf_tie, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 35, SQLDT_USHORT, &temp_stats.emperium_kills, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 36, SQLDT_USHORT, &temp_stats.barricade_kills, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 37, SQLDT_USHORT, &temp_stats.guardian_stone_kills, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 38, SQLDT_USHORT, &temp_stats.conquest_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 39, SQLDT_USHORT, &temp_stats.conquest_losses, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 40, SQLDT_USHORT, &temp_stats.kill_count, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 41, SQLDT_USHORT, &temp_stats.death_count, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 42, SQLDT_USHORT, &temp_stats.wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 43, SQLDT_USHORT, &temp_stats.losses, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 44, SQLDT_USHORT, &temp_stats.ties, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 45, SQLDT_USHORT, &temp_stats.wins_as_leader, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 46, SQLDT_USHORT, &temp_stats.losses_as_leader, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 47, SQLDT_USHORT, &temp_stats.ties_as_leader, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 48, SQLDT_USHORT, &temp_stats.total_deserted, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 49, SQLDT_USHORT, &temp_stats.score, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 50, SQLDT_USHORT, &temp_stats.points, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 51, SQLDT_UINT,   &temp_stats.sp_heal_potions, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 52, SQLDT_UINT,   &temp_stats.hp_heal_potions, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 53, SQLDT_UINT,   &temp_stats.yellow_gemstones, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 54, SQLDT_UINT,   &temp_stats.red_gemstones, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 55, SQLDT_UINT,   &temp_stats.blue_gemstones, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 56, SQLDT_UINT,   &temp_stats.poison_bottles, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 57, SQLDT_UINT,   &temp_stats.acid_demostration, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 58, SQLDT_UINT,   &temp_stats.acid_demostration_fail, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 59, SQLDT_UINT,   &temp_stats.support_skills_used, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 60, SQLDT_UINT,   &temp_stats.healing_done, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 61, SQLDT_UINT,   &temp_stats.wrong_support_skills_used, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 62, SQLDT_UINT,   &temp_stats.wrong_healing_done, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 63, SQLDT_UINT,   &temp_stats.sp_used, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 64, SQLDT_UINT,   &temp_stats.zeny_used, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 65, SQLDT_UINT,   &temp_stats.spiritb_used, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 66, SQLDT_UINT,   &temp_stats.ammo_used, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 67, SQLDT_USHORT, &temp_stats.ranked_points, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 68, SQLDT_USHORT, &temp_stats.ranked_games, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 69, SQLDT_USHORT, &temp_stats.ru_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 70, SQLDT_USHORT, &temp_stats.ru_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 71, SQLDT_USHORT, &temp_stats.ru_captures, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 72, SQLDT_USHORT, &temp_stats.dom_bases, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 73, SQLDT_USHORT, &temp_stats.dom_off_kills, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 74, SQLDT_USHORT, &temp_stats.dom_def_kills, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 75, SQLDT_USHORT, &temp_stats.dom_wins, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 76, SQLDT_USHORT, &temp_stats.dom_lost, 0, NULL, NULL)
		|| SQL_ERROR == SQL->StmtBindColumn(stmt, 77, SQLDT_USHORT, &temp_stats.dom_tie, 0, NULL, NULL)
		|| SQL_SUCCESS != SQL->StmtNextRow(stmt))
	{
		temp_stats.score = 2000;
	}
	
	ShowInfo("Loaded char (AID/CID: %d/%d) - BG Statistics [by Smokexyz]\n", account_id, char_id);
	
	if ((stats = getFromSession(sockt->session[fd], 0)) == NULL) {
		CREATE(stats, struct hBG_stats, 1);
		memcpy(stats, &temp_stats, sizeof(sizeof(struct hBG_stats)));
		addToSession(sockt->session[fd], stats, 0, true);
	} else if (memcmp(stats, &temp_stats, sizeof(struct hBG_stats))) {
		memcpy(stats, &temp_stats, sizeof(struct hBG_stats));
	}
	
	len = 14 + sizeof(struct hBG_stats);
	WFIFOHEAD(fd, len);
	WFIFOW(fd, 0) = PACKET_MAP_BG_STATS_GET;
	WFIFOL(fd, 2) = account_id;
	WFIFOL(fd, 6) = char_id;
	WFIFOL(fd, 10) = char_fd;
	memcpy(WFIFOP(fd, 14), &temp_stats, sizeof(struct hBG_stats));
	WFIFOSET(fd, len);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                     Battle Configuration Parsing                    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void hBG_config_read(const char *key, const char *val)
{
	int value = config_switch (val);

	if (strcmpi(key,"battle_configuration/hBG_enabled") == 0) {
		if (value < 0 || value > 1) {
			ShowWarning("Received Invalid Setting %d for hBG_enabled, defaulting to 0.\n", value);
			return;
		}
		hBG_enabled = value;
	} else if (strcmpi(key, "battle_configuration/hBG_from_town_only") == 0) {
		if (value < 0 || value > 1) {
			ShowWarning("Received Invalid Setting %d for hBG_from_town_only, defaulting to 0.\n", value);
			return;
		}
		hBG_from_town_only = value;
	} else if (strcmpi(key, "battle_configuration/hBG_ip_check") == 0) {
		if (value < 0) {
			ShowWarning("Received Invalid Setting %d for hBG_ip_check, defaulting to 0.\n", value);
			return;
		}
		hBG_ip_check = value;
	} else if (strcmpi(key, "battle_configuration/hBG_idle_announce") == 0) {
		if (value < 0) {
			ShowWarning("Received Invalid Setting %d for hBG_idle_announce, defaulting to 60 seconds.\n", value);
			hBG_idle_announce = 60;
		} else {
			hBG_idle_announce = value;
		}
	} else if (strcmpi(key, "battle_configuration/hBG_idle_autokick") == 0) {
		if (value < 0) {
			ShowWarning("Received Invalid Setting %d for hBG_idle_autokick, defaulting to 5 minutes (300s).\n", value);
			hBG_idle_autokick = 300;
		} else {
			hBG_idle_autokick = value;
		}
	} else if (strcmpi(key, "battle_configuration/hBG_reportafk_leaderonly") == 0) {
		if (value < 0 || value > 1) {
			ShowWarning("Received Invalid Setting %d for hBG_reportafk_leaderonly, defaulting to 0.\n", value);
			hBG_reportafk_leaderonly = 0;
		} else {
			hBG_reportafk_leaderonly = value;
		}
	} else if (strcmpi(key, "battle_configuration/hBG_balanced_queue") == 0) {
		if (value < 0 || value > 1) {
			ShowWarning("Received Invalid Setting %d for hBG_balanced_queue, defaulting to 0.\n", value);
			return;
		}
		hBG_balanced_queue = value;
	} else if (strcmpi(key, "battle_configuration/hBG_reward_rates") == 0) {
		if (value < 0) {
			ShowWarning("Received invalid setting %d for hBG_reward_rates, defaulting to 100.\n", value);
			hBG_reward_rates = 100;
		} else {
			hBG_reward_rates = value;
		}
	} else if (strcmpi(key, "battle_configuration/hBG_xy_interval") == 0) {
		if (value < 1000) {
			ShowWarning("Received Invalid Setting %d for hBG_xy_interval. (min: %d, max: %d) Defaulting to 1000ms. \n", value, 1000, INT_MAX);
			hBG_xy_interval = 1000;
		} else {
			hBG_xy_interval = value;
		}
	} else if (strcmpi(key, "battle_configuration/hBG_ranked_mode") == 0) {
		if (value < 0 || value > 1) {
			ShowWarning("Received Invalid Setting %d for hBG_ranked_mode, defaulting to 0.\n", value);
			hBG_ranked_mode = 0;
		} else {
			hBG_ranked_mode = value;
		}
	} else if (strcmpi(key, "battle_configuration/hBG_leader_change") == 0) {
		if (value < 0 || value > 1) {
			ShowWarning("Received Invalid Setting %d for hBG_leader_change, defaulting to 0.\n", value);
			hBG_leader_change = 0;
		} else {
			hBG_leader_change = value;
		}
	}
}

int hBG_config_get(const char *key)
{
	if (strcmpi(key, "battle_configuration/hBG_enabled") == 0)
		return hBG_enabled;
	else if (strcmpi(key, "battle_configuration/hBG_from_town_only") == 0)
		return hBG_from_town_only;
	else if (strcmpi(key, "battle_configuration/hBG_ip_check") == 0)
		return hBG_ip_check;
	else if (strcmpi(key, "battle_configuration/hBG_idle_announce") == 0)
		return hBG_idle_announce;
	else if (strcmpi(key, "battle_configuration/hBG_idle_autokick") == 0)
		return hBG_idle_autokick;
	else if (strcmpi(key, "battle_configuration/hBG_reportafk_leaderonly") == 0)
		return hBG_reportafk_leaderonly;
	else if (strcmpi(key, "battle_configuration/hBG_balanced_queue") == 0)
		return hBG_balanced_queue;
	else if (strcmpi(key, "battle_configuration/hBG_reward_rates") == 0)
		return hBG_reward_rates;
	else if (strcmpi(key, "battle_configuration/hBG_xy_interval") == 0)
		return hBG_xy_interval;
	else if (strcmpi(key, "battle_configuration/hBG_ranked_mode") == 0)
		return hBG_ranked_mode;
	else if (strcmpi(key, "battle_configuration/hBG_leader_change") == 0)
		return hBG_leader_change;

	return 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                       Plugin Handling
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* run when server starts */
HPExport void plugin_init(void)
{
	int interval = hBG_config_get("battle_configuration/hBG_xy_interval");

	if (SERVER_TYPE == SERVER_TYPE_CHAR) {
		addPacket(PACKET_INTER_BG_STATS_REQ, 14, char_bgstats_fromsql, hpParse_FromMap);
		addPacket(PACKET_INTER_BG_STATS_SAVE, 13 + sizeof(struct hBG_stats), char_bgstats_tosql, hpParse_FromMap);
	}
	
	if (SERVER_TYPE == SERVER_TYPE_MAP) {
		addPacket(PACKET_MAP_BG_STATS_GET, 14+sizeof(struct hBG_stats), hBG_statistics_parsefromchar, hpChrif_Parse);
		
		/* Function Pre-Hooks */
		addHookPre(npc, parse_unknown_mapflag, npc_parse_unknown_mapflag_pre);
		addHookPre(clif, charnameupdate, clif_charnameupdate_pre);
		addHookPre(clif, pGuildRequestInfo,clif_parse_GuildRequestInfo_pre);
		addHookPre(status, get_guild_id, status_get_guild_id_pre);
		addHookPre(status, get_emblem_id, status_get_emblem_id_pre);
		addHookPre(guild, isallied, guild_isallied_pre);
		addHookPre(skill, check_condition_castbegin, skill_check_condition_castbegin_pre);
		addHookPre(skill, not_ok, skillnotok_pre);
		addHookPre(skill, castend_nodamage_id, skill_castend_nodamage_id_pre);
		addHookPre(bg, team_leave, bg_team_leave_pre);
		addHookPre(unit, free, unit_free_pre);
		addHookPre(map, quit, map_quit_pre);
		
		/* Function Post-Hooks */
		addHookPost(clif, pLoadEndAck, clif_parse_LoadEndAck_post);
		addHookPost(clif, pUseSkillToId, clif_parse_UseSkillToId_post);
		addHookPost(clif, getareachar_pc, clif_getareachar_pc_post);
		addHookPost(clif, getareachar_unit, clif_getareachar_unit_post);
		addHookPost(pc, update_idle_time, pc_update_idle_time_post);
		addHookPost(pc, authok, pc_authok_post);
		addHookPost(chrif, save, chrif_save_post);
		addHookPost(status, damage, status_damage_post);
		
		addHookPost( battle,check_target, battle_check_target_post );
		
		/* @Commands */
		addAtcommand("bgrank", bgrank);
		addAtcommand("reportafk", reportafk);
		addAtcommand("leader", leader);
		
		/* Script Commands */
		addScriptCommand("hBG_team_create","siiiss", hBG_team_create);
		addScriptCommand("hBG_queue_create","ss?", hBG_queue_create);
		addScriptCommand("hBG_queue_event","is", hBG_queue_event);
		addScriptCommand("hBG_queue_join","i", hBG_queue_join);
		addScriptCommand("hBG_queue_partyjoin","ii", hBG_queue_partyjoin);
		addScriptCommand("hBG_queue_leave","i", hBG_queue_leave);
		addScriptCommand("hBG_queue_data","ii", hBG_queue_data);
		addScriptCommand("hBG_queue2team","iisiiiss", hBG_queue2team);
		addScriptCommand("hBG_queue2team_single","iisii", hBG_queue2team_single);
		addScriptCommand("hBG_queue2teams","iiiiii*", hBG_queue2teams);
		addScriptCommand("hBG_queue_checkstart","iiii", hBG_queue_checkstart);
		addScriptCommand("hBG_balance_teams","iiiii*", hBG_balance_teams);
		addScriptCommand("hBG_waitingroom2bg","siiiss", hBG_waitingroom2bg);
		addScriptCommand("hBG_waitingroom2bg_single","isiis", hBG_waitingroom2bg_single);
		addScriptCommand("hBG_team_setxy","iii", hBG_team_setxy);
		addScriptCommand("hBG_team_reveal","i", hBG_team_reveal);
		addScriptCommand("hBG_team_conceal","i", hBG_team_conceal);
		addScriptCommand("hBG_team_setquest","ii", hBG_team_setquest);
		addScriptCommand("hBG_viewpointmap","siiiii", hBG_viewpointmap);
		addScriptCommand("hBG_monster_reveal","iii", hBG_monster_reveal);
		addScriptCommand("hBG_monster_set_team","ii", hBG_monster_set_team);
		addScriptCommand("hBG_monster_immunity","ii", hBG_monster_immunity);
		addScriptCommand("hBG_leave","", hBG_leave);
		addScriptCommand("hBG_destroy","i", hBG_destroy);
		addScriptCommand("hBG_clean","i", hBG_clean);
		addScriptCommand("hBG_get_data","ii", hBG_get_data);
		addScriptCommand("hBG_getareausers","isiiii", hBG_getareausers);
		addScriptCommand("hBG_updatescore","sii", hBG_updatescore);
		addScriptCommand("hBG_team_updatescore", "ii", hBG_update_score_team);
		addScriptCommand("hBG_team_guildid","i", hBG_get_team_gid);
		addScriptCommand("hBG_getitem","iii", hBG_getitem);
		addScriptCommand("hBG_getkafrapoints","ii", hBG_getkafrapoints);
		addScriptCommand("hBG_reward","iiiiisiii", hBG_reward);
		addScriptCommand("hBG_flooritem2xy", "siiii", hBG_flooritem2xy);
		addScriptCommand("hBG_warp", "isii", hBG_warp);
		
		hBG_queue_db = idb_alloc(DB_OPT_RELEASE_DATA);
		timer->add_func_list(hBG_send_xy_timer, "hBG_send_xy_timer");
		timer->add_interval(timer->gettick() + interval , hBG_send_xy_timer, 0, 0, interval);
	}
}

/* triggered when server starts loading, before any server-specific data is set */
HPExport void server_preinit(void)
{
	if (SERVER_TYPE == SERVER_TYPE_MAP) {
		addBattleConf("battle_configuration/hBG_enabled", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_from_town_only", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_ip_check", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_idle_announce", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_idle_autokick", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_balanced_queue", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_reward_rates", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_xy_interval", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_ranked_mode", hBG_config_read, hBG_config_get, true);
		addBattleConf("battle_configuration/hBG_leader_change", hBG_config_read, hBG_config_get, true);
	}
}

/* run when server is ready (online) */
HPExport void server_online(void)
{
	if (SERVER_TYPE == SERVER_TYPE_MAP) {
		hBG_build_guild_data();
		ShowStatus("%s v%s has been initialized. [by Smokexyz]\n", pinfo.name, pinfo.version);
	}
}

static int queue_db_final(union DBKey key, struct DBData *data, va_list ap)
{
	struct hBG_queue_data *hBGqd = DB->data2ptr(data);
	
	if (hBGqd)
		hBG_queue_members_finalize(hBGqd); // Unlink all queue members
	
	return 0;
}

/* run when server is shutting down */
HPExport void plugin_final(void)
{
	if (SERVER_TYPE == SERVER_TYPE_MAP) {
		hBG_queue_db->destroy(hBG_queue_db, queue_db_final);
		ShowInfo ("%s v%s has been finalized. [by Smokexyz]\n", pinfo.name, pinfo.version);
	}
}
