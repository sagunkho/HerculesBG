# Hercules Battlegrounds
This project is a full conversion and enhancement of eAmod's Battlegrounds system into an easy-to-install HerculesWS/Hercules plugin library.

#### Pre-requisites
- [HerculesWS/Hercules](https://github.com/HerculesWS/Hercules) emulation software.

#### Installation
1. Place plugin files in plugin directory. 
- (How to re-compile plugins: [Hercules Plugin Compilation Guide] (http://herc.ws/wiki/HPM#Building_a_plugin)).
2. Place all script files in the default directory provided in the repository.
3. Import all provided database and config parts in their respective files.
4. in conf/plugins.conf add "hBG".
5. Start your server.

#### Important Steps
- Make sure SQL files are imported into your database.
- Make sure to ***rebuild your mapcache***.
- Configure your settings in `conf/map/battle/hBG.conf`

#### Documentation
##### Available Battleground Modes
[x] Capture the Flag
[x] Eye of Storm
[x] Triple Inferno
[x] Team DeathMatch
[x] Bossnia
[x] Conquest
[x] Stone Control
[x] Domination
[x] Rush
[x] Free For All

##### Script Commands

***hBG_team_create***(<map_name>, <x>, <y>, <guild_index>, <logout_event>, <die_event>)
: Creates a new battleground team.
- `map_name` is a string argument.
- `x` (int) co-ordinate.
- `y` (int) co-ordinate.
- `guild_index` (int) index of the battleground guild. (1-13)
- `logout_event` (int) the npc event called when a player logs out. (used for cleanups)
- `die_event` (string) the npc event called when a player dies.
- @return (int) team id on success, 0 on failure.

***hBG_queue_create***(<queue_name>, <join_event> {,<min_level>})
: Creates a new queue.
- `queue_name` (int) name of the queue.
- `join_event` (string) event called when a player joins a queue.
- `min_level` (optional int) minimum level to join a queue.
- @return (int) queue id on success, 0 on failure.

***hBG_queue_event***(<queue_id>, <join_event>)
: Sets the event for a queue called when a player joins.
- `queue_id` (int) id of the queue.
- `join_event` (string) npc event called when a player joins a queue.
- @return (int) 0 on failure, 1 on success

***hBG_queue_join***(<queue_id>)
: Makes the attached player join a queue.
- `queue_id` (int) id of the queue
- @return (int) 0 on failure, position in queue on success.

***hBG_queue_partyjoin***(<party_id>, <queue_id>)
: Adds a party into a queue.
- `party_id` (int) Id of the party joining the queue.
- `queue_id` (int) Id of the queue to be joined.
- @return (int) 0 on failure, 1 on success.

***hBG_queue_leave***(<queue_id>)
: Makes the attached player leave a queue.
- `queue_id` (int) Id of the queue to be left.
- @return (int) 0 on failure, 1 on success.

***hBG_queue_data***(<queue_id>, <info_type>)
: Retrieves information of a particular queue.
- `queue_id` (int) Id of the queue.
- `info_type` (int) Information types (0 - User Count | 1 - Copies user names to an array variable `$@qmembers$`)
- @return (int) 0 on failure, 1 on success.

***hBG_queue2team***(<queue_id>, <max_team_members>, <respawn_map>, <respawn_x>, <respawn_y>, <guild_index>, <logout_event>, <die_event>)
: Creates a new battleground team with all the users in queue.
- `queue_id` (int) Id of the queue.
- `max_team_members` (int) Maximum amount of members in a team.
- `respawn_map` (string) Map name to respawn in.
- `respawn_x` (int) X co-ordinate of the respawn_map.
- `respawn_y` (int) Y co-ordinate of the respawn_map.
- `guild_index` (int) Index of the BG Guild to join. (1-13)
- `logout_event` (string) NPC event called on player logout.
- `die_event` (string) NPC event called on player death.
- @return (int) 0 on failure, bg_id on success.

***hBG_queue2team_single***(<queue_id>, <bg_id>, <map_name>, <map_x>, <map_y>)
: Makes a single player from a queue join a battleground.
- `queue_id` (int) Id of the queue.
- `bg_id` (int) Id of the battleground.
- `map_name` (string) Name of the map
- `map_x` (int) X co-ordinate of the map.
- `map_y` (int) Y co-ordinate of the map.
- @return (int) 0 on failure, 1 on success.

***hBG_queue2teams***(<queue_id>, <min_players>, <max_players>, <type>, <team_1>, <team_2> {,...<team_13>})
: Builds BG Teams from a queue.
- `queue_id` (int) Id of a queue.
- `min_players` (int) Minimum amount of players per team.
- `max_players` (int) Maximum amount of players per team.
- `type` (int) Team building method (0 --- Lineal [allows party queueing] | 1 --- Randomized )
- `team_1` (int) Battleground Team Id 1 (required)
- `team_2` (int) Battleground Team Id 2 (required)
- ... `team_13` (int) Battleground Team Id 13 (optional teams up to 13)
- @return (int) 0 on failure, 1 on success.

***hBG_queue_checkstart***(<queue_id>, <type>, <total_teams>, <min_players>)
: Checks if the given BG queue can start a BG in the given mode.
- `queue_id` (int) Id of a queue.
- `type` (int) Type of queue building method. (0 -- Lineal | 1 -- Randomized)
- `total_teams` (int) Total number of teams competing in the Battleground.
- `min_players` (int) Minimum required players per team.
- @return (int) 0 if not ready to start, 1 if not.

***hBG_balance_teams***(<queue_id>, <max_players>, <type>, <team_1>, <team_2>{, ... <team_13>})
: Fills the teams with members from the given queue.
- `queue_id` (int) Id of a queue.
- `max_payers` (int) Maximum players for the team.
- `type` (int) Balancing type method to be used. (0 - Equality | 1 - Class Balance -- not supported yet)
- `team_1` .. `team_13` (int) Team IDs to balance.
- @return (int) 0 on failure, 1 on success.

***hBG_waitingroom2bg***(<map_name>, <map_x>, <map_y>, <guild_index>, <logout_event>, <die_event>)
: Creates a battleground team from players in a waiting room.
- `map_name` (string) Name of the battleground map.
- `map_x` (int) X co-ordinate warp point on the map.
- `map_y` (int) Y co-ordinate warp point on the map.
- `guild_index` (int) battleground guild index (1-13)
- `logout_event` (string) NPC Event called when a player logs out inside a BG.
- `die_event` (string) NPC Event called when player dies in a BG.
- @return (int) 0 on failure, battleground Id on success.

***hBG_waitingroom2bg_single***(<team_id>, <map_name>, <map_x>, <map_y>, <npc_name>)
: Adds the first player from the npc's waiting room into a BG team.
- `team_id` (int) Id of the battleground team.
- `map_name` (int) Name of the map.
- `map_x` (int) X co-ordinate of the warp point on the map.
- `map_y` (int) Y co-ordinate of the warp point on the map.
- `npc_name` (string) Name of the npc.
- @return (int) 0 on failure, 1 on success.

***hBG_team_setxy***(<team_id>, <map_x>, <map_y>)
: Sets the respawn location of a team on the battleground map.
- `team_id` (int) Id of the battleground team.
- `x` (int) X co-ordinate on the map.
- `y` (int) Y co-ordinate on the map.
@return (int) 0 on failure, 1 on success.

***hBG_team_reveal***(<team_id>)
: Reveals the location of a BG Team on the mini-map.
- `team_id` (int) Id of the battleground team.
@return (int) 0 on failure, 1 on success

***hBG_team_conceal***(<team_id>)
: Conceals the location of a BG Team from the mini-map.
- `team_id` (int) Id of the battleground team.
@return (int) 0  on failure, 1 on success.

***hBG_team_setquest***(<team_id>, <quest_id>)
: Sets a quest for a given battleground team.
- `team_id` (int) Id of the battleground team.
- `quest_id` (int)  Id of the quest.
@return (int) 0 on failure, 1 on success.

***hBG_viewpointmap***(<map_name>, <type>, <map_x>, <map_y>, <viewpoint_id>, <color>)
: Sets a viewpoint indicator for an npc that calls this function, on the minimap of all players in the map.
- `map_name` (int) Name of the map.
- `type` (int) Viewpoint Types (0 - Display for 15 seconds | 1 - display until dead or teleported | 2 - Remove Viewpoint)
- `x` (int) X co-ordinate on the mini-map.
- `y` (int) Y co-orindate on the mini-map.
- `viewpoint_id` (int) Unique Id of the viewpoint.
- `color` (int) Color of the viewpoint. (0x00RRGGBB)
- @return 0 on failure, 1 on success.

***hBG_monster_reveal***(<gid>, <type>, <color>)
: Reveals a monster's location on the minimap.
- `gid` (int) GID of the monster.
- `type` (int) Viewpoint Types (0 - Display for 15 seconds | 1 - display until dead or teleported | 2 - Remove Viewpoint)
- `color` (int) Color of the viewpoint. (0x00RRGGBB)
- @return 0 on failure, 1 on success.

***hBG_monster_set_team***(<gid>, <team_id>)
: Adds a monster to a battleground team.
- `gid` (int) GID of the monster.
- `team_id` (int) Id of the battleground team.
- @return 0 on failure, 1 on success.

***hBG_monster_immunity***(<gid>, <flag>)
: Sets an immunity state from damage on a monster.
- `gid` (int) GID of the monster.
- `flag` (int) 0 = off | 1 = On
- @return 0 on failure, 1 on success.

***hBG_leave***()
: Makes the attached player leave a battleground team.
- @return 0 on failure, 1 on success.

***hBG_destroy***(<team_id>)
: Destroys/Deletes battleground team.
- `team_id` (int) Battleground team Id.
- @return 0 on failure, 1 on success.

***hBG_clean***(<team_id>)
: Cleans/Removes all players from a Battleground Team.
- `team_id` (int) Battleground team Id.
- @return 0 on failure, 1 on success.

***hBG_getareausers***(<team_id>, <map_name>, <x1>, <y1>, <x2>, <y2>)
: Gets the user count within an area in a Battleground map.
- `map_name` (string) Name of the battleground map.
- `x1` (int)
- `y1` (int)
- `x2` (int)
- `y2` (int)
- @return number of players within the area, or 0 on failure.

***hBG_updatescore***(<map_name>, <lion_score>, <eagle_score>)
: Update the score on a battleground map.
- `map_name` (string) Name of the Battleground map.
- `lion_score` (int) Score for the lion Icon on the map.
- `eagle_score` (int) Score for the eagle Icon on the map.
- @return 0 on failure, 1 on success.

***hBG_team_updatescore***(<team_id>, <score>)
: Updates the score for a battleground team.
- `team_id` (int) Id of the battleground team.
- `score` (int) Score of the team.
- @return 0 on failure, 1 on success.

***hBG_get_team_gid***(<team_id>)
: Retrieves the guild index of a battleground team
- `team_id` (int) Id of the battleground team.
- @return guild index of a battleground team, or 0 on failure.

***hBG_get_data***(<team_id>, <type>)
: Retrieve information of a battleground team.
- `team_id` (int) Id of the battleground team.
- `type` (int) Types - 
- - 0 -- (int) User Count 
- - 1 -- (var) Fill variable `$@bgmembers$` array with user list and return count 
- - 2 -- (string) Team Name
- - 3 -- (string) Team Master Name
- - 4 -- Chat color of the Team.
-- @return data or 0 on failure.

***hBG_getitem***(<team_id>, <item_id>, <amount>)
: Gives an Item x amount to a battleground team.
- `team_id` (int) ID of the battleground team.
- `item_id` (int) Id of the item.
- `amount` (int) Amount of the item.
- @return 0 on failure, `amount` on success.

***hBG_getkafrapoints***(<team_id>, <kafra_points>)
: Gives a battleground team kafra points.
- `team_id` (int) ID of the battleground team.
- `kafra_points` (int) Kafra points to be given.
- @return 0 on failure, `kafra_points` on success.

***hBG_reward***(<team_id>, <item_id>, <item_amount>, <kafra_points>, <quest_id>, <custom_variable>, <variable_value>, <bg_arena>, <bg_result>)
: Gives rewards and sets other data for the members of a battleground team.
- `team_id` (int) ID of the battleground team.
- `item_id` (int) Id of the item.
- `amount` (int) Amount of the item.
- `kafra_points` (int) Kafra points to be given.
- `quest_id` (int) Quest Id to be set.
- `custom_variable` (var reference) A variable to add for the members (eg. #KAFRAPOINTS, #CASHPOINTS etc...)
- `variable_value` (int) amount for the `custom_variable` to be added.
- `bg_arena` (int) Arena Id of the BG (0 EoS | 1 Boss | 2 TI | 3 CTF | 4 TD | 5 SC | 6 CON | 7 RUSH | 8 DOM)
- `bg_result` (int) Result of the Battleground (0 Won | 1 Tie | 2 Lost)
- @return 0 on failure, 1 on success.

***hBG_flooritem2xy***(<map_name>, <x>, <y>, <item_id>, <item_amount>)
: Adds an item on the floor of a map and it's given co-ordinates.
Arguments are self-explanatory :D

##### @commands

````
@joinbg - join a bg queue.
@leavebg - leave a bg queue.
@reportafk - Report an AFK player.
@bgstart - Enable battleground rotation and send an announcement as a GM.
@bgstop - Disable battleground rotation and send an announcement as a GM.
````

#### Support
[Project Topic](http://herc.ws/board/topic/14083-hercules-battlegrounds/) on http://herc.ws/board/

#### Author
[Smokexyz](https://github.com/Smokexyz) / [sagunkho](https://github.com/sagunkho)

If you like this project, you can help motivate it's development by donating a minimum amount ---
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=NZP4VCYY9RP4U)

#### License
GNU GPL V3, 29 June 2007
