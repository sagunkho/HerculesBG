# Hercules Battlegrounds
This project is a full conversion and enhancement of eAmod's Battlegrounds 2.0 system into an easy-to-install HerculesWS/Hercules plugin library.

#### Pre-requisites
- HerculesWS/Hercules.git emulation software.

#### Installation
1. Place plugin files in plugin directory. 
- (How to re-compile plugins: [Hercules Plugin Compilation Guide] (http://herc.ws/wiki/HPM#Building_a_plugin)).
2. Place all script files in the default directory provided in the repository (or your own).
3. Place all database and config parts in their respective files.
4. in conf/plugins.conf add "hBG".
5. Start your server.


#### Contributors
[Smokexyz](https://github.com/Smokexyz) / [sagunkho](https://github.com/sagunkho)

#### Documentation
##### Available Battleground Modes
- Capture the Flag
- Eye of Storm
- Triple Inferno
- Team DeathMatch
- Bossnia
- Conquest
- Stone Control
- Domination
- Rush
- Free For All

##### Script Commands

````
hBG_team_create
hBG_queue_create
hBG_queue_event
hBG_queue_join
hBG_queue_partyjoin
hBG_queue_leave
hBG_queue_data
hBG_queue2team
hBG_queue2team_single
hBG_queue2teams
hBG_queue_checkstart
hBG_balance_teams
hBG_waitingroom2bg
hBG_waitingroom2bg_single
hBG_team_setxy
hBG_team_reveal
hBG_team_conceal
hBG_team_setquest
hBG_viewpointmap
hBG_monster_reveal
hBG_monster_set_team
hBG_monster_immunity
hBG_leave
hBG_destroy
hBG_clean
hBG_get_data
hBG_getareausers
hBG_updatescore
hBG_team_updatescore
hBG_team_guildid
hBG_getitem
hBG_getkafrapoints
hBG_reward
hBG_flooritem2xy
````

##### @commands

````
@joinbg - join a bg queue.
@leavebg - leave a bg queue.
````

#### License
GNU GPL V3, 29 June 2007
