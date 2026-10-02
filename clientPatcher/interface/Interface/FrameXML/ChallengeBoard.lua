-- The challenge board (server: mod-playerbots ChallengeBoard.cpp, protocol at the top of it). Using the board in
-- Stormwind's Trade District opens it: four missions that change every 20 minutes, each a challenge against one raid
-- boss, fought at once with bots. A challenge won leaves its reward here: gold and a satchel.
-- A banner at the top of the screen follows the challenge itself, from the group being assembled to the way home.
-- Every mission comes in tiers, Défi I to X, picked on the dial at the top of the board: the same boss with more
-- health and damage, fewer attempts, and a bigger reward. Winning at the highest tier open opens the next.
-- The Donjons tab posts dungeons of the Mythic+ pool: one taken up here runs as a key at the player's level, and once
-- it is done a reward waits here, on top of the key's own.
-- The L'Infini tab is the board's god alone (mod-stat-growth InfiniteGod.cpp), the highest raid fight there is: on
-- every level-80 board, with a tier ladder of its own (only a win against it opens its next tier).
-- The Hollow Voice tab is the board's pinnacle (mod-stat-growth HollowVoice.cpp), built like L'Infini's page: on every
-- level-80 board, one difficulty (Défi I only), and signing up asks for an equipped item level (VOICE_*).

local PREFIX = "Challenge"
local SOUND = "Sound\\Interface\\MythicPlus\\"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"   -- headings only (titles, the tier, the key level, banners)
local FRIZ = "Fonts\\FRIZQT__.TTF"        -- the timer and the messages
local SATCHEL = 4573
local SATCHEL_ICON = "Interface\\Icons\\INV_Misc_Bag_07"
local ROLE_TANK, ROLE_HEALER, ROLE_DAMAGE = 2, 4, 8

local WIDTH, HEIGHT = 840, 588
local CARD_WIDTH, CARD_HEIGHT, CARD_GAP = 184, 322, 16
local ROTATION_SECONDS = 20 * 60

local STATE_OPEN, STATE_UNDERWAY, STATE_WON, STATE_CLAIMED = 0, 1, 2, 3
-- RaidFinder::ChallengeEvent
local EVENT_ARRIVED, EVENT_KILLED, EVENT_RETURNING, EVENT_WIPED, EVENT_WON, EVENT_FAILED = 1, 2, 3, 4, 5, 6
local EVENT_PULLING = 7
local EVENT_FADING = 8
-- A dungeon challenge completed (ChallengeBoard.cpp EVENT_DUNGEON_WON)
local EVENT_DUNGEON_WON = 20
local KIND_DUNGEON = 2
local KEYSTONE_ICON = "Interface\\Icons\\INV_Relics_Hourglass"
-- The board's god: L'Infini, and its page's art (localTools/interface/buildChallengeGodArt.py): its figure fills the
-- top GOD_FIGURE_BOTTOM of its texture
local GOD_BOSS = 930000
local GOD_ART = "Interface\\ChallengeBoard\\"
local GOD_FIGURE_BOTTOM = 0.6367
-- Its gear: the server sends Défi I's item level as the mission's, each tier adds this (ChallengeBoard.cpp)
local GOD_ITEM_LEVEL_PER_TIER = 10
local GOD_REQUIRED_ITEM_LEVEL_PER_TIER = 10
-- The paragon it asks: the server sends Défi I's as the mission's base, each tier adds this (ChallengeTiers.h)
local GOD_PARAGON_PER_TIER = 50
-- One attempt at every tier (ChallengeTiers.h BossProfiles): a wipe sends everyone home
local GOD_ATTEMPTS = 1
-- The Hollow Voice: its page's art (ChallengeVoice-*, the same script), one difficulty and one attempt
-- (ChallengeTiers.h BossProfiles). The server sends the equipped item level signing up asks for (the mission's) and
-- the player's own (the board's), both Player::GetAverageItemLevelForDF, the Dungeon Finder's measure.
local VOICE_BOSS = 930100
local VOICE_FIGURE_BOTTOM = 0.6367
local VOICE_ATTEMPTS = 1

-- Tiers: what each does, as the server has it (mod-playerbots ChallengeTiers.h). Change them together.
local TIER_MIN, TIER_MAX = 1, 10
local TIER_ROMAN = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X" }
local TIER_GUARANTEED_ITEM = 7
-- A raid mission's satchel piece above Défi I: an epic fitted to the player, the drops' item level and this more a
-- tier (ChallengeTiers.h ItemLevelPerTier)
local TIER_ITEM_LEVEL = 5

local function TierItemLevel(itemLevel, tier)
    return itemLevel + TIER_ITEM_LEVEL * (tier - TIER_MIN)
end
-- ChallengeBoard.cpp SatchelExtraItemChance, by raid mode (10, 25, 10 heroic, 25 heroic)
local SATCHEL_ITEM_CHANCE = { [0] = 35, 45, 50, 60 }

-- A tier asks for 31 more paragon points, and the boss's health grows by what they are measured to be worth (the
-- bench's curve, flat past 425 points: PowerScaling.h ParagonCurve), its damage 11.5% a tier; L'Infini by its own
-- profile (below). A key past +10 asks for 8.5 a level (MythicDungeon.h). ChallengeTiers.h holds the server's:
-- change them together.
local PARAGON_CURVE = { { 0, 1.0 }, { 85, 2.0 }, { 212, 3.7 }, { 255, 5.4 }, { 340, 7.8 }, { 425, 9.6 } }
local function ParagonDpsIndex(points)
    local last = PARAGON_CURVE[#PARAGON_CURVE]
    if points >= last[1] then return last[2] end
    for index = 2, #PARAGON_CURVE do
        local low, high = PARAGON_CURVE[index - 1], PARAGON_CURVE[index]
        if points <= high[1] then
            return low[2] + (high[2] - low[2]) * (points - low[1]) / (high[1] - low[1])
        end
    end
    return last[2]
end
local function TierParagon(tier) return 31 * (tier - 1) end
local function TierHealth(tier) return ParagonDpsIndex(TierParagon(tier)) end
local function TierDamage(tier) return 1.1152 ^ (tier - 1) end
-- L'Infini's tiers, from its profile (ChallengeTiers.h HealthMultiplier / DamageMultiplier with its BossProfile,
-- worked out on the power model): health by the profile's power, damage by its health. Change them together.
local GOD_TIER_HEALTH = { 1.00, 1.35, 1.72, 2.61, 3.44, 4.24, 4.93, 5.35, 5.49, 5.63 }
local GOD_TIER_DAMAGE = { 1.00, 1.08, 1.15, 1.23, 1.31, 1.39, 1.47, 1.55, 1.63, 1.72 }
local function KeyParagon(level) return level > 10 and floor((17 * (level - 10) + 1) / 2) or 0 end
-- Paragon only exists at the level cap: below it the boards say nothing about it
local PARAGON_BRACKET = 80
local function TierWipes(tier) return tier >= 9 and 1 or tier >= 6 and 2 or 3 end
local function TierGoldPercent(tier) return 100 + 30 * (tier - 1) end
local function TierExtraEssences(tier) return 3 * (tier - 1) end
local function TierExtraParagon(tier) return floor((tier - 1) / 2) end
local function TierExtraItemChance(tier) return 7 * (tier - 1) end

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Tableau des défis",
    heading = "Missions",
    intro = "Chaque défi vous mène droit devant un boss de raid : des compagnons complètent votre groupe, "
        .. "vous l'affrontez sur-le-champ, et la récompense vous attend ici.",
    refresh = "Nouvelles missions dans",
    challenge = "DÉFI",
    players = "%d joueurs · %s",
    normal = "Normal",
    heroic = "Héroïque",
    heroicTag = "HÉROÏQUE",
    itemLevel = "Butin de niveau d'objet %d",
    tierItemLevel = "Butin %d · pièce du défi : niveau d'objet %d",
    required = "Requis : niv. d'objet %d+ · parangon %d",
    paragon = "+%d Parangon",
    essences = "+%d essences",
    rewards = "Récompenses",
    take = "Relever le défi",
    claim = "Récupérer",
    underway = "En cours",
    claimed = "Accomplie",
    role = "Votre rôle",
    quit = "Abandonner le défi",
    waiting = "Récompenses en attente",
    locked = "Les missions s'ouvrent au niveau 60.\nRevenez plus tard, aventurier.",
    empty = "Aucune mission pour le moment.",
    satchel = "Sacoche du défi",
    satchelHint = "De l'or, et peut-être un butin de plus du boss.",
    accepted = "Défi accepté",
    gold = "or",
    assembling = "Constitution du groupe",
    ready = "Le groupe est prêt : en route !",
    travel = "En route vers %s…",
    fight = "Affrontez %s !",
    killed = "%s est vaincu ! Tirages du butin…",
    returning = "Retour au tableau dans %d",
    pulling = "Le tank engage le combat dans %d",
    wiped = "Le groupe est tombé. Tentatives restantes : %d",
    won = "Défi réussi ! Votre récompense vous attend au tableau.",
    deadInChallenge = "Défi en cours : vous serez relevé à la fin du combat.",
    tabRaids = "Raids",
    tabDungeons = "Donjons",
    headingDungeons = "Donjons",
    introDungeons = "Choisissez un donjon du Mythique+ et lancez-le d'ici, à votre clé. Terminez-le : une récompense "
        .. "de plus vous attend au tableau, en plus de celle du donjon.",
    dungeonTag = "DONJON",
    mythicTag = "MYTHIQUE+",
    key = "Votre clé",
    dungeonKey = "Mythique+ · clé +%d",
    dungeonTime = "Temps imparti : %d min",
    timedBonus = "Terminé à temps : l'or et les essences ×1,5, et un point de parangon de plus.",
    launch = "Lancer le donjon",
    dungeonsLocked = "Les défis de donjon s'ouvrent au niveau 80.\nRevenez plus tard, aventurier.",
    dungeonWon = "Défi de donjon réussi ! Votre récompense vous attend au tableau.",
    wonTimed = "Réussi à temps, clé +%d",
    wonLate = "Réussi hors temps, clé +%d",
    satchelDungeon = "De l'or : l'objet du donjon, la clé vous l'a déjà donné.",
    tier = "Défi %s",
    tierBoss = "Boss : points de vie ×%s · dégâts ×%s · %d tentative%s",
    tierNext = "Remportez un défi en Défi %s pour ouvrir le palier suivant.",
    tierTop = "Le palier ultime.",
    tierOpened = "Nouveau palier ouvert : Défi %s !",
    tierTitle = "Paliers des défis",
    tierHelp = "Le même boss, plus coriace à chaque palier, et mieux payé. Remporter un défi au plus haut palier "
        .. "ouvert ouvre le suivant.",
    tierReward = "Récompense : or +%d%%, +%d essences, +%d parangon",
    paragonWanted = "Parangon conseillé",
    paragonValue = "%d  ·  vous %d",
    paragonTip = "Parangon conseillé : %d (vous : %d). Le boss grandit avec le parangon qu'il demande : à ce "
        .. "niveau, le combat est celui du Défi I.",
    keyParagon = "Parangon conseillé pour +%d : %d (vous : %d)",
    keyParagonNone = "Jusqu'à +10, aucun parangon requis : la clé se joue à l'équipement.",
    satchelChance = "Butin du boss : %d%% de chances.",
    satchelSure = "Un butin du boss assuré, un second à %d%%.",
    satchelTierGear = "Au-dessus du défi I, ce butin est une épique faite pour vous, de niveau d'objet %d.",
    tabGod = "L'Infini",
    headingGod = "Le défi ultime",
    introGod = "Au cœur d'Ulduar, là où les Titans gravèrent la carte des cieux, une présence veille depuis l'aube "
        .. "du monde.",
    godEpithet = "Gardien du Planétarium céleste",
    godQuote = "« Vous comptez vos vies en battements de cœur. J'ai vu naître les étoiles, et je les verrai "
        .. "s'éteindre. »",
    godSignature = "— L'Infini",
    godLore = "Quand les Titans ordonnèrent le cosmos, ils laissèrent au Planétarium un gardien chargé d'en tenir "
        .. "les comptes. Seul face aux cieux sans fin, il a regardé chaque monde naître puis s'éteindre, jusqu'à "
        .. "devenir ce qu'il contemplait. Il ne défend ni Ulduar ni ses maîtres : il juge ceux qui prétendent défier "
        .. "l'ordre des étoiles, et les rend à la poussière dont ils sont faits.",
    godRequired = "Requis",
    godItemLevel = "Niveau d'objet %d+",
    godGear = "Équipement épique imprégné, niveau d'objet %d",
    godGearTitle = "Imprégné par L'Infini",
    godGearHelp = "Chaque pièce qu'il laisse porte l'un de ces pouvoirs, tiré au hasard, quelle qu'elle soit :",
    godGearBonuses = {
        { "Égide des astres", "Quand vous subissez des dégâts, chance de réduire de 5% les dégâts subis "
            .. "pendant 10 s." },
        { "Éclat d'étoile filante", "Vos attaques et sorts nuisibles ont une chance d'infliger 4000 points de "
            .. "dégâts des Arcanes à la cible." },
        { "Étincelle d'éternité", "Vos attaques et sorts ont une chance d'augmenter votre score de hâte de 150 "
            .. "pendant 10 s." },
    },
    godParagon = "Parangon conseillé %d · vous %d",
    godFace = "Affronter L'Infini",
    godOnce = "Une victoire par tableau : il revient avec les nouvelles missions.",
    godLocked = "L'Infini attend les aventuriers de niveau 80.",
    godTierTitle = "Paliers de L'Infini",
    godTierHelp = "Ses paliers s'ouvrent à part des missions : seule une victoire contre lui ouvre le suivant.",
    godTierNext = "Vainquez L'Infini en Défi %s pour ouvrir le palier suivant.",
    tabVoice = "La Voix creuse",
    headingVoice = "Le pinacle",
    introVoice = "Au Plateau du Puits de soleil, dans la salle où flottait M'uru, un archevêque prêche encore à une "
        .. "nef déserte.",
    voiceName = "La Voix creuse",
    voiceEpithet = "L'archevêque Aldric Mantaube et Vel'thazar",
    voicePlace = "Plateau du Puits de soleil",
    voiceQuote = "« La Lumière vous a jugés. Je ne suis que sa main. »",
    voiceSignature = "— L'archevêque Aldric Mantaube",
    voiceLore = "Aldric Mantaube vint au Puits de soleil quand la Lumière y renaquit, pour veiller sur elle. Une voix "
        .. "l'y attendait, douce comme une prière ; il la crut divine et la laissa parler par sa bouche. Ce n'était "
        .. "pas la Lumière, mais Vel'thazar, un seigneur de l'effroi tapi dans sa foi, qui se repaît de chaque âme "
        .. "jugée en son nom. Faites taire l'archevêque, et la Voix creuse se lèvera de ce qu'il en reste.",
    voiceTier = "Défi unique",
    voiceProfile = "Taillé pour : niv. d'objet %d · parangon %d · %d tentative%s",
    voiceTierTitle = "Une seule difficulté",
    voiceTierHelp = "La Voix creuse n'a pas de paliers : un seul combat, taillé pour un groupe de %d joueurs au niveau "
        .. "d'objet %d avec %d points de parangon. Une défaite renvoie tout le monde au tableau.",
    voiceItemLevel = "Niv. d'objet équipé %d+ · vous %d",
    voiceItemLevelUnknown = "Niveau d'objet équipé %d+",
    voiceLocked = "Niveau d'objet équipé insuffisant : %d requis, vous %d.",
    voiceLockHelp = "Pour s'inscrire, chaque joueur du groupe doit porter un niveau d'objet moyen d'au moins %d (celui "
        .. "de la recherche de donjons).",
    voiceFace = "Affronter la Voix creuse",
    voiceOnce = "Une victoire par tableau : elle revient avec les nouvelles missions.",
    voiceAbsent = "La Voix creuse attend les aventuriers de niveau 80.",
    satchelVoice = "De l'or : la Voix creuse ne laisse aucun butin.",
    failed = {
        [1] = "Défi échoué : trop de tentatives.",
        [2] = "Défi échoué : le temps est écoulé.",
        [3] = "Défi annulé : le boss est introuvable.",
    },
    errors = {
        [1] = "Approchez-vous du tableau.",
        [2] = "Les missions s'ouvrent au niveau 60.",
        [3] = "Cette mission n'est plus au tableau.",
        [4] = "Mission déjà accomplie sur ce tableau.",
        [5] = "Vous êtes déjà dans un défi, un raid ou une file d'attente.",
        [6] = "Seul le chef du groupe peut relever un défi.",
        [7] = "Impossible pour le moment (champ de bataille, recherche de donjon…).",
        [8] = "Choisissez au moins un rôle.",
        [9] = "Votre groupe est trop nombreux pour ce défi.",
        [10] = "Ce défi n'est pas disponible.",
        [11] = "Aucune récompense à récupérer.",
        [12] = "Vos sacs sont pleins.",
        [13] = "Un membre du groupe n'a pas le niveau requis.",
        [14] = "Ce palier n'est pas encore ouvert.",
        [15] = "Les défis de donjon s'ouvrent au niveau 80.",
        [16] = "Ce défi exige un niveau d'objet équipé de %d (le vôtre : %d).",
        [17] = "%s n'a pas le niveau d'objet équipé requis (%d).",
    },
} or {
    title = "Challenge Board",
    heading = "Missions",
    intro = "Each challenge takes you straight before a raid boss: companions fill your group, you fight it at "
        .. "once, and the reward waits for you here.",
    refresh = "New missions in",
    challenge = "CHALLENGE",
    players = "%d players · %s",
    normal = "Normal",
    heroic = "Heroic",
    heroicTag = "HEROIC",
    itemLevel = "Drops item level %d",
    tierItemLevel = "Drops %d · challenge piece: item level %d",
    required = "Requires item level %d+ · paragon %d",
    paragon = "+%d Paragon",
    essences = "+%d essences",
    rewards = "Rewards",
    take = "Take the challenge",
    claim = "Claim",
    underway = "Under way",
    claimed = "Completed",
    role = "Your role",
    quit = "Abandon the challenge",
    waiting = "Rewards waiting",
    locked = "Missions open at level 60.\nCome back later, adventurer.",
    empty = "No missions for now.",
    satchel = "Challenge Satchel",
    satchelHint = "Gold, and maybe one more of the boss's spoils.",
    accepted = "Challenge accepted",
    gold = "gold",
    assembling = "Assembling the group",
    ready = "The group is ready: on your way!",
    travel = "On your way to %s…",
    fight = "Face %s!",
    killed = "%s is defeated! Rolling for loot…",
    returning = "Back to the board in %d",
    pulling = "The tank pulls in %d",
    wiped = "The group fell. Attempts left: %d",
    won = "Challenge won! Your reward waits at the board.",
    deadInChallenge = "Challenge under way: you will be raised when the fight ends.",
    tabRaids = "Raids",
    tabDungeons = "Dungeons",
    headingDungeons = "Dungeons",
    introDungeons = "Pick a Mythic+ dungeon and set out from here, at your key. Complete it: one more reward waits "
        .. "for you at the board, on top of the dungeon's own.",
    dungeonTag = "DUNGEON",
    mythicTag = "MYTHIC+",
    key = "Your key",
    dungeonKey = "Mythic+ · key +%d",
    dungeonTime = "Time limit: %d min",
    timedBonus = "Completed in time: gold and essences ×1.5, and one more paragon point.",
    launch = "Set out",
    dungeonsLocked = "Dungeon challenges open at level 80.\nCome back later, adventurer.",
    dungeonWon = "Dungeon challenge complete! Your reward waits at the board.",
    wonTimed = "Completed in time, key +%d",
    wonLate = "Completed over time, key +%d",
    satchelDungeon = "Gold: the dungeon's item, the key already gave you.",
    tier = "Tier %s",
    tierBoss = "Boss: health ×%s · damage ×%s · %d attempt%s",
    tierNext = "Win a challenge at tier %s to open the next one.",
    tierTop = "The ultimate tier.",
    tierOpened = "New tier open: tier %s!",
    tierTitle = "Challenge tiers",
    tierHelp = "The same boss, tougher at every tier, and better paid. Winning a challenge at the highest tier open "
        .. "opens the next.",
    tierReward = "Reward: gold +%d%%, +%d essences, +%d paragon",
    paragonWanted = "Recommended paragon",
    paragonValue = "%d  ·  yours %d",
    paragonTip = "Recommended paragon: %d (yours: %d). The boss grows with the paragon it asks for: at that level, "
        .. "the fight is the one of Défi I.",
    keyParagon = "Recommended paragon for +%d: %d (yours: %d)",
    keyParagonNone = "Up to +10 no paragon is needed: the key is a matter of gear.",
    satchelChance = "Boss drop: %d%% chance.",
    satchelSure = "One boss drop for sure, a second at %d%%.",
    satchelTierGear = "Above Défi I, that drop is an epic made for you, item level %d.",
    tabGod = "L'Infini",
    headingGod = "The Ultimate Challenge",
    introGod = "At the heart of Ulduar, where the Titans carved the map of the heavens, a presence has kept watch "
        .. "since the dawn of the world.",
    godEpithet = "Keeper of the Celestial Planetarium",
    godQuote = "\"You count your lives in heartbeats. I watched the stars being born, and I will watch them "
        .. "die.\"",
    godSignature = "— L'Infini",
    godLore = "When the Titans ordered the cosmos, they left a keeper in the Planetarium to hold its reckoning. "
        .. "Alone before the endless heavens, it watched every world be born and fade, until it became what it "
        .. "beheld. It defends neither Ulduar nor its masters: it judges those who would defy the order of the "
        .. "stars, and returns them to the dust they are made of.",
    godRequired = "Requires",
    godItemLevel = "Item level %d+",
    godGear = "Imbued epic gear, item level %d",
    godGearTitle = "Imbued by L'Infini",
    godGearHelp = "Every piece it leaves carries one of these powers, drawn at random, whatever the piece:",
    godGearBonuses = {
        { "Aegis of the Stars", "When you take damage, a chance to take 5% less damage for 10 sec." },
        { "Shooting Star Shard", "Your attacks and harmful spells have a chance to deal 4000 Arcane damage to the "
            .. "target." },
        { "Spark of Eternity", "Your attacks and spells have a chance to raise your haste rating by 150 for 10 sec." },
    },
    godParagon = "Recommended paragon %d · yours %d",
    godFace = "Face L'Infini",
    godOnce = "One win per board: it returns with the new missions.",
    godLocked = "L'Infini awaits adventurers of level 80.",
    godTierTitle = "L'Infini's tiers",
    godTierHelp = "Its tiers open apart from the missions': only a win against it opens the next.",
    godTierNext = "Defeat L'Infini at tier %s to open the next one.",
    tabVoice = "The Hollow Voice",
    headingVoice = "The Pinnacle",
    introVoice = "On the Sunwell Plateau, in the hall where M'uru once floated, an archbishop still preaches to an "
        .. "empty nave.",
    voiceName = "The Hollow Voice",
    voiceEpithet = "Archbishop Aldric Dawnmantle and Vel'thazar",
    voicePlace = "Sunwell Plateau",
    voiceQuote = "\"The Light has judged you. I am only its hand.\"",
    voiceSignature = "— Archbishop Aldric Dawnmantle",
    voiceLore = "Aldric Dawnmantle came to the Sunwell when the Light was reborn there, to watch over it. A voice was "
        .. "waiting for him, soft as a prayer; he took it for the divine and let it speak through his mouth. It was "
        .. "not the Light, but Vel'thazar, a dreadlord hidden in his faith, who feeds on every soul judged in its "
        .. "name. Silence the archbishop, and the Hollow Voice will rise from what is left of him.",
    voiceTier = "One difficulty",
    voiceProfile = "Made for: item level %d · paragon %d · %d attempt%s",
    voiceTierTitle = "A single difficulty",
    voiceTierHelp = "The Hollow Voice has no tiers: one fight, made for a group of %d players at item level %d with %d "
        .. "paragon points. A defeat sends everyone back to the board.",
    voiceItemLevel = "Equipped item level %d+ · yours %d",
    voiceItemLevelUnknown = "Equipped item level %d+",
    voiceLocked = "Equipped item level too low: %d required, yours %d.",
    voiceLockHelp = "To sign up, every player of the group must wear an average item level of at least %d (the Dungeon "
        .. "Finder's).",
    voiceFace = "Face the Hollow Voice",
    voiceOnce = "One win per board: it returns with the new missions.",
    voiceAbsent = "The Hollow Voice awaits adventurers of level 80.",
    satchelVoice = "Gold: the Hollow Voice leaves no spoils.",
    failed = {
        [1] = "Challenge failed: too many attempts.",
        [2] = "Challenge failed: time ran out.",
        [3] = "Challenge cancelled: the boss could not be found.",
    },
    errors = {
        [1] = "Get closer to the board.",
        [2] = "Missions open at level 60.",
        [3] = "This mission is no longer on the board.",
        [4] = "Mission already completed on this board.",
        [5] = "You are already in a challenge, a raid or a queue.",
        [6] = "Only the group leader can take a challenge up.",
        [7] = "Not possible right now (battleground, Dungeon Finder…).",
        [8] = "Pick at least one role.",
        [9] = "Your group is too large for this challenge.",
        [10] = "This challenge is not available.",
        [11] = "No reward to claim.",
        [12] = "Your bags are full.",
        [13] = "A group member is below the required level.",
        [14] = "That tier is not open yet.",
        [15] = "Dungeon challenges open at level 80.",
        [16] = "This challenge asks for an equipped item level of %d (yours: %d).",
        [17] = "%s is below the required equipped item level (%d).",
    },
}

local state = {
    rotation = 0,
    left = 0,
    receivedAt = 0,
    bracket = 0,
    challenge = 0,
    missions = {},
    rewards = {},
    shownRotation = nil,
    openTier = TIER_MIN,        -- the highest tier open to the player in their bracket
    tier = nil,                 -- the tier picked on the dial (the highest open until they pick another)
    godOpenTier = TIER_MIN,     -- the same two for the god, on its own ladder
    godTier = nil,
    currentTier = 0,            -- the tier of the challenge they are in
    page = "raids",             -- the tab shown: "raids", "dungeons", "god" or "voice"
    itemLevel = nil,            -- the player's equipped item level, as the server measures it for a sign-up
    key = 2,                    -- the player's Mythic+ key level
    contract = 0,               -- the dungeon challenge they took up (Dungeon Finder entry), 0 for none
    dungeons = {},
}

local frame, cards, rewardRows, emptyText, timerText, timerFill, errorText, quitButton, roleButtons
local dial, keyStrip, dungeonCards, tabs, godPage, voicePage
local banner
local incoming = { missions = {}, rewards = {}, dungeons = {} }

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function DungeonName(dungeonId)
    return (dungeonId and dungeonId > 0 and GetLFGDungeonInfo(dungeonId)) or ""
end

-- Raids whose Dungeon Finder entry has no art at all (Onyxia's Lair, entries 46 and 257: its texture name is empty):
-- art shipped with the patch, laid out like the Dungeon Finder's own (Interface\LFGFrame)
local BACKGROUND_BY_DUNGEON = {
    [46] = "Interface\\LFGFrame\\UI-LFG-BACKGROUND-ONYXIASLAIR",
    [257] = "Interface\\LFGFrame\\UI-LFG-BACKGROUND-ONYXIASLAIR",
}
local BACKGROUND_FALLBACK = "Interface\\LFGFrame\\UI-LFG-BACKGROUND-GENERICDUNGEON"
-- Bosses of the board's own, fought somewhere in a raid rather than as its boss there: where, under the boss's name
-- (L'Infini, mod-stat-growth InfiniteGod.cpp: Ulduar's Celestial Planetarium)
local PLACE_BY_BOSS = {
    [930000] = GetLocale() == "frFR" and "Ulduar - Planétarium céleste" or "Ulduar - Celestial Planetarium",
    [VOICE_BOSS] = TEXT.voicePlace,
}

local function DungeonTexture(dungeonId, kind)
    local textureName = dungeonId and dungeonId > 0 and select(10, GetLFGDungeonInfo(dungeonId))
    if textureName and textureName ~= "" then
        return "Interface\\LFGFrame\\" .. kind .. textureName
    end
    return "Interface\\LFGFrame\\" .. kind .. "Raid"
end

-- The raid's art; a raid without any gets a generic piece rather than an empty card
local function SetBackground(texture, dungeonId)
    local textureName = dungeonId and dungeonId > 0 and select(10, GetLFGDungeonInfo(dungeonId))
    if BACKGROUND_BY_DUNGEON[dungeonId] then
        texture:SetTexture(BACKGROUND_BY_DUNGEON[dungeonId])
    elseif textureName and textureName ~= "" then
        texture:SetTexture(DungeonTexture(dungeonId, "UI-LFG-BACKGROUND-"))
    else
        texture:SetTexture(BACKGROUND_FALLBACK)
    end
end

local function FormatTime(seconds)
    seconds = max(0, floor(seconds or 0))
    return format("%d:%02d", floor(seconds / 60), seconds % 60)
end

local function Money(copper)
    return GetCoinTextureString(copper or 0)
end

-- 3.3.5 has neither SetShown nor SetEnabled
local function SetShown(region, shown)
    if shown then
        region:Show()
    else
        region:Hide()
    end
end

local function SetEnabled(button, enabled)
    if enabled then
        button:Enable()
    else
        button:Disable()
    end
end

local function SetAtlas(texture, name)
    local atlas = RetailUIAtlas[name]
    texture:SetTexture(atlas[1])
    texture:SetTexCoord(atlas[4], atlas[5], atlas[6], atlas[7])
end

-- A decimal the way the player's language writes it
local function Decimal(value)
    local text = format("%.2f", value):gsub("0+$", ""):gsub("%.$", "")
    return french and text:gsub("%.", ",") or text
end

local function TierName(tier)
    return format(TEXT.tier, TIER_ROMAN[tier] or tostring(tier))
end

-- What a mission pays at a tier: the server sends Défi I's
local function MissionReward(mission, tier)
    return floor(mission.gold * TierGoldPercent(tier) / 100), mission.paragon + TierExtraParagon(tier),
        mission.essences + TierExtraEssences(tier)
end

local function IsGod(mission)
    return mission and mission.boss == GOD_BOSS
end

local function IsVoice(mission)
    return mission and mission.boss == VOICE_BOSS
end

-- A boss with a page of its own rather than a card
local function HasOwnPage(mission)
    return IsGod(mission) or IsVoice(mission)
end

-- The god's tier dial: the tier picked, never above the highest open
local function GodTier()
    state.godTier = min(state.godTier or state.godOpenTier, state.godOpenTier)
    return state.godTier
end

-- The tier a card shows: the one it was won at, the challenge's own while under way, or the dial's (the god's own)
local function CardTier(mission)
    if mission.wonAt and mission.wonAt > 0 then
        return mission.wonAt
    elseif mission.state == STATE_UNDERWAY and state.currentTier > 0 then
        return state.currentTier
    elseif IsGod(mission) then
        return GodTier()
    elseif IsVoice(mission) then
        return TIER_MIN
    end
    return state.tier or state.openTier
end

local function GodMission()
    for _, mission in ipairs(state.missions) do
        if IsGod(mission) then
            return mission
        end
    end
end

local function VoiceMission()
    for _, mission in ipairs(state.missions) do
        if IsVoice(mission) then
            return mission
        end
    end
end

-- Whether the player wears the equipped item level a mission asks to sign up (unknown: the server decides)
local function MeetsItemLevel(mission)
    local wanted = mission and mission.signUpItemLevel or 0
    return wanted == 0 or not state.itemLevel or state.itemLevel >= wanted
end

-- Tweens: every animation of the board and the banner, driven by one clock ---------------------------------------

local tweens = {}
local driver = CreateFrame("Frame")

local function Tween(duration, delay, update, done)
    tinsert(tweens, { time = -(delay or 0), duration = duration, update = update, done = done })
end

local function OutCubic(p)
    local inverse = 1 - p
    return 1 - inverse * inverse * inverse
end

local function OutBack(p)
    local c = 1.70158
    local q = p - 1
    return 1 + (c + 1) * q * q * q + c * q * q
end

driver:SetScript("OnUpdate", function(_, elapsed)
    for index = #tweens, 1, -1 do
        local tween = tweens[index]
        tween.time = tween.time + elapsed
        if tween.time >= 0 then
            local progress = min(1, tween.time / tween.duration)
            tween.update(progress)
            if progress >= 1 then
                tremove(tweens, index)
                if tween.done then
                    tween.done()
                end
            end
        end
    end
end)

-- Roles -------------------------------------------------------------------------------------------------------------

local function RolesMask()
    local _, tank, healer, damage = GetLFGRoles()
    return (tank and ROLE_TANK or 0) + (healer and ROLE_HEALER or 0) + (damage and ROLE_DAMAGE or 0)
end

local function RefreshRoles()
    if not roleButtons then
        return
    end

    local _, tank, healer, damage = GetLFGRoles()
    local chosen = { TANK = tank, HEALER = healer, DAMAGER = damage }
    local canTank, canHeal, canDamage = true, true, true
    if GetAvailableRoles then
        canTank, canHeal, canDamage = GetAvailableRoles()
    end
    local available = { TANK = canTank, HEALER = canHeal, DAMAGER = canDamage }

    for role, button in pairs(roleButtons) do
        local on = chosen[role] and available[role]
        button.icon:SetDesaturated(not on)
        button.icon:SetAlpha(available[role] and (on and 1 or 0.45) or 0.15)
        SetShown(button.ring, on and true or false)
        SetEnabled(button, available[role] and true or false)
    end
end

local function ToggleRole(role)
    local leader, tank, healer, damage = GetLFGRoles()
    if role == "TANK" then
        tank = not tank
    elseif role == "HEALER" then
        healer = not healer
    else
        damage = not damage
    end
    SetLFGRoles(leader, tank, healer, damage)
    PlaySound(RolesMask() > 0 and "igMainMenuOptionCheckBoxOn" or "igMainMenuOptionCheckBoxOff")
    RefreshRoles()
end

-- Messages under the cards ------------------------------------------------------------------------------------------

local function ShowError(code, value, detail)
    code = tonumber(code)
    local text = TEXT.errors[code] or TEXT.errors[10]
    -- The equipped item level a sign-up asks for: the player's own (16), or a party member's (17)
    if code == 16 then
        text = format(text, tonumber(value) or 0, tonumber(detail) or 0)
    elseif code == 17 then
        text = format(text, detail or "?", tonumber(value) or 0)
    end
    PlaySound("igQuestFailed")
    UIErrorsFrame:AddMessage(text, 1, 0.1, 0.1, 1)
    if errorText then
        errorText:SetText(text)
        errorText:SetAlpha(1)
        Tween(0.6, 3, function(p) errorText:SetAlpha(1 - p) end)
    end
end

-- Cards -------------------------------------------------------------------------------------------------------------

local function CardTooltipSatchel(owner)
    GameTooltip:SetOwner(owner, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. SATCHEL)
    GameTooltip:AddLine(TEXT.satchelHint, 1, 0.9, 0.7, true)
    local mission = owner:GetParent().mission
    if mission and mission.kind == KIND_DUNGEON then
        GameTooltip:AddLine(TEXT.satchelDungeon, 1, 0.82, 0.3, true)
    elseif IsVoice(mission) then
        -- Its bosses have no loot table: the satchel holds the gold alone
        GameTooltip:AddLine(TEXT.satchelVoice, 1, 0.82, 0.3, true)
    elseif mission then
        local tier = CardTier(mission)
        local chance = min(100, (SATCHEL_ITEM_CHANCE[mission.difficulty] or 35) + TierExtraItemChance(tier))
        GameTooltip:AddLine(tier >= TIER_GUARANTEED_ITEM and format(TEXT.satchelSure, chance) or
            format(TEXT.satchelChance, chance), 1, 0.82, 0.3, true)
        if tier > TIER_MIN and (mission.itemLevel or 0) > 0 and not IsGod(mission) then
            GameTooltip:AddLine(format(TEXT.satchelTierGear, TierItemLevel(mission.itemLevel, tier)),
                0.75, 0.45, 1, true)
        end
    end
    GameTooltip:Show()
end

local function CreateCard(index)
    local card = CreateFrame("Frame", nil, frame)
    card:SetSize(CARD_WIDTH, CARD_HEIGHT)
    card:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    card:SetBackdropColor(0.04, 0.03, 0.02, 0.92)
    card:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)
    card:EnableMouse(true)

    -- A reward waiting: a warm glow breathes behind the card
    local glow = frame:CreateTexture(nil, "BORDER")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetBlendMode("ADD")
    glow:SetPoint("TOPLEFT", card, "TOPLEFT", -34, 34)
    glow:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", 34, -34)
    glow:Hide()
    card.glow = glow

    local art = card:CreateTexture(nil, "ARTWORK")
    art:SetPoint("TOPLEFT", 5, -5)
    art:SetPoint("TOPRIGHT", -5, -5)
    art:SetHeight(110)
    art:SetTexCoord(0, 0.640625, 0, 0.8)
    card.art = art

    local shade = card:CreateTexture(nil, "ARTWORK", nil, 1)
    shade:SetTexture("Interface\\Buttons\\WHITE8X8")
    shade:SetPoint("BOTTOMLEFT", art, "BOTTOMLEFT")
    shade:SetPoint("BOTTOMRIGHT", art, "BOTTOMRIGHT")
    shade:SetHeight(56)
    shade:SetGradientAlpha("VERTICAL", 0.04, 0.03, 0.02, 1, 0.04, 0.03, 0.02, 0)

    local kind = card:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    kind:SetPoint("TOPLEFT", art, "TOPLEFT", 8, -7)
    kind:SetText(TEXT.challenge)
    kind:SetTextColor(1, 0.82, 0.3)
    kind:SetShadowOffset(1, -1)
    card.kind = kind

    -- The raid's icon, straddling the bottom of the art in a thin gold frame
    local iconFrame = CreateFrame("Frame", nil, card)
    iconFrame:SetSize(44, 44)
    iconFrame:SetPoint("CENTER", art, "BOTTOMLEFT", 30, 2)
    iconFrame:SetBackdrop({
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 10,
    })
    iconFrame:SetBackdropBorderColor(0.85, 0.7, 0.4, 1)
    local icon = iconFrame:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 3, -3)
    icon:SetPoint("BOTTOMRIGHT", -3, 3)
    icon:SetTexCoord(0.06, 0.94, 0.06, 0.94)
    card.icon = icon

    local name = card:CreateFontString(nil, "OVERLAY")
    name:SetFont(MORPHEUS, 17)
    name:SetShadowOffset(1, -1)
    name:SetTextColor(1, 0.9, 0.7)
    name:SetPoint("TOPLEFT", art, "BOTTOMLEFT", 8, -24)
    name:SetPoint("TOPRIGHT", art, "BOTTOMRIGHT", -8, -24)
    name:SetJustifyH("LEFT")
    name:SetHeight(38)
    name:SetJustifyV("TOP")
    card.name = name

    local raid = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    raid:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -2)
    raid:SetPoint("TOPRIGHT", name, "BOTTOMRIGHT", 0, -2)
    raid:SetJustifyH("LEFT")
    raid:SetTextColor(0.75, 0.7, 0.6)
    card.raid = raid

    local size = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    size:SetPoint("TOPLEFT", raid, "BOTTOMLEFT", 0, -3)
    size:SetJustifyH("LEFT")
    card.size = size

    -- What the boss drops in this mode, in the epic colour, as one more line about the mission
    local itemLevel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    itemLevel:SetPoint("TOPLEFT", size, "BOTTOMLEFT", 0, -3)
    itemLevel:SetJustifyH("LEFT")
    itemLevel:SetTextColor(0.75, 0.45, 1)
    card.itemLevel = itemLevel

    local divider = card:CreateTexture(nil, "ARTWORK")
    SetAtlas(divider, "ChallengeMode-ThinDivider")
    divider:SetHeight(10)
    divider:SetPoint("TOPLEFT", itemLevel, "BOTTOMLEFT", -6, -6)
    divider:SetPoint("RIGHT", card, "RIGHT", -8, 0)

    local rewardLabel = card:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    rewardLabel:SetPoint("TOPLEFT", divider, "BOTTOMLEFT", 6, -4)
    rewardLabel:SetText(TEXT.rewards)

    local gold = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    gold:SetPoint("TOPLEFT", rewardLabel, "BOTTOMLEFT", 0, -8)
    card.gold = gold

    -- Paragon points, when the mission carries some: between the gold and the satchel, in the paragon purple
    local paragon = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    paragon:SetPoint("RIGHT", card, "RIGHT", -52, 0)
    paragon:SetPoint("TOP", gold, "TOP", 0, -1)
    paragon:SetJustifyH("RIGHT")
    paragon:SetTextColor(0.64, 0.21, 0.93)
    card.paragon = paragon

    -- Essences, under the paragon points, in the essences' green
    local essences = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    essences:SetPoint("TOPRIGHT", paragon, "BOTTOMRIGHT", 0, -2)
    essences:SetJustifyH("RIGHT")
    essences:SetTextColor(0.3, 1, 0.45)
    card.essences = essences

    local satchel = CreateFrame("Button", nil, card)
    satchel:SetSize(30, 30)
    satchel:SetPoint("RIGHT", card, "RIGHT", -14, 0)
    satchel:SetPoint("TOP", rewardLabel, "BOTTOM", 0, -2)
    local satchelIcon = satchel:CreateTexture(nil, "ARTWORK")
    satchelIcon:SetAllPoints()
    satchelIcon:SetTexture(SATCHEL_ICON)
    local satchelBorder = satchel:CreateTexture(nil, "OVERLAY")
    satchelBorder:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    satchelBorder:SetBlendMode("ADD")
    satchelBorder:SetVertexColor(0.64, 0.21, 0.93)
    satchelBorder:SetPoint("TOPLEFT", -13, 13)
    satchelBorder:SetPoint("BOTTOMRIGHT", 13, -13)
    satchel:SetScript("OnEnter", CardTooltipSatchel)
    satchel:SetScript("OnLeave", function() GameTooltip:Hide() end)
    card.satchel = satchel
    card.satchelIcon = satchelIcon

    local button = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
    button:SetSize(CARD_WIDTH - 28, 24)
    button:SetPoint("BOTTOM", card, "BOTTOM", 0, 14)
    card.button = button

    local status = card:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    status:SetPoint("BOTTOM", card, "BOTTOM", 0, 20)
    card.status = status

    local check = card:CreateTexture(nil, "OVERLAY", nil, 3)
    SetAtlas(check, "ui-questtracker-tracker-check-2x")
    check:SetSize(46, 46)
    check:SetPoint("CENTER", art, "CENTER", 0, 0)
    check:Hide()
    card.check = check

    -- Hovering an open mission lights the card
    local highlight = card:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    highlight:SetBlendMode("ADD")
    highlight:SetPoint("TOPLEFT", 4, -4)
    highlight:SetPoint("BOTTOMRIGHT", -4, 4)
    highlight:SetAlpha(0.18)

    card:SetScript("OnEnter", function(self)
        if self.mission and self.mission.state == STATE_OPEN then
            PlaySound("GAMESCREENSMALLBUTTONMOUSEOVER")
        end
    end)

    -- The stamp shown when a challenge is accepted, and the reward popping out when it is claimed
    local stamp = card:CreateFontString(nil, "OVERLAY")
    stamp:SetFont(MORPHEUS, 22, "OUTLINE")
    stamp:SetTextColor(1, 0.82, 0.2)
    stamp:SetPoint("CENTER", art, "CENTER", 0, 0)
    stamp:SetAlpha(0)
    card.stamp = stamp

    local burst = card:CreateTexture(nil, "OVERLAY", nil, 4)
    SetAtlas(burst, "ChallengeMode-SpikeyStar")
    burst:SetBlendMode("ADD")
    burst:SetPoint("CENTER", satchel, "CENTER")
    burst:SetAlpha(0)
    card.burst = burst

    local popIcon = card:CreateTexture(nil, "OVERLAY", nil, 5)
    popIcon:SetTexture(SATCHEL_ICON)
    popIcon:SetPoint("CENTER", satchel, "CENTER")
    popIcon:SetAlpha(0)
    card.popIcon = popIcon

    local popText = card:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    popText:SetPoint("BOTTOM", satchel, "TOP", 0, 6)
    popText:SetAlpha(0)
    card.popText = popText

    card.index = index
    card:Hide()
    return card
end

local function LayoutCards(count, set)
    local total = count * CARD_WIDTH + max(0, count - 1) * CARD_GAP
    local left = (WIDTH - total) / 2
    for index, card in ipairs(set or cards) do
        card.x = left + (index - 1) * (CARD_WIDTH + CARD_GAP)
        card.y = -162
        card:ClearAllPoints()
        card:SetPoint("TOPLEFT", frame, "TOPLEFT", card.x, card.y)
    end
end

local function FillCard(card, mission)
    card.mission = mission
    SetBackground(card.art, mission.dungeon)
    card.icon:SetTexture(DungeonTexture(mission.dungeon, "LFGIcon-"))
    card.name:SetText(mission.name)
    card.raid:SetText(PLACE_BY_BOSS[mission.boss] or DungeonName(mission.dungeon))
    local heroic = mission.difficulty >= 2
    local tier = CardTier(mission)
    card.size:SetText(format(TEXT.players, mission.players, heroic and TEXT.heroic or TEXT.normal))
    local kind = TEXT.challenge .. " " .. (TIER_ROMAN[tier] or "")
    card.kind:SetText(heroic and (kind .. "  |cffff8000" .. TEXT.heroicTag .. "|r") or kind)
    if (mission.requiredItemLevel or 0) > 0 then
        -- A boss of the board's own, tuned on its own gear: what it asks, at the card's tier
        card.itemLevel:SetText(format(TEXT.required, mission.requiredItemLevel,
            (mission.baseParagon or 0) + TierParagon(tier)))
    else
        card.itemLevel:SetText(mission.itemLevel <= 0 and "" or tier > TIER_MIN and
            format(TEXT.tierItemLevel, mission.itemLevel, TierItemLevel(mission.itemLevel, tier)) or
            format(TEXT.itemLevel, mission.itemLevel))
    end
    local gold, paragon, essences = MissionReward(mission, tier)
    card.gold:SetText(Money(gold))
    card.paragon:SetText(paragon > 0 and format(TEXT.paragon, paragon) or "")
    card.essences:SetText(essences > 0 and format(TEXT.essences, essences) or "")

    local done = mission.state == STATE_CLAIMED
    card.art:SetDesaturated(done)
    card.icon:SetDesaturated(done)
    SetShown(card.check, done)
    SetShown(card.glow, mission.state == STATE_WON)
    card:SetBackdropBorderColor(unpack(mission.state == STATE_WON and { 1, 0.82, 0.25, 1 } or
        mission.state == STATE_UNDERWAY and { 0.35, 0.65, 1, 1 } or { 0.75, 0.6, 0.35, 1 }))

    local button = card.button
    button:SetScript("OnClick", nil)
    card.status:SetText("")
    if mission.state == STATE_OPEN then
        button:Show()
        button:SetText(TEXT.take)
        SetEnabled(button, state.challenge == 0)
        button:SetScript("OnClick", function()
            local roles = RolesMask()
            if roles == 0 then
                return ShowError(8)
            end
            PlaySound("igMainMenuOptionCheckBoxOn")
            state.starting = mission.boss
            Send("START\t" .. mission.boss .. "\t" .. roles .. "\t" .. (state.tier or state.openTier))
        end)
    elseif mission.state == STATE_WON then
        button:Show()
        button:SetText(TEXT.claim)
        SetEnabled(button, true)
        button:SetScript("OnClick", function()
            Send("CLAIM\t" .. state.rotation .. "\t" .. mission.boss)
        end)
    else
        button:Hide()
        card.status:SetText(mission.state == STATE_UNDERWAY and TEXT.underway or TEXT.claimed)
        if mission.state == STATE_UNDERWAY then
            card.status:SetTextColor(0.45, 0.75, 1)
        else
            card.status:SetTextColor(1, 0.86, 0.55)
        end
    end
end

-- A dungeon challenge on a card: the dungeon's art, the key it runs at and its time, the reward at that key
local function FillDungeonCard(card, dungeon)
    card.mission = dungeon
    SetBackground(card.art, dungeon.dungeon)
    card.icon:SetTexture(DungeonTexture(dungeon.dungeon, "LFGIcon-"))
    card.name:SetText(DungeonName(dungeon.dungeon))
    card.kind:SetText(TEXT.dungeonTag .. "  |cffff8000" .. TEXT.mythicTag .. "|r")
    if dungeon.wonLevel > 0 then
        card.raid:SetText(format(dungeon.wonTimed and TEXT.wonTimed or TEXT.wonLate, dungeon.wonLevel))
    else
        card.raid:SetText(format(TEXT.dungeonKey, state.key))
    end
    card.size:SetText(format(TEXT.dungeonTime, floor(dungeon.limit / 60)))
    card.itemLevel:SetText("")
    card.gold:SetText(Money(dungeon.gold))
    card.paragon:SetText(dungeon.paragon > 0 and format(TEXT.paragon, dungeon.paragon) or "")
    card.essences:SetText(dungeon.essences > 0 and format(TEXT.essences, dungeon.essences) or "")

    local done = dungeon.state == STATE_CLAIMED
    card.art:SetDesaturated(done)
    card.icon:SetDesaturated(done)
    SetShown(card.check, done)
    SetShown(card.glow, dungeon.state == STATE_WON)
    card:SetBackdropBorderColor(unpack(dungeon.state == STATE_WON and { 1, 0.82, 0.25, 1 } or
        dungeon.state == STATE_UNDERWAY and { 1, 0.86, 0.55, 1 } or { 0.75, 0.6, 0.35, 1 }))

    local button = card.button
    button:SetScript("OnClick", nil)
    card.status:SetText("")
    if dungeon.state == STATE_OPEN then
        button:Show()
        button:SetText(TEXT.launch)
        SetEnabled(button, state.challenge == 0)
        button:SetScript("OnClick", function()
            local roles = RolesMask()
            if roles == 0 then
                return ShowError(8)
            end
            PlaySound("igMainMenuOptionCheckBoxOn")
            state.startingDungeon = dungeon.dungeon
            Send("DUNGEON\t" .. dungeon.dungeon .. "\t" .. roles)
        end)
    elseif dungeon.state == STATE_WON then
        button:Show()
        button:SetText(TEXT.claim)
        SetEnabled(button, true)
        button:SetScript("OnClick", function()
            Send("CLAIM\t" .. state.rotation .. "\t" .. dungeon.dungeon)
        end)
    else
        button:Hide()
        card.status:SetText(dungeon.state == STATE_UNDERWAY and TEXT.underway or TEXT.claimed)
        card.status:SetTextColor(1, 0.86, 0.55)
    end
end

-- Cards drop in one after the other: each falls a little and fades in
local function AnimateCardsIn()
    if state.page == "god" then
        godPage.Enter(true)
        return
    elseif state.page == "voice" then
        voicePage.Enter(true)
        return
    end
    for index, card in ipairs(state.page == "dungeons" and dungeonCards or cards) do
        if card.mission then
            card:SetAlpha(0)
            Tween(0.42, (index - 1) * 0.08, function(p)
                local eased = OutBack(p)
                card:SetAlpha(min(1, p * 1.6))
                card:SetPoint("TOPLEFT", frame, "TOPLEFT", card.x, card.y + 26 * (1 - eased))
            end)
        end
    end
end

local function Refresh(animate)
    if not frame then
        return
    end

    local dungeonsPage = state.page == "dungeons"
    local godShown = state.page == "god"
    local voiceShown = state.page == "voice"
    -- The god and the Hollow Voice have their pages: the raid cards are the drawn missions
    local raidMissions = {}
    for _, mission in ipairs(state.missions) do
        if not HasOwnPage(mission) then
            tinsert(raidMissions, mission)
        end
    end
    local shown = min(#raidMissions, #cards)
    LayoutCards(max(shown, 1), cards)
    for index, card in ipairs(cards) do
        local mission = raidMissions[index]
        if mission and state.page == "raids" then
            FillCard(card, mission)
            card:Show()
        else
            card.mission = mission and card.mission or nil
            card:Hide()
        end
    end

    local shownDungeons = min(#state.dungeons, #dungeonCards)
    LayoutCards(max(shownDungeons, 1), dungeonCards)
    for index, card in ipairs(dungeonCards) do
        local dungeon = state.dungeons[index]
        if dungeon and dungeonsPage then
            FillDungeonCard(card, dungeon)
            card:Show()
        else
            card:Hide()
        end
    end

    SetShown(dial, state.page == "raids" and state.bracket ~= 0)
    SetShown(keyStrip, dungeonsPage and shownDungeons > 0)
    if keyStrip:IsShown() then
        keyStrip.Refresh()
    end
    -- A page's scene in place of the board's parchment and header, or the board back
    local god, voice = GodMission(), VoiceMission()
    godPage.SetScene(godShown)
    voicePage.SetScene(voiceShown)
    for _, region in ipairs(frame.boardArt) do
        SetShown(region, not (godShown or voiceShown))
    end
    SetShown(godPage, godShown and god ~= nil)
    if godPage:IsShown() then
        godPage.Refresh(god)
    end
    SetShown(voicePage, voiceShown and voice ~= nil)
    if voicePage:IsShown() then
        voicePage.Refresh(voice)
    end

    if godShown and not god then
        emptyText:SetText(state.bracket < 80 and TEXT.godLocked or TEXT.empty)
        emptyText:Show()
    elseif voiceShown and not voice then
        emptyText:SetText(state.bracket < 80 and TEXT.voiceAbsent or TEXT.empty)
        emptyText:Show()
    elseif godShown or voiceShown then
        emptyText:Hide()
    elseif dungeonsPage and shownDungeons == 0 then
        emptyText:SetText(state.bracket < 80 and TEXT.dungeonsLocked or TEXT.empty)
        emptyText:Show()
    elseif not dungeonsPage and state.bracket == 0 then
        emptyText:SetText(TEXT.locked)
        emptyText:Show()
    elseif not dungeonsPage and shown == 0 then
        emptyText:SetText(TEXT.empty)
        emptyText:Show()
    else
        emptyText:Hide()
    end

    -- Rewards won on another board
    for index, row in ipairs(rewardRows) do
        local reward = state.rewards[index]
        if reward then
            row.icon:SetTexture(DungeonTexture(reward.dungeon, "LFGIcon-"))
            local name, grade = reward.name, TIER_ROMAN[reward.tier] or "I"
            if reward.kind == KIND_DUNGEON then
                name, grade = DungeonName(reward.dungeon), "+" .. reward.level
            end
            row.text:SetText(name .. " |cffffd24d" .. grade .. "|r  " ..
                Money(reward.gold) .. (reward.paragon > 0 and
                ("  |cffa335ee" .. format(TEXT.paragon, reward.paragon) .. "|r") or "") .. (reward.essences > 0 and
                ("  |cff4dff73" .. format(TEXT.essences, reward.essences) .. "|r") or ""))
            row.button:SetScript("OnClick", function()
                Send("CLAIM\t" .. reward.rotation .. "\t" .. reward.boss)
            end)
            row:Show()
        else
            row:Hide()
        end
    end
    SetShown(rewardRows.label, #state.rewards > 0)

    SetShown(quitButton, state.challenge ~= 0)
    RefreshRoles()
    if dial then
        dial.Refresh()
    end

    if animate then
        AnimateCardsIn()
    end
end

-- The board's own pages, L'Infini's and the Hollow Voice's: one scene filling the window. The board's parchment and
-- header give way to the boss's place (the Celestial Planetarium, the Sunwell's chamber), the boss standing on its left
-- and melting into it (its edges faded in its texture), its name at its feet; down the right, the page's title, its
-- tier dial (L'Infini) or its one difficulty (the Hollow Voice), its words and its story, then what it asks and pays
-- and the way in. The footer (roles, rewards waiting) stays, drawn over the scene (buildChallengeGodArt.py for the
-- art). Opening it and taking it up have sounds of their own, short effects, never music.

-- The window's inside (ChallengeBoard ground: 2 in from the sides, under the 21 of the title bar) and the boss in it
local GOD_SCENE_LEFT, GOD_SCENE_TOP = 2, 21
local GOD_FIGURE_WIDTH = 444
local GOD_COLUMN_LEFT, GOD_COLUMN_RIGHT = 424, 26
-- Ulduar's cosmic chest opening, for stepping in; Sunwell's gate opening for the Hollow Voice's
local GOD_OPEN_SOUND = "Sound\\Doodad\\UL_Chest_Cosmic_Open.wav"
local VOICE_OPEN_SOUND = "Sound\\Doodad\\SunwellRaid_Gate_02Open.wav"
-- What sets the two pages apart. The god: its tier ladder and its gear, its name the server's. The Hollow Voice: one
-- difficulty, no gear of its own, a sign-up locked under an equipped item level, its name the page's (the server's is
-- the Archbishop's, the boss the challenge follows).
local PAGE_SPECS = {
    god = {
        boss = GOD_BOSS, art = "ChallengeGod", figureBottom = GOD_FIGURE_BOTTOM, sound = GOD_OPEN_SOUND,
        nameSize = 46, heading = TEXT.headingGod, intro = TEXT.introGod, epithet = TEXT.godEpithet,
        quote = TEXT.godQuote, signature = TEXT.godSignature, lore = TEXT.godLore, face = TEXT.godFace,
        once = TEXT.godOnce, tiers = true,
    },
    voice = {
        boss = VOICE_BOSS, art = "ChallengeVoice", figureBottom = VOICE_FIGURE_BOTTOM, sound = VOICE_OPEN_SOUND,
        name = TEXT.voiceName, nameSize = 40, heading = TEXT.headingVoice, intro = TEXT.introVoice,
        epithet = TEXT.voiceEpithet, quote = TEXT.voiceQuote, signature = TEXT.voiceSignature, lore = TEXT.voiceLore,
        face = TEXT.voiceFace, once = TEXT.voiceOnce, tiers = false,
    },
}

-- A line of text with the board's shadow, so it reads on the painting
local function GodText(parent, font, size, r, g, b)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(font, size)
    text:SetTextColor(r, g, b)
    text:SetShadowColor(0, 0, 0, 1)
    text:SetShadowOffset(1, -1)
    return text
end

-- The scene itself: textures of the window in place of its ground and parchment (hidden with them), under everything
-- the window draws on top (the footer's labels and buttons, its border). One draw layer each: the client ignores a
-- texture's sublevel, so the backdrop, the boss and the shade take BACKGROUND, BORDER and ARTWORK (the window's own
-- ARTWORK, the header's divider, is hidden with the header).
local function CreateGodScene(spec)
    local scene = {}
    local function Layer(layer)
        local texture = frame:CreateTexture(nil, layer)
        texture:Hide()
        tinsert(scene, texture)
        return texture
    end

    local backdrop = Layer("BACKGROUND")
    scene.backdrop = backdrop
    backdrop:SetTexture(GOD_ART .. spec.art .. "-Backdrop")
    backdrop:SetPoint("TOPLEFT", frame, "TOPLEFT", GOD_SCENE_LEFT, -GOD_SCENE_TOP)
    backdrop:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -GOD_SCENE_LEFT, GOD_SCENE_LEFT)

    local figure = Layer("BORDER")
    figure:SetTexture(GOD_ART .. spec.art .. "-Figure")
    figure:SetTexCoord(0, 1, 0, spec.figureBottom)
    figure:SetPoint("TOPLEFT", frame, "TOPLEFT", GOD_SCENE_LEFT, -GOD_SCENE_TOP)
    figure:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", GOD_SCENE_LEFT, GOD_SCENE_LEFT)
    figure:SetWidth(GOD_FIGURE_WIDTH)
    scene.figure = figure

    -- Shade: the text column a little darker, the footer and the title bar's edge darker still
    local column = Layer("ARTWORK")
    column:SetTexture("Interface\\Buttons\\WHITE8X8")
    column:SetPoint("TOPLEFT", frame, "TOPLEFT", GOD_COLUMN_LEFT - 60, -GOD_SCENE_TOP)
    column:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -GOD_SCENE_LEFT, GOD_SCENE_LEFT)
    column:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.55)
    local foot = Layer("ARTWORK")
    foot:SetTexture("Interface\\Buttons\\WHITE8X8")
    foot:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", GOD_SCENE_LEFT, GOD_SCENE_LEFT)
    foot:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -GOD_SCENE_LEFT, GOD_SCENE_LEFT)
    foot:SetHeight(150)
    foot:SetGradientAlpha("VERTICAL", 0, 0, 0, 0.8, 0, 0, 0, 0)
    local head = Layer("ARTWORK")
    head:SetTexture("Interface\\Buttons\\WHITE8X8")
    head:SetPoint("TOPLEFT", frame, "TOPLEFT", GOD_SCENE_LEFT, -GOD_SCENE_TOP)
    head:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -GOD_SCENE_LEFT, -GOD_SCENE_TOP)
    head:SetHeight(60)
    head:SetGradientAlpha("VERTICAL", 0, 0, 0, 0, 0, 0, 0, 0.6)

    -- A soft gold light that swells when the challenge is taken up (never a flash: it stays dim)
    local swell = Layer("ARTWORK")
    SetAtlas(swell, "ChallengeMode-SoftYellowGlow")
    swell:SetBlendMode("ADD")
    swell:SetPoint("CENTER", figure, "CENTER", 0, 40)
    swell:SetSize(520, 520)
    swell:SetAlpha(0)
    scene.swell = swell
    return scene
end

local function CreateGodPage(spec)
    local scene = CreateGodScene(spec)
    local boss = spec.boss

    local page = CreateFrame("Frame", nil, frame)
    page:SetPoint("TOPLEFT", frame, "TOPLEFT", GOD_SCENE_LEFT, -GOD_SCENE_TOP)
    page:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -GOD_SCENE_LEFT, GOD_SCENE_LEFT)
    page:Hide()
    page.scene = scene

    -- The tier the page shows and takes up: the god's dial, the Hollow Voice's one difficulty
    local function PageTier()
        return spec.tiers and GodTier() or TIER_MIN
    end

    -- At the boss's feet: its name
    local kind = GodText(page, FRIZ, 11, 1, 0.82, 0.3)
    kind:SetPoint("TOPLEFT", page, "TOPLEFT", 22, -18)

    local name = GodText(page, MORPHEUS, spec.nameSize, 1, 0.9, 0.7)
    name:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 30, 128)

    local epithet = GodText(page, FRIZ, 12, 1, 0.82, 0.3)
    epithet:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 4, -2)
    epithet:SetText(spec.epithet)

    local place = GodText(page, FRIZ, 11, 0.78, 0.74, 0.66)
    place:SetPoint("TOPLEFT", epithet, "BOTTOMLEFT", 0, -3)

    local check = page:CreateTexture(nil, "OVERLAY", nil, 3)
    SetAtlas(check, "ui-questtracker-tracker-check-2x")
    check:SetSize(72, 72)
    check:SetPoint("CENTER", scene.figure, "CENTER", 0, 40)
    check:Hide()

    local stamp = page:CreateFontString(nil, "OVERLAY")
    stamp:SetFont(MORPHEUS, 30, "OUTLINE")
    stamp:SetTextColor(1, 0.82, 0.2)
    stamp:SetPoint("CENTER", scene.figure, "CENTER", 0, 40)
    stamp:SetAlpha(0)

    -- Down the right: title, dial, words, story, then what it asks and pays
    local column = CreateFrame("Frame", nil, page)
    column:SetPoint("TOPLEFT", page, "TOPLEFT", GOD_COLUMN_LEFT, 0)
    column:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", -GOD_COLUMN_RIGHT, 0)
    local columnWidth = WIDTH - 2 * GOD_SCENE_LEFT - GOD_COLUMN_LEFT - GOD_COLUMN_RIGHT

    local title = GodText(column, MORPHEUS, 30, 1, 0.86, 0.55)
    title:SetPoint("TOP", column, "TOP", 0, -20)
    title:SetText(spec.heading)

    local subtitle = GodText(column, FRIZ, 11, 0.85, 0.8, 0.7)
    subtitle:SetPoint("TOP", title, "BOTTOM", 0, -6)
    subtitle:SetWidth(columnWidth - 10)
    subtitle:SetJustifyH("CENTER")
    subtitle:SetSpacing(2)
    subtitle:SetText(spec.intro)

    local function Divider(y)
        local divider = column:CreateTexture(nil, "ARTWORK")
        SetAtlas(divider, "ChallengeMode-ThinDivider")
        divider:SetHeight(10)
        divider:SetPoint("TOPLEFT", column, "TOPLEFT", 0, y)
        divider:SetPoint("TOPRIGHT", column, "TOPRIGHT", 0, y)
    end
    Divider(-96)

    -- Its tier dial (its own ladder), or its one difficulty and the profile it is made for
    local dialY = -128
    local dialGlow = column:CreateTexture(nil, "BACKGROUND")
    SetAtlas(dialGlow, "ChallengeMode-SoftYellowGlow")
    dialGlow:SetBlendMode("ADD")
    dialGlow:SetPoint("CENTER", column, "TOP", 0, dialY)
    dialGlow:SetSize(230, 60)

    local tierText = GodText(column, MORPHEUS, 26, 1, 0.86, 0.55)
    tierText:SetPoint("CENTER", column, "TOP", 0, dialY)

    local tierInfo = GodText(column, FRIZ, 11, 0.85, 0.8, 0.7)
    tierInfo:SetPoint("TOP", tierText, "BOTTOM", 0, -4)

    local dialArea = CreateFrame("Frame", nil, column)
    dialArea:SetPoint("CENTER", column, "TOP", 0, dialY - 8)
    dialArea:SetSize(170, 52)
    dialArea:EnableMouse(true)
    dialArea:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_BOTTOM")
        if not spec.tiers then
            local mission = page.rewards.mission
            GameTooltip:AddLine(TEXT.voiceTierTitle, 1, 0.82, 0.3)
            GameTooltip:AddLine(format(TEXT.voiceTierHelp, mission and mission.players or 10,
                mission and mission.requiredItemLevel or 0, mission and mission.baseParagon or 0), 1, 0.9, 0.7, true)
            GameTooltip:Show()
            return
        end
        local tier = GodTier()
        GameTooltip:AddLine(TEXT.godTierTitle, 1, 0.82, 0.3)
        GameTooltip:AddLine(TEXT.godTierHelp, 1, 0.9, 0.7, true)
        GameTooltip:AddLine(" ")
        GameTooltip:AddLine(TierName(tier), 1, 0.86, 0.55)
        GameTooltip:AddLine(format(TEXT.tierReward, TierGoldPercent(tier) - 100, TierExtraEssences(tier),
            TierExtraParagon(tier)), 0.85, 0.8, 0.7, true)
        GameTooltip:AddLine(" ")
        if state.godOpenTier >= TIER_MAX then
            GameTooltip:AddLine(TEXT.tierTop, 0.62, 0.57, 0.5, true)
        else
            GameTooltip:AddLine(format(TEXT.godTierNext, TIER_ROMAN[state.godOpenTier]), 0.62, 0.57, 0.5, true)
        end
        GameTooltip:Show()
    end)
    dialArea:SetScript("OnLeave", function() GameTooltip:Hide() end)

    local function Arrow(direction)
        local button = CreateFrame("Button", nil, column)
        button:SetSize(30, 30)
        local arrow = direction < 0 and "Prev" or "Next"
        button:SetNormalTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. arrow .. "Page-Up")
        button:SetPushedTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. arrow .. "Page-Down")
        button:SetDisabledTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. arrow .. "Page-Disabled")
        button:SetHighlightTexture("Interface\\Buttons\\UI-Common-MouseHilight", "ADD")
        button:SetPoint("CENTER", column, "TOP", direction * 100, dialY)
        button:SetScript("OnClick", function()
            local tier = GodTier() + direction
            if tier < TIER_MIN or tier > state.godOpenTier then
                return
            end
            state.godTier = tier
            PlaySound("igMainMenuOptionCheckBoxOn")
            Refresh(false)
            Tween(0.3, 0, function(p)
                tierText:SetFont(MORPHEUS, 26 + 8 * (1 - OutCubic(p)))
                page.rewards:SetAlpha(0.3 + 0.7 * p)
            end)
        end)
        return button
    end
    local previousTier, nextTier
    if spec.tiers then
        previousTier = Arrow(-1)
        nextTier = Arrow(1)
    end

    -- Its words, then its story
    local quote = GodText(column, FRIZ, 13, 1, 0.86, 0.55)
    quote:SetPoint("TOP", column, "TOP", 0, -178)
    quote:SetWidth(columnWidth - 10)
    quote:SetJustifyH("CENTER")
    quote:SetSpacing(3)
    quote:SetText(spec.quote)

    local signature = GodText(column, FRIZ, 11, 0.75, 0.62, 0.4)
    signature:SetPoint("TOPRIGHT", quote, "BOTTOMRIGHT", -6, -5)
    signature:SetText(spec.signature)

    local lore = GodText(column, FRIZ, 12, 0.86, 0.83, 0.76)
    -- A width and a height of its own: anchored at both sides only, the client keeps it to one line
    lore:SetPoint("TOP", column, "TOP", 0, -252)
    lore:SetWidth(columnWidth - 6)
    lore:SetHeight(132)
    lore:SetJustifyH("LEFT")
    lore:SetJustifyV("TOP")
    lore:SetSpacing(3)
    lore:SetText(spec.lore)

    Divider(-392)

    -- What it asks, what it pays
    local requiredLabel = GodText(column, FRIZ, 11, 1, 0.82, 0.3)
    requiredLabel:SetPoint("TOPLEFT", column, "TOPLEFT", 4, -408)
    requiredLabel:SetText(TEXT.godRequired)
    local itemLevel = GodText(column, FRIZ, 11, 0.78, 0.55, 1)
    itemLevel:SetPoint("TOPLEFT", requiredLabel, "BOTTOMLEFT", 0, -4)
    local paragonNeed = GodText(column, FRIZ, 11, 1, 0.86, 0.55)
    paragonNeed:SetPoint("TOPLEFT", itemLevel, "BOTTOMLEFT", 0, -3)

    local rewards = CreateFrame("Frame", nil, column)
    rewards:SetPoint("TOPLEFT", column, "TOP", 10, -396)
    rewards:SetPoint("TOPRIGHT", column, "TOPRIGHT", 0, -396)
    rewards:SetHeight(56)
    page.rewards = rewards
    local rewardLabel = GodText(rewards, FRIZ, 11, 1, 0.82, 0.3)
    rewardLabel:SetPoint("TOPLEFT", 0, -12)
    rewardLabel:SetText(TEXT.rewards)
    local gold = GodText(rewards, FRIZ, 12, 1, 1, 1)
    gold:SetPoint("TOPLEFT", rewardLabel, "BOTTOMLEFT", 0, -4)
    local paragon = GodText(rewards, FRIZ, 11, 0.72, 0.4, 1)
    paragon:SetPoint("TOPLEFT", gold, "BOTTOMLEFT", 0, -3)
    local essences = GodText(rewards, FRIZ, 11, 0.45, 0.9, 0.55)
    essences:SetPoint("LEFT", paragon, "RIGHT", 8, 0)
    -- The gear's line, across the column under what it asks and pays: too long for the rewards' half. The Hollow
    -- Voice has no gear of its own: the line says why its way in is locked, when it is.
    local gear = GodText(column, FRIZ, 11, 0.78, 0.55, 1)
    gear:SetPoint("TOP", column, "TOP", 0, -462)
    if spec.tiers then
        -- What the gear carries on top of its item level (MythicDungeonSystem.cpp TouchByInfiniteGod), on hover
        local gearHover = CreateFrame("Frame", nil, column)
        gearHover:SetAllPoints(gear)
        gearHover:EnableMouse(true)
        gearHover:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:AddLine(TEXT.godGearTitle, 0.78, 0.55, 1)
            GameTooltip:AddLine(TEXT.godGearHelp, 1, 0.9, 0.7, true)
            for _, bonus in ipairs(TEXT.godGearBonuses) do
                GameTooltip:AddLine(" ")
                GameTooltip:AddLine(bonus[1], 1, 0.82, 0.3)
                GameTooltip:AddLine(bonus[2], 0.85, 0.8, 0.7, true)
            end
            GameTooltip:Show()
        end)
        gearHover:SetScript("OnLeave", function() GameTooltip:Hide() end)
    else
        gear:SetTextColor(0.85, 0.53, 0.37)
    end

    local satchel = CreateFrame("Button", nil, rewards)
    satchel:SetSize(30, 30)
    satchel:SetPoint("TOPRIGHT", rewards, "TOPRIGHT", -6, -4)
    local satchelIcon = satchel:CreateTexture(nil, "ARTWORK")
    satchelIcon:SetAllPoints()
    satchelIcon:SetTexture(SATCHEL_ICON)
    local satchelBorder = satchel:CreateTexture(nil, "OVERLAY")
    satchelBorder:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    satchelBorder:SetBlendMode("ADD")
    satchelBorder:SetVertexColor(0.64, 0.21, 0.93)
    satchelBorder:SetPoint("TOPLEFT", -12, 12)
    satchelBorder:SetPoint("BOTTOMRIGHT", 12, -12)
    satchel:SetScript("OnEnter", CardTooltipSatchel)
    satchel:SetScript("OnLeave", function() GameTooltip:Hide() end)

    local burst = rewards:CreateTexture(nil, "OVERLAY", nil, 4)
    SetAtlas(burst, "ChallengeMode-SpikeyStar")
    burst:SetBlendMode("ADD")
    burst:SetPoint("CENTER", satchel, "CENTER")
    burst:SetAlpha(0)
    local popIcon = rewards:CreateTexture(nil, "OVERLAY", nil, 5)
    popIcon:SetTexture(SATCHEL_ICON)
    popIcon:SetPoint("CENTER", satchel, "CENTER")
    popIcon:SetAlpha(0)
    local popText = rewards:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    popText:SetPoint("BOTTOM", satchel, "TOP", 0, 6)
    popText:SetAlpha(0)

    -- The way in; a reward waiting glows behind it
    local glow = column:CreateTexture(nil, "BACKGROUND")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetBlendMode("ADD")
    glow:SetPoint("CENTER", column, "TOP", 0, -497)
    glow:SetSize(330, 70)
    glow:Hide()

    local button = CreateFrame("Button", nil, column, "UIPanelButtonTemplate")
    button:SetSize(250, 28)
    button:SetPoint("CENTER", column, "TOP", 0, -497)
    -- A disabled button still shows its tooltip: locked, it says what the sign-up asks for
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        local mission = rewards.mission
        if mission and mission.state == STATE_OPEN and not MeetsItemLevel(mission) then
            GameTooltip:AddLine(format(TEXT.voiceLocked, mission.signUpItemLevel, state.itemLevel or 0),
                0.85, 0.53, 0.37, true)
            GameTooltip:AddLine(format(TEXT.voiceLockHelp, mission.signUpItemLevel), 1, 0.9, 0.7, true)
        else
            GameTooltip:AddLine(spec.once, 1, 0.9, 0.7, true)
        end
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)
    local status = GodText(column, FRIZ, 14, 1, 0.86, 0.55)
    status:SetPoint("CENTER", column, "TOP", 0, -497)

    local pulse = 0
    page:SetScript("OnUpdate", function(_, elapsed)
        pulse = pulse + elapsed
        if glow:IsShown() then
            glow:SetAlpha(0.35 + 0.35 * (0.5 + 0.5 * math.sin(pulse * 3)))
        end
    end)

    page.Refresh = function(mission)
        rewards.mission = mission
        local tier = PageTier()
        local shownTier = CardTier(mission)
        local open = mission.state == STATE_OPEN

        name:SetText(spec.name or mission.name)
        place:SetText(format(TEXT.players, mission.players, PLACE_BY_BOSS[boss]))
        kind:SetText(TEXT.challenge .. " " .. (TIER_ROMAN[shownTier] or ""))

        if spec.tiers then
            tierText:SetText(TierName(tier))
            tierInfo:SetText(format(TEXT.tierBoss, Decimal(GOD_TIER_HEALTH[tier] or 1),
                Decimal(GOD_TIER_DAMAGE[tier] or 1), GOD_ATTEMPTS,
                GOD_ATTEMPTS > 1 and "s" or ""))
            dialGlow:SetAlpha(0.18 + 0.05 * tier)
            SetEnabled(previousTier, open and tier > TIER_MIN and state.challenge == 0)
            SetEnabled(nextTier, open and tier < state.godOpenTier and state.challenge == 0)
        else
            -- One difficulty: the profile it is made for (ChallengeTiers.h BossProfiles)
            tierText:SetText(TEXT.voiceTier)
            tierInfo:SetText(format(TEXT.voiceProfile, mission.requiredItemLevel or 0, mission.baseParagon or 0,
                VOICE_ATTEMPTS, VOICE_ATTEMPTS > 1 and "s" or ""))
            dialGlow:SetAlpha(0.4)
        end

        local wanted
        if spec.tiers then
            -- The tier's profile: 10 item levels more a tier (ChallengeTiers.h BossProfiles)
            itemLevel:SetText(format(TEXT.godItemLevel,
                (mission.requiredItemLevel or 0) + GOD_REQUIRED_ITEM_LEVEL_PER_TIER * (tier - TIER_MIN)))
            wanted = (mission.baseParagon or 0) + GOD_PARAGON_PER_TIER * (tier - TIER_MIN)
        else
            -- The equipped item level signing up asks for, beside the player's: gold when they wear it, a dull
            -- ember when not (the server refuses the sign-up below it)
            local signUp = mission.signUpItemLevel or 0
            itemLevel:SetText(state.itemLevel and format(TEXT.voiceItemLevel, signUp, state.itemLevel) or
                format(TEXT.voiceItemLevelUnknown, signUp))
            if MeetsItemLevel(mission) then
                itemLevel:SetTextColor(1, 0.86, 0.55)
            else
                itemLevel:SetTextColor(0.85, 0.53, 0.37)
            end
            wanted = mission.baseParagon or 0
        end
        paragonNeed:SetText(format(TEXT.godParagon, wanted, state.paragon or 0))
        if (state.paragon or 0) >= wanted then
            paragonNeed:SetTextColor(1, 0.86, 0.55)
        else
            paragonNeed:SetTextColor(0.85, 0.53, 0.37)
        end

        local goldAmount, paragonAmount, essenceAmount = MissionReward(mission, shownTier)
        gold:SetText(Money(goldAmount))
        paragon:SetText(paragonAmount > 0 and format(TEXT.paragon, paragonAmount) or "")
        essences:SetText(essenceAmount > 0 and format(TEXT.essences, essenceAmount) or "")
        local locked = open and not MeetsItemLevel(mission)
        if spec.tiers then
            local rewardLevel = min(460, (mission.itemLevel or 0) + GOD_ITEM_LEVEL_PER_TIER * (shownTier - TIER_MIN))
            -- Generated templates have four-level spacing, with exact endpoints at 370 and 460.
            if rewardLevel > 370 and rewardLevel < 460 then
                rewardLevel = 285 + 4 * floor((rewardLevel - 285) / 4)
            end
            gear:SetText(format(TEXT.godGear, rewardLevel))
        else
            gear:SetText(locked and format(TEXT.voiceLocked, mission.signUpItemLevel, state.itemLevel or 0) or "")
        end

        local done = mission.state == STATE_CLAIMED
        scene.figure:SetDesaturated(done)
        SetShown(check, done)
        SetShown(glow, mission.state == STATE_WON)

        button:SetScript("OnClick", nil)
        status:SetText("")
        if open then
            button:Show()
            button:SetText(spec.face)
            SetEnabled(button, state.challenge == 0 and not locked)
            button:SetScript("OnClick", function()
                local roles = RolesMask()
                if roles == 0 then
                    return ShowError(8)
                end
                PlaySound("igMainMenuOptionCheckBoxOn")
                state.starting = boss
                Send("START\t" .. boss .. "\t" .. roles .. "\t" .. PageTier())
            end)
        elseif mission.state == STATE_WON then
            button:Show()
            button:SetText(TEXT.claim)
            SetEnabled(button, true)
            button:SetScript("OnClick", function()
                Send("CLAIM\t" .. state.rotation .. "\t" .. boss)
            end)
        else
            button:Hide()
            status:SetText(mission.state == STATE_UNDERWAY and TEXT.underway or TEXT.claimed)
            if mission.state == STATE_UNDERWAY then
                status:SetTextColor(0.45, 0.75, 1)
            else
                status:SetTextColor(1, 0.86, 0.55)
            end
        end
    end

    -- The scene shown or hidden (Refresh puts the board's parchment and header back when no page's is shown)
    page.SetScene = function(shown)
        for _, texture in ipairs(scene) do
            SetShown(texture, shown)
        end
    end

    -- Stepping in: the scene fades up, the boss rising out of the dark
    page.Enter = function(withSound)
        if withSound then
            PlaySoundFile(spec.sound)
        end
        -- Only the two paintings fade: a texture's SetAlpha turns a gradient's colour white (the shade's)
        Tween(0.7, 0, function(p)
            local eased = OutCubic(p)
            scene.backdrop:SetAlpha(eased)
            scene.figure:SetAlpha(eased)
            scene.figure:SetPoint("TOPLEFT", frame, "TOPLEFT", GOD_SCENE_LEFT, -GOD_SCENE_TOP + 16 * (1 - eased))
            scene.figure:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", GOD_SCENE_LEFT, GOD_SCENE_LEFT - 16 * (1 - eased))
        end)
        page:SetAlpha(0)
        Tween(0.5, 0.25, function(p) page:SetAlpha(p) end)
    end

    -- A tier of the god just opened: the dial moves to it and its name bursts
    page.Opened = function(tier)
        state.godTier = tier
        PlaySoundFile(SOUND .. "NewRecord.ogg")
        UIErrorsFrame:AddMessage(format(TEXT.tierOpened, TIER_ROMAN[tier]), 1, 0.86, 0.55, 1)
        Tween(0.8, 0, function(p)
            tierText:SetFont(MORPHEUS, 26 + 14 * (1 - OutCubic(p)))
            dialGlow:SetAlpha((0.18 + 0.05 * tier) + 0.6 * (1 - p))
        end)
    end

    -- Taken up: a Mythic+ start's weight rather than a quest's, a dim gold swell over the boss, the stamp
    page.Accepted = function()
        PlaySoundFile(SOUND .. "ChallengeStart.ogg")
        PlaySoundFile(SOUND .. "DomeOpen.ogg")
        stamp:SetText(TEXT.accepted)
        Tween(0.5, 0, function(p)
            stamp:SetAlpha(p)
            stamp:SetFont(MORPHEUS, 30 + 26 * (1 - OutCubic(p)), "OUTLINE")
        end)
        Tween(0.8, 1.9, function(p) stamp:SetAlpha(1 - p) end)
        Tween(1, 0, function(p) scene.swell:SetAlpha(0.45 * OutCubic(p)) end)
        Tween(2.2, 1.2, function(p) scene.swell:SetAlpha(0.45 * (1 - p)) end)
    end

    page.Claimed = function(goldAmount, paragonAmount, essenceAmount)
        popText:SetText("+" .. Money(goldAmount) .. (paragonAmount > 0 and
            ("\n|cffa335ee" .. format(TEXT.paragon, paragonAmount) .. "|r") or "") .. ((essenceAmount or 0) > 0 and
            ("\n|cff4dff73" .. format(TEXT.essences, essenceAmount) .. "|r") or ""))
        Tween(0.9, 0, function(p)
            local grow = OutBack(min(1, p * 1.6))
            popIcon:SetSize(30 + 36 * grow, 30 + 36 * grow)
            popIcon:SetAlpha(p < 0.6 and 1 or (1 - p) / 0.4)
            burst:SetSize(40 + 150 * p, 40 + 150 * p)
            burst:SetAlpha(0.9 * (1 - p))
            popText:SetAlpha(p < 0.7 and 1 or (1 - p) / 0.3)
            popText:SetPoint("BOTTOM", satchel, "TOP", 0, 6 + 40 * OutCubic(p))
        end)
    end
    return page
end

-- The window --------------------------------------------------------------------------------------------------------

local function CreateBoard()
    frame = CreateFrame("Frame", "ChallengeBoardFrame", UIParent)
    frame:SetSize(WIDTH, HEIGHT)
    frame:SetPoint("CENTER", 0, 20)
    frame:SetFrameStrata("HIGH")
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:SetClampedToScreen(true)
    frame:Hide()

    local ground = frame:CreateTexture(nil, "BACKGROUND")
    ground:SetTexture(RetailUIFiles["ui-background-rock"], true)
    ground:SetHorizTile(true)
    ground:SetVertTile(true)
    ground:SetPoint("TOPLEFT", 2, -21)
    ground:SetPoint("BOTTOMRIGHT", -2, 2)

    local parchment = frame:CreateTexture(nil, "BORDER")
    SetAtlas(parchment, "questbg-parchment")
    parchment:SetPoint("TOPLEFT", 8, -26)
    parchment:SetPoint("BOTTOMRIGHT", -8, 8)
    parchment:SetVertexColor(0.34, 0.28, 0.22)

    RetailUI.ApplyNineSlice(frame, false)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOPLEFT", frame, "TOPLEFT", 26, -4)
    title:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -26, -4)
    title:SetJustifyH("CENTER")
    title:SetText(TEXT.title)
    title:SetTextColor(1, 0.82, 0)

    local closeButton = CreateFrame("Button", nil, frame)
    closeButton:SetSize(24, 24)
    closeButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -5, -5)
    closeButton:SetFrameLevel(frame:GetFrameLevel() + 20)
    closeButton:SetNormalTexture(RetailUIAtlas["redbutton-exit-2x"][1])
    RetailUI.SetAtlas(closeButton:GetNormalTexture(), "redbutton-exit-2x")
    closeButton:SetPushedTexture(RetailUIAtlas["redbutton-exit-pressed-2x"][1])
    RetailUI.SetAtlas(closeButton:GetPushedTexture(), "redbutton-exit-pressed-2x")
    closeButton:SetHighlightTexture(RetailUIAtlas["redbutton-highlight-2x"][1])
    RetailUI.SetAtlas(closeButton:GetHighlightTexture(), "redbutton-highlight-2x")
    closeButton:GetHighlightTexture():SetBlendMode("ADD")
    closeButton:SetScript("OnClick", function() frame:Hide() end)

    local mover = CreateFrame("Frame", nil, frame)
    mover:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 16)
    mover:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -40, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(frame:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() frame:StartMoving() end)
    mover:SetScript("OnDragStop", function() frame:StopMovingOrSizing() end)

    local heading = frame:CreateFontString(nil, "OVERLAY")
    heading:SetFont(MORPHEUS, 28)
    heading:SetShadowOffset(1, -1)
    heading:SetTextColor(1, 0.86, 0.55)
    heading:SetPoint("TOPLEFT", frame, "TOPLEFT", 34, -40)
    heading:SetText(TEXT.heading)
    frame.heading = heading

    local intro = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    intro:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 2, -4)
    intro:SetWidth(470)
    intro:SetJustifyH("LEFT")
    intro:SetTextColor(0.85, 0.8, 0.7)
    intro:SetText(TEXT.intro)
    frame.intro = intro

    -- The board's clock: a watch, the time left before the missions change beside it, and a bar running down with it
    -- underneath both. The watch has a column of its own: the label used to run under it.
    local timerIcon = frame:CreateTexture(nil, "OVERLAY")
    timerIcon:SetTexture("Interface\\Icons\\INV_Misc_PocketWatch_01")
    timerIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    timerIcon:SetSize(38, 38)
    timerIcon:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -196, -42)

    local timerIconBorder = frame:CreateTexture(nil, "OVERLAY", nil, 1)
    timerIconBorder:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    timerIconBorder:SetPoint("TOPLEFT", timerIcon, "TOPLEFT", -12, 12)
    timerIconBorder:SetPoint("BOTTOMRIGHT", timerIcon, "BOTTOMRIGHT", 12, -12)

    local timerLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    timerLabel:SetPoint("TOPLEFT", timerIcon, "TOPRIGHT", 10, 0)
    timerLabel:SetJustifyH("LEFT")
    timerLabel:SetText(TEXT.refresh)

    timerText = frame:CreateFontString(nil, "OVERLAY")
    timerText:SetFont(FRIZ, 20)
    timerText:SetShadowOffset(1, -1)
    timerText:SetTextColor(1, 1, 1)
    timerText:SetPoint("TOPLEFT", timerLabel, "BOTTOMLEFT", 0, -1)

    local timerBar = CreateFrame("Frame", nil, frame)
    timerBar:SetSize(202, 12)
    timerBar:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -32, -86)
    timerBar:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 8,
        insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    timerBar:SetBackdropColor(0, 0, 0, 0.65)
    timerBar:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)

    timerFill = timerBar:CreateTexture(nil, "ARTWORK")
    timerFill:SetTexture("Interface\\Buttons\\WHITE8X8")
    timerFill:SetGradient("HORIZONTAL", 0.75, 0.45, 0.1, 1, 0.85, 0.35)
    timerFill:SetHeight(6)
    timerFill:SetPoint("LEFT", timerBar, "LEFT", 3, 0)
    timerFill.full = 196

    local divider = frame:CreateTexture(nil, "ARTWORK")
    SetAtlas(divider, "ChallengeMode-ThinDivider")
    divider:SetHeight(12)
    divider:SetPoint("TOPLEFT", frame, "TOPLEFT", 24, -100)
    divider:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -24, -100)

    -- What L'Infini's and the Hollow Voice's scenes take the place of (CreateGodPage)
    frame.boardArt = { ground, parchment, heading, intro, timerIcon, timerIconBorder, timerLabel, timerText, timerBar,
        divider }

    -- The tier dial: an arrow either side of the tier's name, what it does to the boss under it. Only the tiers open
    -- can be picked; the one after the highest says how to open it.
    dial = CreateFrame("Frame", nil, frame)
    dial:SetSize(460, 52)
    dial:SetPoint("TOP", frame, "TOP", 0, -108)
    dial:EnableMouse(true)

    local dialGlow = dial:CreateTexture(nil, "BACKGROUND")
    SetAtlas(dialGlow, "ChallengeMode-SoftYellowGlow")
    dialGlow:SetBlendMode("ADD")
    dialGlow:SetPoint("CENTER", dial, "TOP", 0, -16)
    dialGlow:SetSize(250, 64)

    local tierText = dial:CreateFontString(nil, "OVERLAY")
    tierText:SetFont(MORPHEUS, 26)
    tierText:SetShadowOffset(1, -1)
    tierText:SetTextColor(1, 0.86, 0.55)
    tierText:SetPoint("CENTER", dial, "TOP", 0, -16)

    local tierInfo = dial:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    tierInfo:SetPoint("TOP", tierText, "BOTTOM", 0, -4)
    tierInfo:SetTextColor(0.85, 0.8, 0.7)

    -- The paragon the tier asks for, beside the dial's right arrow: gold when the player has it, a dull ember when not
    local paragonLabel = dial:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    paragonLabel:SetPoint("LEFT", dial, "TOP", 124, -10)
    paragonLabel:SetText(TEXT.paragonWanted)
    local paragonValue = dial:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    paragonValue:SetPoint("TOPLEFT", paragonLabel, "BOTTOMLEFT", 0, -2)

    local function Arrow(direction)
        local button = CreateFrame("Button", nil, dial)
        button:SetSize(30, 30)
        local page = direction < 0 and "Prev" or "Next"
        button:SetNormalTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. page .. "Page-Up")
        button:SetPushedTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. page .. "Page-Down")
        button:SetDisabledTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. page .. "Page-Disabled")
        button:SetHighlightTexture("Interface\\Buttons\\UI-Common-MouseHilight", "ADD")
        button:SetPoint("CENTER", dial, "TOP", direction * 104, -16)
        button:SetScript("OnClick", function()
            local tier = (state.tier or state.openTier) + direction
            if tier < TIER_MIN or tier > state.openTier then
                return
            end
            state.tier = tier
            PlaySound("igMainMenuOptionCheckBoxOn")
            Refresh(false)
            -- The new tier's name swells and settles, and the rewards on the cards with it
            Tween(0.3, 0, function(p)
                tierText:SetFont(MORPHEUS, 26 + 8 * (1 - OutCubic(p)))
                for _, card in ipairs(cards) do
                    card.gold:SetAlpha(0.3 + 0.7 * p)
                    card.paragon:SetAlpha(0.3 + 0.7 * p)
                    card.essences:SetAlpha(0.3 + 0.7 * p)
                end
            end)
        end)
        return button
    end
    local previousTier = Arrow(-1)
    local nextTier = Arrow(1)

    dial.Refresh = function()
        local tier = min(state.tier or state.openTier, state.openTier)
        state.tier = tier
        tierText:SetText(TierName(tier))
        local wipes = TierWipes(tier)
        tierInfo:SetText(format(TEXT.tierBoss, Decimal(TierHealth(tier)), Decimal(TierDamage(tier)), wipes,
            wipes > 1 and "s" or ""))
        local showParagon = (state.bracket or 0) >= PARAGON_BRACKET and state.paragon
        SetShown(paragonLabel, showParagon and true or false)
        SetShown(paragonValue, showParagon and true or false)
        if showParagon then
            local wanted = TierParagon(tier)
            paragonValue:SetText(format(TEXT.paragonValue, wanted, state.paragon))
            if state.paragon >= wanted then
                paragonValue:SetTextColor(1, 0.86, 0.55)
            else
                paragonValue:SetTextColor(0.85, 0.53, 0.37)
            end
        end
        -- The higher the tier, the brighter the dial
        dialGlow:SetAlpha(0.18 + 0.05 * tier)
        SetEnabled(previousTier, tier > TIER_MIN and state.challenge == 0)
        SetEnabled(nextTier, tier < state.openTier and state.challenge == 0)
    end

    dial:SetScript("OnEnter", function(self)
        local tier = state.tier or state.openTier
        GameTooltip:SetOwner(self, "ANCHOR_BOTTOM")
        GameTooltip:AddLine(TEXT.tierTitle, 1, 0.82, 0.3)
        GameTooltip:AddLine(TEXT.tierHelp, 1, 0.9, 0.7, true)
        GameTooltip:AddLine(" ")
        GameTooltip:AddLine(TierName(tier), 1, 0.86, 0.55)
        GameTooltip:AddLine(format(TEXT.tierReward, TierGoldPercent(tier) - 100, TierExtraEssences(tier),
            TierExtraParagon(tier)), 0.85, 0.8, 0.7, true)
        if (state.bracket or 0) >= PARAGON_BRACKET and state.paragon then
            GameTooltip:AddLine(format(TEXT.paragonTip, TierParagon(tier), state.paragon), 0.85, 0.8, 0.7, true)
        end
        GameTooltip:AddLine(" ")
        if state.openTier >= TIER_MAX then
            GameTooltip:AddLine(TEXT.tierTop, 0.62, 0.57, 0.5, true)
        else
            GameTooltip:AddLine(format(TEXT.tierNext, TIER_ROMAN[state.openTier]), 0.62, 0.57, 0.5, true)
        end
        GameTooltip:Show()
    end)
    dial:SetScript("OnLeave", function() GameTooltip:Hide() end)

    -- A tier just opened: the dial moves to it and its name bursts
    dial.Opened = function(tier)
        state.tier = tier
        dial.Refresh()
        PlaySoundFile(SOUND .. "NewRecord.ogg")
        UIErrorsFrame:AddMessage(format(TEXT.tierOpened, TIER_ROMAN[tier]), 1, 0.86, 0.55, 1)
        Tween(0.8, 0, function(p)
            tierText:SetFont(MORPHEUS, 26 + 14 * (1 - OutCubic(p)))
            dialGlow:SetAlpha((0.18 + 0.05 * tier) + 0.6 * (1 - p))
        end)
    end

    cards = {}
    for index = 1, 4 do
        cards[index] = CreateCard(index)
    end

    -- The dungeons page: the player's key in the Mythic+ tab's own slot, beside what finishing in time adds
    keyStrip = CreateFrame("Frame", nil, frame)
    keyStrip:SetSize(520, 52)
    keyStrip:SetPoint("TOP", frame, "TOP", 0, -108)
    keyStrip:Hide()

    local slot = CreateFrame("Frame", nil, keyStrip)
    slot:SetSize(52, 52)
    slot:SetPoint("LEFT", keyStrip, "LEFT", 0, 0)
    local slotBackground = slot:CreateTexture(nil, "BORDER")
    SetAtlas(slotBackground, "ChallengeMode-KeystoneSlotBG")
    slotBackground:SetSize(46, 46)
    slotBackground:SetPoint("CENTER")
    local slotIcon = slot:CreateTexture(nil, "ARTWORK")
    slotIcon:SetTexture(KEYSTONE_ICON)
    slotIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    slotIcon:SetSize(30, 30)
    slotIcon:SetPoint("CENTER")
    local slotFrame = slot:CreateTexture(nil, "OVERLAY")
    SetAtlas(slotFrame, "ChallengeMode-KeystoneSlotFrame")
    slotFrame:SetSize(52, 52)
    slotFrame:SetPoint("CENTER")
    local slotGlow = slot:CreateTexture(nil, "OVERLAY", nil, 1)
    SetAtlas(slotGlow, "ChallengeMode-KeystoneSlotFrameGlow")
    slotGlow:SetSize(52, 52)
    slotGlow:SetPoint("CENTER")
    slotGlow:SetBlendMode("ADD")

    local keyLabel = keyStrip:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    keyLabel:SetPoint("TOPLEFT", slot, "TOPRIGHT", 8, -6)
    keyLabel:SetText(TEXT.key)
    local keyText = keyStrip:CreateFontString(nil, "OVERLAY")
    keyText:SetFont(MORPHEUS, 26)
    keyText:SetShadowOffset(1, -1)
    keyText:SetTextColor(1, 0.86, 0.55)
    keyText:SetPoint("TOPLEFT", keyLabel, "BOTTOMLEFT", 0, -1)

    local bonus = keyStrip:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    bonus:SetPoint("LEFT", slot, "RIGHT", 120, 0)
    bonus:SetPoint("RIGHT", keyStrip, "RIGHT", 0, 0)
    bonus:SetJustifyH("LEFT")
    bonus:SetTextColor(0.85, 0.8, 0.7)
    bonus:SetText(TEXT.timedBonus)

    local pulse = 0
    keyStrip:SetScript("OnUpdate", function(_, elapsed)
        pulse = pulse + elapsed
        slotGlow:SetAlpha(0.25 + 0.25 * math.sin(pulse * 2))
    end)
    keyStrip.Refresh = function()
        keyText:SetText("+" .. state.key)
        local wanted = KeyParagon(state.key)
        local line
        if wanted == 0 then
            line = "|cffa89c80" .. TEXT.keyParagonNone .. "|r"
        elseif state.paragon then
            line = (state.paragon >= wanted and "|cffffdb8c" or "|cffd9885f")
                .. format(TEXT.keyParagon, state.key, wanted, state.paragon) .. "|r"
        end
        bonus:SetText(TEXT.timedBonus .. (line and ("\n" .. line) or ""))
    end

    dungeonCards = {}
    for index = 1, 4 do
        local card = CreateCard(index)
        card.isDungeon = true
        dungeonCards[index] = card
    end

    godPage = CreateGodPage(PAGE_SPECS.god)
    voicePage = CreateGodPage(PAGE_SPECS.voice)

    emptyText = frame:CreateFontString(nil, "OVERLAY")
    emptyText:SetFont(FRIZ, 16)
    emptyText:SetTextColor(0.9, 0.8, 0.6)
    emptyText:SetPoint("CENTER", frame, "CENTER", 0, 10)
    emptyText:Hide()

    -- Footer: the role the challenge takes you in, and the rewards waiting from other boards
    local roleLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    roleLabel:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 30, 54)
    roleLabel:SetText(TEXT.role)

    roleButtons = {}
    local previous
    for _, role in ipairs({ "TANK", "HEALER", "DAMAGER" }) do
        local button = CreateFrame("Button", nil, frame)
        button:SetSize(38, 38)
        if previous then
            button:SetPoint("LEFT", previous, "RIGHT", 8, 0)
        else
            button:SetPoint("TOPLEFT", roleLabel, "BOTTOMLEFT", -2, -4)
        end
        local ring = button:CreateTexture(nil, "BACKGROUND")
        SetAtlas(ring, "ChallengeMode-SoftYellowGlow")
        ring:SetBlendMode("ADD")
        ring:SetPoint("TOPLEFT", -10, 10)
        ring:SetPoint("BOTTOMRIGHT", 10, -10)
        button.ring = ring
        local icon = button:CreateTexture(nil, "ARTWORK")
        icon:SetAllPoints()
        icon:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-ROLES")
        icon:SetTexCoord(GetTexCoordsForRole(role))
        button.icon = icon
        button:SetScript("OnClick", function() ToggleRole(role) end)
        button:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_TOP")
            GameTooltip:SetText(_G[role] or role)
            GameTooltip:Show()
        end)
        button:SetScript("OnLeave", function() GameTooltip:Hide() end)
        roleButtons[role] = button
        previous = button
    end

    quitButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    quitButton:SetSize(170, 24)
    quitButton:SetPoint("LEFT", previous, "RIGHT", 24, 0)
    quitButton:SetText(TEXT.quit)
    quitButton:SetScript("OnClick", function()
        PlaySound("igQuestLogAbandonQuest")
        Send("QUIT")
    end)
    quitButton:Hide()

    errorText = frame:CreateFontString(nil, "OVERLAY", "GameFontRedSmall")
    errorText:SetPoint("BOTTOM", frame, "BOTTOM", 0, 20)
    errorText:SetAlpha(0)

    rewardRows = {}
    local rewardLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    rewardLabel:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -30, 76)
    rewardLabel:SetText(TEXT.waiting)
    rewardRows.label = rewardLabel
    for index = 1, 3 do
        local row = CreateFrame("Frame", nil, frame)
        row:SetSize(300, 22)
        row:SetPoint("TOPRIGHT", rewardLabel, "BOTTOMRIGHT", 0, -2 - (index - 1) * 22)
        local button = CreateFrame("Button", nil, row, "UIPanelButtonTemplate")
        button:SetSize(90, 20)
        button:SetPoint("RIGHT")
        button:SetText(TEXT.claim)
        row.button = button
        local icon = row:CreateTexture(nil, "ARTWORK")
        icon:SetSize(18, 18)
        icon:SetPoint("RIGHT", button, "LEFT", -6, 0)
        row.icon = icon
        local text = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        text:SetPoint("RIGHT", icon, "LEFT", -6, 0)
        text:SetJustifyH("RIGHT")
        row.text = text
        row:Hide()
        rewardRows[index] = row
    end

    -- The tabs under the window, as on the character sheet: the raid missions, the dungeon challenges, the god, the
    -- Hollow Voice
    tabs = {}
    local pages = { "raids", "dungeons", "god", "voice" }
    local headings = { raids = TEXT.heading, dungeons = TEXT.headingDungeons, god = TEXT.headingGod,
        voice = TEXT.headingVoice }
    local intros = { raids = TEXT.intro, dungeons = TEXT.introDungeons, god = TEXT.introGod, voice = TEXT.introVoice }
    for index, label in ipairs({ TEXT.tabRaids, TEXT.tabDungeons, TEXT.tabGod, TEXT.tabVoice }) do
        local tab = CreateFrame("Button", "ChallengeBoardFrameTab" .. index, frame, "CharacterFrameTabButtonTemplate")
        tab:SetID(index)
        tab:SetText(label)
        PanelTemplates_TabResize(tab, 12)
        if index == 1 then
            tab:SetPoint("TOPLEFT", frame, "BOTTOMLEFT", 24, 4)
        else
            tab:SetPoint("LEFT", tabs[index - 1], "RIGHT", -16, 0)
        end
        tab:SetScript("OnClick", function(self)
            local page = pages[self:GetID()]
            if page == state.page then
                return
            end
            PlaySound("igCharacterInfoTab")
            state.page = page
            PanelTemplates_SetTab(frame, self:GetID())
            frame.heading:SetText(headings[page])
            frame.intro:SetText(intros[page])
            Refresh(true)
        end)
        tabs[index] = tab
    end
    PanelTemplates_SetNumTabs(frame, #pages)
    PanelTemplates_SetTab(frame, 1)

    -- The clock ticks, the waiting rewards breathe, and a new board is asked for when the time runs out
    local clock = 0
    frame:SetScript("OnUpdate", function(_, elapsed)
        clock = clock + elapsed
        local left = state.left - (GetTime() - state.receivedAt)
        timerText:SetText(FormatTime(left))
        timerFill:SetWidth(max(1, timerFill.full * max(0, left) / ROTATION_SECONDS))
        if left <= 0 and not state.asked then
            state.asked = true
            Send("OPEN")
        end
        -- Gear changed with the board open: the board again, with the item level the server measures now (a set
        -- swapped is many changes, asked for once)
        if state.gearChanged and GetTime() - state.gearChanged > 0.5 then
            state.gearChanged = nil
            Send("OPEN")
        end

        local pulse = 0.5 + 0.5 * math.sin(clock * 3)
        for _, set in ipairs({ cards, dungeonCards }) do
            for _, card in ipairs(set) do
                if card.glow:IsShown() then
                    card.glow:SetAlpha(0.35 + 0.45 * pulse)
                end
            end
        end
    end)

    frame:SetScript("OnShow", function()
        PlaySound("igQuestListOpen")
        frame:SetAlpha(0)
        frame:SetScale(0.94)
        Tween(0.22, 0, function(p)
            frame:SetAlpha(p)
            frame:SetScale(0.94 + 0.06 * OutCubic(p))
        end)
    end)
    frame:SetScript("OnHide", function()
        PlaySound("igQuestListClose")
        GameTooltip:Hide()
    end)

    tinsert(UISpecialFrames, "ChallengeBoardFrame")
end

local function ShowBoard(open)
    if not frame then
        CreateBoard()
    end

    local newRotation = state.shownRotation ~= state.rotation
    local wasShown = frame:IsShown()
    if open and not wasShown then
        state.shownRotation = state.rotation
        frame:Show()
        Refresh(true)
    elseif wasShown then
        -- A new board while it is open: the old cards leave, the new ones drop in
        if newRotation then
            state.shownRotation = state.rotation
            PlaySound("igCharacterInfoTab")
        end
        Refresh(newRotation)
    end
end

-- Claiming: the satchel jumps out of its card in a burst of light, and the gold with it
local function AnimateClaim(boss, gold, paragon, essences)
    PlaySound("igQuestListComplete")
    PlaySound("LOOTWINDOWCOINSOUND")
    if not frame or not frame:IsShown() then
        return
    end
    if boss == GOD_BOSS and state.page == "god" and godPage:IsShown() then
        return godPage.Claimed(gold, paragon, essences)
    elseif boss == VOICE_BOSS and state.page == "voice" and voicePage:IsShown() then
        return voicePage.Claimed(gold, paragon, essences)
    end

    for _, card in ipairs(state.page == "dungeons" and dungeonCards or cards) do
        if card.mission and card.mission.boss == boss then
            card.popText:SetText("+" .. Money(gold) .. (paragon > 0 and
                ("\n|cffa335ee" .. format(TEXT.paragon, paragon) .. "|r") or "") .. ((essences or 0) > 0 and
                ("\n|cff4dff73" .. format(TEXT.essences, essences) .. "|r") or ""))
            Tween(0.9, 0, function(p)
                local grow = OutBack(min(1, p * 1.6))
                card.popIcon:SetSize(30 + 34 * grow, 30 + 34 * grow)
                card.popIcon:SetAlpha(p < 0.6 and 1 or (1 - p) / 0.4)
                card.burst:SetSize(40 + 140 * p, 40 + 140 * p)
                card.burst:SetAlpha(0.9 * (1 - p))
                card.popText:SetAlpha(p < 0.7 and 1 or (1 - p) / 0.3)
                card.popText:SetPoint("BOTTOM", card.satchel, "TOP", 0, 6 + 40 * OutCubic(p))
            end)
        end
    end
end

local function AnimateAccepted(boss)
    -- The god's page and the Hollow Voice's have a sound of their own
    if boss == GOD_BOSS and frame and frame:IsShown() and state.page == "god" and godPage:IsShown() then
        return godPage.Accepted()
    elseif boss == VOICE_BOSS and frame and frame:IsShown() and state.page == "voice" and voicePage:IsShown() then
        return voicePage.Accepted()
    end
    PlaySound("WriteQuest")
    if not frame or not frame:IsShown() then
        return
    end

    for _, card in ipairs(state.page == "dungeons" and dungeonCards or cards) do
        if card.mission and card.mission.boss == boss then
            card.stamp:SetText(TEXT.accepted)
            Tween(0.35, 0, function(p)
                card.stamp:SetAlpha(p)
                card.stamp:SetFont(MORPHEUS, 22 + 20 * (1 - OutCubic(p)), "OUTLINE")
            end)
            Tween(0.6, 1.4, function(p) card.stamp:SetAlpha(1 - p) end)
        end
    end
end

-- The banner: the challenge under way, at the top of the screen ----------------------------------------------------

local function CreateBanner()
    banner = CreateFrame("Frame", "ChallengeBanner", UIParent)
    banner:SetSize(460, 64)
    banner:SetPoint("TOP", UIParent, "TOP", 0, -120)
    banner:SetFrameStrata("MEDIUM")
    banner:Hide()

    local back = banner:CreateTexture(nil, "BACKGROUND")
    back:SetTexture("Interface\\Buttons\\WHITE8X8")
    back:SetAllPoints()
    back:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.8)
    local backRight = banner:CreateTexture(nil, "BACKGROUND")
    backRight:SetTexture("Interface\\Buttons\\WHITE8X8")
    backRight:SetPoint("TOPLEFT", banner, "TOP")
    backRight:SetPoint("BOTTOMRIGHT")
    backRight:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.8, 0, 0, 0, 0)
    back:SetPoint("BOTTOMRIGHT", banner, "BOTTOM")

    for _, edge in ipairs({ "TOP", "BOTTOM" }) do
        local line = banner:CreateTexture(nil, "ARTWORK")
        SetAtlas(line, "ChallengeMode-ThinDivider")
        line:SetHeight(10)
        line:SetPoint(edge .. "LEFT", banner, edge .. "LEFT", 0, edge == "TOP" and 4 or -4)
        line:SetPoint(edge .. "RIGHT", banner, edge .. "RIGHT", 0, edge == "TOP" and 4 or -4)
    end

    local icon = banner:CreateTexture(nil, "ARTWORK")
    icon:SetSize(40, 40)
    icon:SetPoint("LEFT", banner, "LEFT", 70, 0)
    banner.icon = icon

    local title = banner:CreateFontString(nil, "OVERLAY")
    title:SetFont(MORPHEUS, 20)
    title:SetShadowOffset(1, -1)
    title:SetTextColor(1, 0.86, 0.5)
    title:SetPoint("TOPLEFT", icon, "TOPRIGHT", 12, 2)
    title:SetPoint("RIGHT", banner, "RIGHT", -60, 0)
    title:SetJustifyH("LEFT")
    banner.title = title

    local text = banner:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    text:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -3)
    text:SetPoint("RIGHT", banner, "RIGHT", -60, 0)
    text:SetJustifyH("LEFT")
    banner.text = text

    banner:SetScript("OnUpdate", function(self)
        if self.countdownEnd then
            local left = math.ceil(self.countdownEnd - GetTime())
            if left ~= self.countdownShown and left >= 0 then
                self.countdownShown = left
                self.text:SetText(format(self.countdownFormat or TEXT.returning, left))
                if left <= 3 and left > 0 then
                    PlaySoundFile(SOUND .. "CountdownTick.ogg")
                end
            end
        end
        if self.hideAt and GetTime() >= self.hideAt then
            self.hideAt = nil
            Tween(0.5, 0, function(p) self:SetAlpha(1 - p) end, function() self:Hide() end)
        end
    end)
end

local function FindMission(boss)
    for _, mission in ipairs(state.missions) do
        if mission.boss == boss then
            return mission
        end
    end
end

-- Every line shows for this long, then the banner fades: it only speaks when something happens
local BANNER_SECONDS = 5

local function ShowBanner(title, text, dungeon, hideAfter)
    if not banner then
        CreateBanner()
    end

    banner.title:SetText(title or "")
    banner.text:SetText(text or "")
    banner.countdownEnd = nil
    if dungeon and dungeon > 0 then
        banner.icon:SetTexture(DungeonTexture(dungeon, "LFGIcon-"))
    end
    banner.hideAt = GetTime() + (hideAfter or BANNER_SECONDS)

    if not banner:IsShown() then
        banner:SetAlpha(0)
        banner:Show()
    end
    -- Each new line pops: the banner swells for an instant and settles
    Tween(0.3, 0, function(p)
        banner:SetAlpha(max(banner:GetAlpha(), p))
        banner:SetScale(1.08 - 0.08 * OutCubic(p))
    end)
end

-- The wipe that ends a challenge (RaidFinder.cpp ChallengeFadeSeconds): the screen goes to black and the music fades
-- out, then the way home in the dark. Once there (the fight's silence arrived), the music's volume is put back and the
-- screen clears. The volume is put back at a logout too: it is never left at nothing.
local fade = CreateFrame("Frame", nil, UIParent)
fade:SetFrameStrata("FULLSCREEN_DIALOG")
fade:SetAllPoints(UIParent)
fade:EnableMouse(false)
fade.black = fade:CreateTexture(nil, "BACKGROUND")
fade.black:SetAllPoints()
fade.black:SetTexture(0, 0, 0, 1)
fade:Hide()
local FADE_BACK_DELAY, FADE_BACK_SECONDS = 2, 1

local function RestoreMusic()
    if fade.music then
        SetCVar("Sound_MusicVolume", fade.music)
        fade.music = nil
    end
end

local function FadeToBlack(seconds)
    seconds = max(tonumber(seconds) or 3, 0.5)
    fade.music = fade.music or tonumber(GetCVar("Sound_MusicVolume")) or 1
    fade.waiting = true
    local music = fade.music
    fade:SetAlpha(0)
    fade:Show()
    Tween(seconds, 0, function(p)
        fade:SetAlpha(p)
        SetCVar("Sound_MusicVolume", music * (1 - p))
    end)
end

fade:RegisterEvent("PLAYER_ENTERING_WORLD")
fade:RegisterEvent("PLAYER_LOGOUT")
fade:SetScript("OnEvent", function(_, event)
    if event == "PLAYER_LOGOUT" then
        RestoreMusic()
    elseif fade.waiting then
        fade.waiting = false
        Tween(FADE_BACK_SECONDS, FADE_BACK_DELAY, function(p)
            fade:SetAlpha(1 - p)
        end, function()
            fade:Hide()
            RestoreMusic()
        end)
    end
end)

local function OnEvent(event, boss, value, name, tier)
    if tier and tier > TIER_MIN then
        name = name .. " · " .. TierName(tier)
    end
    local mission = FindMission(boss)
    local dungeon = mission and mission.dungeon or banner and banner.dungeon
    if banner then
        banner.dungeon = dungeon or banner.dungeon
    end

    if event == EVENT_DUNGEON_WON then
        PlaySound("LEVELUPSOUND")
        ShowBanner(DungeonName(boss), TEXT.dungeonWon, boss, 6)
        return
    end

    if event == EVENT_ARRIVED then
        PlaySoundFile(SOUND .. "ChallengeStart.ogg")
        ShowBanner(name, format(TEXT.fight, name), dungeon)
        -- The god is introduced as the group arrives, before the pull's countdown (Cinematics.lua)
        if boss == GOD_BOSS and Cinematics then
            Cinematics.Play("LInfini")
        end
    elseif event == EVENT_KILLED then
        PlaySoundFile(SOUND .. "NewRecord.ogg")
        ShowBanner(name, format(TEXT.killed, name), dungeon)
    elseif event == EVENT_PULLING then
        -- The bot tank pulls when this runs out: the last seconds tick, as the Mythic+ countdown does
        PlaySound("ReadyCheck")
        ShowBanner(name, format(TEXT.pulling, value), dungeon, value + 1)
        banner.countdownFormat = TEXT.pulling
        banner.countdownEnd = GetTime() + value
        banner.countdownShown = value
    elseif event == EVENT_RETURNING then
        ShowBanner(name, format(TEXT.returning, value), dungeon, value + 1)
        banner.countdownFormat = TEXT.returning
        banner.countdownEnd = GetTime() + value
        banner.countdownShown = value
    elseif event == EVENT_FADING then
        FadeToBlack(value)
    elseif event == EVENT_WIPED then
        PlaySound("RaidWarning")
        ShowBanner(name, format(TEXT.wiped, value), dungeon)
    elseif event == EVENT_WON then
        PlaySound("LEVELUPSOUND")
        state.challenge = 0
        ShowBanner(name, TEXT.won, dungeon, 6)
    elseif event == EVENT_FAILED then
        PlaySound("igQuestFailed")
        state.challenge = 0
        ShowBanner(name, TEXT.failed[value] or TEXT.failed[1], dungeon, 6)
    end
end

-- The Raid Finder's status, while a challenge is being assembled: how full the group is
local function OnRaidFinderStatus(runState, counts)
    if state.challenge == 0 then
        return
    end

    local mission = FindMission(state.challenge)
    local name = mission and mission.name or ""
    local runStateNumber = tonumber(runState) or 0
    if runStateNumber == 1 or runStateNumber == 2 then
        local tanks, needTanks, healers, needHealers, damage, needDamage =
            string.match(counts or "", "(%d+):(%d+),(%d+):(%d+),(%d+):(%d+)")
        local have = (tonumber(tanks) or 0) + (tonumber(healers) or 0) + (tonumber(damage) or 0)
        local need = (tonumber(needTanks) or 0) + (tonumber(needHealers) or 0) + (tonumber(needDamage) or 0)
        ShowBanner(name, format("%s : %d/%d", TEXT.assembling, have, need), mission and mission.dungeon)
    elseif runStateNumber == 3 and banner and banner:IsShown() and banner.stage ~= "travel" then
        banner.stage = "travel"
        ShowBanner(name, format(TEXT.travel, name), mission and mission.dungeon)
    end
end

-- Messages ----------------------------------------------------------------------------------------------------------

local function Handle(message)
    local kind, a, b, c, d, e, f, g, h, i, j, k = strsplit("\t", message)
    if kind == "B" then
        incoming.rotation = tonumber(a) or 0
        incoming.left = tonumber(b) or 0
        incoming.bracket = tonumber(c) or 0
        incoming.challenge = tonumber(d) or 0
        incoming.openTier = tonumber(e) or TIER_MIN
        incoming.currentTier = tonumber(f) or 0
        incoming.key = tonumber(g) or 2
        incoming.contract = tonumber(h) or 0
        incoming.paragon = tonumber(i)
        incoming.godOpenTier = tonumber(j) or TIER_MIN
        incoming.itemLevel = tonumber(k)
        incoming.dungeons = {}
        incoming.missions = {}
        incoming.rewards = {}
    elseif kind == "M" then
        tinsert(incoming.missions, {
            kind = tonumber(a) or 1,
            boss = tonumber(b) or 0,
            dungeon = tonumber(c) or 0,
            players = tonumber(d) or 10,
            gold = tonumber(e) or 0,
            state = tonumber(f) or 0,
            difficulty = tonumber(g) or 0,
            itemLevel = tonumber(h) or 0,
            paragon = tonumber(i) or 0,
            name = j or "",
            essences = tonumber(k) or 0,
            wonAt = tonumber((select(13, strsplit("\t", message)))) or 0,
            requiredItemLevel = tonumber((select(14, strsplit("\t", message)))) or 0,
            baseParagon = tonumber((select(15, strsplit("\t", message)))) or 0,
            signUpItemLevel = tonumber((select(16, strsplit("\t", message)))) or 0,
        })
    elseif kind == "D" then
        tinsert(incoming.dungeons, {
            kind = KIND_DUNGEON,
            dungeon = tonumber(a) or 0,
            boss = tonumber(a) or 0,
            state = tonumber(b) or 0,
            gold = tonumber(c) or 0,
            paragon = tonumber(d) or 0,
            essences = tonumber(e) or 0,
            limit = tonumber(f) or 0,
            wonLevel = tonumber(g) or 0,
            wonTimed = g ~= nil and h == "1",
        })
    elseif kind == "R" then
        tinsert(incoming.rewards, {
            rotation = tonumber(a) or 0,
            boss = tonumber(b) or 0,
            dungeon = tonumber(c) or 0,
            gold = tonumber(d) or 0,
            paragon = tonumber(e) or 0,
            name = f or "",
            essences = tonumber(g) or 0,
            tier = tonumber(h) or TIER_MIN,
            kind = tonumber(i) or 1,
            level = tonumber(j) or 0,
        })
    elseif kind == "E" then
        local previousChallenge = state.challenge
        state.rotation = incoming.rotation or 0
        state.left = incoming.left or 0
        state.receivedAt = GetTime()
        state.bracket = incoming.bracket or 0
        state.challenge = incoming.challenge or 0
        state.missions = incoming.missions
        state.rewards = incoming.rewards
        state.asked = false
        state.currentTier = incoming.currentTier or 0
        state.key = incoming.key or 2
        state.paragon = incoming.paragon
        state.itemLevel = incoming.itemLevel
        state.dungeons = incoming.dungeons or {}
        -- The answer to this player's own DUNGEON: the card gets its stamp
        local previousContract = state.contract
        state.contract = incoming.contract or 0
        if state.startingDungeon and state.contract == state.startingDungeon and previousContract ~= state.contract then
            local dungeon = state.startingDungeon
            state.startingDungeon = nil
            AnimateAccepted(dungeon)
        end
        -- A tier opened since the last board: the dial moves up to it
        local opened = state.openTier and (incoming.openTier or TIER_MIN) > state.openTier and state.seenBoard
        state.openTier = incoming.openTier or TIER_MIN
        -- The same for the god's own ladder
        local godOpened = (incoming.godOpenTier or TIER_MIN) > state.godOpenTier and state.seenBoard
        state.godOpenTier = incoming.godOpenTier or TIER_MIN
        if godOpened then
            state.godTier = state.godOpenTier
        end
        state.seenBoard = true
        -- The answer to this player's own START: the card gets its stamp
        if state.challenge ~= 0 and previousChallenge == 0 and state.starting == state.challenge then
            state.starting = nil
            AnimateAccepted(state.challenge)
            if banner then
                banner.stage = nil
            end
        end
        if a == "1" then
            ShowBoard(true)
        elseif frame and frame:IsShown() then
            ShowBoard(false)
        end
        if godOpened and godPage and godPage:IsShown() then
            godPage.Opened(state.godOpenTier)
            Refresh(false)
        end
        if opened and dial and frame:IsShown() then
            dial.Opened(state.openTier)
            Refresh(false)
        elseif opened then
            state.tier = state.openTier
        end
    elseif kind == "H" then
        if frame and frame:IsShown() then
            frame:Hide()
        end
    elseif kind == "P" then
        OnEvent(tonumber(a) or 0, tonumber(b) or 0, tonumber(c) or 0, d or "", tonumber(e) or 0)
    elseif kind == "C" then
        AnimateClaim(tonumber(b) or 0, tonumber(c) or 0, tonumber(d) or 0, tonumber(e) or 0)
    elseif kind == "X" then
        state.starting = nil
        ShowError(a, b, c)
    end
end

-- In a challenge the dead wait, no release: a wipe raises the group at the landing spot, the challenge's end brings
-- everyone home (RaidFinder.cpp). The death window is closed as it opens, and they are told why.
local function InChallenge()
    return state.challenge ~= 0
end

hooksecurefunc("StaticPopup_Show", function(which)
    if which == "DEATH" and InChallenge() then
        StaticPopup_Hide("DEATH")
    end
end)

local deathWatch = CreateFrame("Frame")
deathWatch:RegisterEvent("PLAYER_DEAD")
deathWatch:SetScript("OnEvent", function()
    if InChallenge() then
        StaticPopup_Hide("DEATH")
        UIErrorsFrame:AddMessage(TEXT.deadInChallenge, 1, 0.86, 0.55, 1)
    end
end)

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_EQUIPMENT_CHANGED")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "PLAYER_EQUIPMENT_CHANGED" then
        if frame and frame:IsShown() then
            state.gearChanged = GetTime()
        end
        return
    end
    if sender ~= UnitName("player") then
        return
    end

    if prefix == PREFIX then
        Handle(message)
    elseif prefix == "RaidFinder" then
        local kind, runState, _, counts = strsplit("\t", message)
        if kind == "S" then
            OnRaidFinderStatus(runState, counts)
        end
    end
end)
