-- This file is part of a Hercules Plugin library.
-- http://herc.ws - http://github.com/HerculesWS/Hercules
--  __                 _
-- / _\_ __ ___   ___ | | _______  ___   _ ____
-- \ \| '_ ` _ \ / _ \| |/ / _ \ \/ / | | |_  /
-- _\ \ | | | | | (_) |   <  __/>  <| |_| |/ /
-- \__/_| |_| |_|\___/|_|\_\___/_/\_\\__, /___|
--                                   |___/
--       Copyright (c) 2017 Smokexyz.
--          All rights reserved.
--
-- Hercules Battlegrounds

-- 
--
-- Table structure for table `char_bg_stats`
--

CREATE TABLE IF NOT EXISTS `char_bg_stats` (
  `char_id` INT(11) NOT NULL,
  `best_damage` INT(11) NOT NULL DEFAULT '0',
  `total_damage_done` INT(11) NOT NULL DEFAULT '0',
  `total_damage_received` INT(11) NOT NULL DEFAULT '0',
  `ti_wins` INT(11) NOT NULL DEFAULT '0',
  `ti_lost` INT(11) NOT NULL DEFAULT '0',
  `ti_tie` INT(11) NOT NULL DEFAULT '0',
  `eos_flags` INT(11) NOT NULL DEFAULT '0',
  `eos_bases` INT(11) NOT NULL DEFAULT '0',
  `eos_wins` INT(11) NOT NULL DEFAULT '0',
  `eos_lost` INT(11) NOT NULL DEFAULT '0',
  `eos_tie` INT(11) NOT NULL DEFAULT '0',
  `boss_killed` INT(11) NOT NULL DEFAULT '0',
  `boss_damage` INT(11) NOT NULL DEFAULT '0',
  `boss_flags` INT(11) NOT NULL DEFAULT '0',
  `boss_wins` INT(11) NOT NULL DEFAULT '0',
  `boss_lost` INT(11) NOT NULL DEFAULT '0',
  `boss_tie` INT(11) NOT NULL DEFAULT '0',
  `dom_bases` INT(11) NOT NULL DEFAULT '0',
  `dom_off_kills` INT(11) NOT NULL DEFAULT '0',
  `dom_def_kills` INT(11) NOT NULL DEFAULT '0',
  `dom_wins` INT(11) NOT NULL DEFAULT '0',
  `dom_lost` INT(11) NOT NULL DEFAULT '0',
  `dom_tie` INT(11) NOT NULL DEFAULT '0',
  `td_kills` INT(11) NOT NULL DEFAULT '0',
  `td_deaths` INT(11) NOT NULL DEFAULT '0',
  `td_wins` INT(11) NOT NULL DEFAULT '0',
  `td_lost` INT(11) NOT NULL DEFAULT '0',
  `td_tie` INT(11) NOT NULL DEFAULT '0',
  `sc_stolen` INT(11) NOT NULL DEFAULT '0',
  `sc_captured` INT(11) NOT NULL DEFAULT '0',
  `sc_dropped` INT(11) NOT NULL DEFAULT '0',
  `sc_wins` INT(11) NOT NULL DEFAULT '0',
  `sc_lost` INT(11) NOT NULL DEFAULT '0',
  `sc_tie` INT(11) NOT NULL DEFAULT '0',
  `ctf_taken` INT(11) NOT NULL DEFAULT '0',
  `ctf_captured` INT(11) NOT NULL DEFAULT '0',
  `ctf_dropped` INT(11) NOT NULL DEFAULT '0',
  `ctf_wins` INT(11) NOT NULL DEFAULT '0',
  `ctf_lost` INT(11) NOT NULL DEFAULT '0',
  `ctf_tie` INT(11) NOT NULL DEFAULT '0',
  `emperium_kills` INT(11) NOT NULL DEFAULT '0',
  `barricade_kills` INT(11) NOT NULL DEFAULT '0',
  `guardian_stone_kills` INT(11) NOT NULL DEFAULT '0',
  `conquest_wins` INT(11) NOT NULL DEFAULT '0',
  `conquest_losses` INT(11) NOT NULL DEFAULT '0',
  `kill_count` INT(11) NOT NULL DEFAULT '0',
  `death_count` INT(11) NOT NULL DEFAULT '0',
  `wins` INT(11) NOT NULL DEFAULT '0',
  `losses` INT(11) NOT NULL DEFAULT '0',
  `ties` INT(11) NOT NULL DEFAULT '0',
  `wins_as_leader` INT(11) NOT NULL DEFAULT '0',
  `losses_as_leader` INT(11) NOT NULL DEFAULT '0',
  `ties_as_leader` INT(11) NOT NULL DEFAULT '0',
  `total_deserted` INT(11) NOT NULL DEFAULT '0',
  `score` INT(11) NOT NULL DEFAULT '0',
  `points` INT(11) NOT NULL DEFAULT '0',
  `sp_heal_potions` INT(11) NOT NULL DEFAULT '0',
  `hp_heal_potions` INT(11) NOT NULL DEFAULT '0',
  `yellow_gemstones` INT(11) NOT NULL DEFAULT '0',
  `red_gemstones` INT(11) NOT NULL DEFAULT '0',
  `blue_gemstones` INT(11) NOT NULL DEFAULT '0',
  `poison_bottles` INT(11) NOT NULL DEFAULT '0',
  `acid_demostration` INT(11) NOT NULL DEFAULT '0',
  `acid_demostration_fail` INT(11) NOT NULL DEFAULT '0',
  `support_skills_used` INT(11) NOT NULL DEFAULT '0',
  `healing_done` INT(11) NOT NULL DEFAULT '0',
  `wrong_support_skills_used` INT(11) NOT NULL DEFAULT '0',
  `wrong_healing_done` INT(11) NOT NULL DEFAULT '0',
  `sp_used` INT(11) NOT NULL DEFAULT '0',
  `zeny_used` INT(11) NOT NULL DEFAULT '0',
  `spiritb_used` INT(11) NOT NULL DEFAULT '0',
  `ammo_used` INT(11) NOT NULL DEFAULT '0',
  `ranked_points` INT(11) NOT NULL DEFAULT '0',
  `ranked_games` INT(11) NOT NULL DEFAULT '0',
  `ru_captures` INT(11) NOT NULL DEFAULT '0',
  `ru_wins` INT(11) NOT NULL DEFAULT '0',
  `ru_lost` INT(11) NOT NULL DEFAULT '0',
  `ru_skulls` INT(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`char_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
