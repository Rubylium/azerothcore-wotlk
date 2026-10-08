-- The FX lab (FxLab.cpp, .agents/docs/systems/fx-lab.md): the middle of the plain gray room on map 451 (development),
-- for `.tele FxLab` besides `.fxlab`.

DELETE FROM `game_tele` WHERE `id` = 2100 OR `name` = 'FxLab';
INSERT INTO `game_tele` (`id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, `name`) VALUES
(2100, 2933.333, 800, 0.5, 0, 451, 'FxLab');
