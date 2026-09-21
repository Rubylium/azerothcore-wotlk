-- ============================================================================
-- Author: Noa
-- ============================================================================
-- Contains the data structures for the game races and classes

--FR
local Races_Informations = {
[1] = {
      Name = "Humain",
	  Description = "Les humains sont une race jeune et, de ce fait, extrêmement polyvalente. Ils maîtrisent les arts du combat, de l'artisanat et de la magie avec une efficacité surprenante. Leur courage et leur optimisme les ont menés à bâtir certains des royaumes les plus splendides du monde. En cette ère troublée, après des générations de conflits, les humains veulent retrouver la gloire qui les distinguait autrefois et se forger un avenir nouveau et radieux.",
	  Spell_1 = {name = "Chaque homme pour soi", icon = "spell_shadow_charm", description = "Supprime tous les effets qui entravent le déplacement ainsi que tous les effets qui vous font perdre le contrôle de votre personnage."},
	  Spell_2 = {name = "Spécialisation Épées", icon = "ability_meleedamage", description = "L'expertise avec les épées et les épées à deux mains augmente de 3."},
	  Spell_3 = {name = "Spécialisation Masses", icon = "inv_hammer_05", description = "L'expertise avec les masses et les masses à deux mains augmente de 3."},
	  Spell_4 = {name = "Esprit humain", icon = "inv_enchant_shardbrilliantsmall", description = "Esprit augmenté de 3%."},
	  Spell_5 = {name = "Perception", icon = "spell_nature_sleep", description = "Augmente votre détection du Camouflage."},
	  Spell_6 = {name = "Diplomatie", icon = "inv_misc_note_02", description = "Les gains de réputation augmentent de 10%."},
	 },	 
[2] = {
      Name = "Nain",
	  Description = "Par le passé, les nains ne se souciaient que des richesses arrachées aux entrailles de la terre. C'est ainsi qu'ils mirent au jour les vestiges d'une race divine qui, semble-t-il, leur a donné la vie... et un héritage enchanté. Poussés par cette découverte à en apprendre davantage, les nains se sont consacrés à la quête des artefacts perdus et du savoir arcanique. Aujourd'hui, on trouve des archéologues nains aux quatre coins du monde.",
	  Spell_1 = {name = "Spécialisation Masses", icon = "inv_hammer_05", description = "L'expertise avec les masses et les masses à deux mains augmente de 5."},
	  Spell_2 = {name = "Peau de pierre", icon = "spell_shadow_unholystrength", description = "Supprime tous les effets de poison, de maladie et de saignement, et augmente votre armure de 10% pendant 0.1 seconde."},
	  Spell_3 = {name = "Spécialisation armes à feu", icon = "inv_musket_03", description = "Vos chances de coup critique avec les armes à feu augmentent de 1%."},
	  Spell_4 = {name = "Résistance au Givre", icon = "spell_frost_wizardmark", description = "Réduit de 2% les chances d'être touché par les sorts de Givre."},
	  Spell_5 = {name = "Trouver un trésor", icon = "racial_dwarf_findtreasure", description = "Permet au nain de repérer les trésors proches en les affichant sur la mini-carte. Dure jusqu'à annulation."},
	 },	 
[3] = {
      Name = "Elfe de la nuit",
	  Description = "Il y a dix mille ans, les elfes de la nuit fondèrent un vaste empire, mais l'usage inconsidéré de la magie originelle causa leur perte. Consternés, ils se retirèrent dans les forêts où ils vécurent isolés jusqu'au retour de leur ancien ennemi : la Légion ardente. Ils n'eurent alors d'autre choix que de sortir de leur réclusion et de lutter pour leur place dans le nouveau monde.",
	  Spell_1 = {name = "Ombreterre", icon = "ability_ambush", description = "Activez pour vous fondre dans les ombres, ce qui réduit les chances d'être détecté par les ennemis. Dure jusqu'à annulation ou jusqu'à ce que vous bougiez. À l'annulation, toute la menace est rétablie envers les ennemis encore en combat."},
	  Spell_2 = {name = "Insaisissable", icon = "ability_racial_ultravision", description = "Réduit les chances d'être détecté lorsque vous êtes sous l'effet de Camouflage ou d'Ombreterre."},
	  Spell_3 = {name = "Résistance à la Nature", icon = "spell_nature_spiritarmor", description = "Réduit de 2% les chances d'être touché par les sorts de Nature."},
	  Spell_4 = {name = "Vivacité", icon = "ability_racial_shadowmeld", description = "Réduit de 2% les chances d'être touché par les attaques de mêlée et à distance."},
	  Spell_5 = {name = "Esprit de feu follet", icon = "spell_nature_wispsplode", description = "Vous vous transformez en feu follet à votre mort, ce qui augmente votre vitesse de 75%."},  
	 },
[4] = {
      Name = "Gnome",
	  Description = "Malgré leur petite taille, les gnomes de Khaz Modan ont mis leur prodigieux intellect au service de leur place dans l'Histoire. Leur royaume souterrain, Gnomeregan, était sans nul doute une merveille de technologie à vapeur. Ils n'en ont pas moins perdu la cité lors d'une invasion massive de troggs. Aujourd'hui, les créateurs de cette merveille errent sur les terres des nains, qu'ils aident de leur mieux.",
	  Spell_1 = {name = "Artiste de l'évasion", icon = "ability_rogue_trip", description = "Vous échappez à tout effet qui vous immobilise ou réduit votre vitesse de déplacement."},
	  Spell_2 = {name = "Résistance aux Arcanes", icon = "spell_nature_wispsplode", description = "Réduit de 2% les chances d'être touché par les sorts des Arcanes."},
	  Spell_3 = {name = "Ouverture d'esprit", icon = "inv_enchant_essenceeternallarge", description = "Intelligence augmentée de 5%."},
	  Spell_4 = {name = "Spécialisation Ingénierie", icon = "inv_misc_gear_01", description = "Compétence en Ingénierie augmentée de 15."},  
	 },
[5] = {
      Name = "Draeneï",
	  Description = "Loin d'Argus, leur foyer, les honorables draeneï ont fui la Légion ardente pendant des éons avant de trouver une planète reculée où s'établir. Ils partagèrent ce monde avec les orcs chamaniques et le nommèrent Draenor. Avec le temps, la Légion corrompit les orcs, qui firent la guerre et faillirent exterminer les paisibles draeneï. Quelques rares rescapés gagnèrent Azeroth, où ils cherchent désormais des alliés dans leur combat contre la Légion ardente.",
	  Spell_1 = {name = "Don des Naaru", icon = "spell_holy_holyprotection", description = "Soigne la cible pendant 15 s. Le montant des soins augmente avec votre puissance d'attaque."},
	  Spell_2 = {name = "Taille de gemmes", icon = "spell_misc_conjuremanajewel", description = "Compétence en Joaillerie augmentée de 5."},
	  Spell_3 = {name = "Présence héroïque", icon = "inv_helmet_21", description = "Augmente de 1% les chances de toucher avec toutes les attaques et tous les sorts, pour vous et tous les membres de votre groupe dans un rayon de 30 m."},
	  Spell_4 = {name = "Résistance à l'Ombre", icon = "spell_shadow_detectinvisibility", description = "Réduit de 2% les chances d'être touché par les sorts d'Ombre."},
	 },
[6] = {
      Name = "Orc",
	  Description = "La race des orcs est originaire de la planète Draenor. Ce peuple pacifique, aux croyances chamaniques, fut asservi par la Légion ardente et contraint de prendre part à la guerre contre les humains d'Azeroth. Il fallut de longues années, mais les orcs finirent par échapper à la corruption des démons et par recouvrer leur liberté. Aujourd'hui, ils se battent pour leur honneur dans un monde qui les hait et les méprise.",
	  Spell_1 = {name = "Fureur sanguinaire", icon = "racial_orc_berserkerstrength", description = "Augmente la puissance d'attaque de 6%. Dure 15 s."},
	  Spell_2 = {name = "Commandement", icon = "ability_warrior_warcry", description = "Les dégâts infligés par les familiers augmentent de 5%."},
	  Spell_3 = {name = "Robustesse", icon = "inv_helmet_23", description = "La durée des étourdissements est réduite de 15% supplémentaires."},
	  Spell_4 = {name = "Spécialisation Haches", icon = "inv_axe_02", description = "L'expertise avec les armes de pugilat, les haches et les haches à deux mains augmente de 5."},
	 },
[7] = {
      Name = "Mort-vivant",
      Description = "Hors de portée du Roi-liche, les Réprouvés cherchent le moyen de le renverser. La banshee Sylvanas mène leur soif de vengeance contre le Fléau. Les humains sont eux aussi devenus des ennemis, implacables dans leur volonté de purger le monde des morts-vivants. Les Réprouvés se soucient peu de leurs propres alliés ; pour eux, la Horde n'est qu'un outil au service de leurs sombres desseins.",
	  Spell_1 = {name = "Cannibalisme", icon = "ability_racial_cannibalize", description = "Une fois activé, régénère 7% de la santé totale toutes les 2 s pendant 10 s. Ne fonctionne qu'avec les cadavres d'humanoïdes ou de morts-vivants à moins de 5 m."},
	  Spell_2 = {name = "Volonté des Réprouvés", icon = "spell_shadow_raisedead", description = "Supprime tout effet de Charme, de Peur ou de Sommeil. Cet effet partage un temps de recharge de 45 s avec les effets similaires."},
	  Spell_3 = {name = "Résistance à l'Ombre", icon = "spell_shadow_detectinvisibility", description = "Réduit de 2% les chances d'être touché par les sorts d'Ombre."},
	  Spell_4 = {name = "Respiration aquatique", icon = "spell_shadow_demonbreath", description = "La durée de la respiration sous l'eau augmente de 233%."}, 
	 },
[8] = {
      Name = "Tauren",
	  Description = "Les taurens s'efforcent sans relâche de préserver l'équilibre de la Nature et de respecter les volontés de la déesse qu'ils vénèrent, la Terre-Mère. Récemment attaqués par de redoutables centaures, ils auraient été anéantis sans une rencontre fortuite avec les orcs, qui les aidèrent à vaincre les intrus. Pour honorer cette dette de sang, les taurens ont rejoint la Horde, scellant l'amitié des deux races.",
	  Spell_1 = {name = "Choc de guerre", icon = "ability_warstomp", description = "Étourdit jusqu'à 5 ennemis dans un rayon de 8 m pendant 2 s."},
	  Spell_2 = {name = "Vigueur d'endurance", icon = "spell_nature_unyeildingstamina", description = "La santé de base augmente de 5%."},
	  Spell_3 = {name = "Résistance à la Nature", icon = "spell_nature_spiritarmor", description = "Réduit de 2% les chances d'être touché par les sorts de Nature."},
	  Spell_4 = {name = "Culture", icon = "inv_misc_flower_01", description = "Compétence en Herboristerie augmentée de 15."},
	 },
[9] = {
      Name = "Troll",
	  Description = "Les féroces trolls de la tribu Sombrelance peuplaient les jungles de Strangleronce jusqu'à ce que des factions guerrières les en chassent. Avec le temps, les trolls se lièrent d'amitié avec la Horde des orcs, et Thrall, le jeune chef de guerre orc, les convainquit de le suivre en Kalimdor. Malgré leur héritage foncièrement sombre, les trolls Sombrelance occupent une place privilégiée au sein de la Horde.",
	  Spell_1 = {name = "Berserker", icon = "racial_troll_berserk", description = "Augmente votre vitesse d'attaque et d'incantation de 20% pendant 10 s."},
	  Spell_2 = {name = "Régénération", icon = "spell_nature_regenerate", description = "Le taux de régénération de la santé augmente de 10%. 10% de la régénération totale se poursuit pendant le combat."},
	  Spell_3 = {name = "Tueur de bêtes", icon = "inv_misc_pelt_bear_ruin_02", description = "Les dégâts infligés aux Bêtes augmentent de 5%."},
	  Spell_4 = {name = "Spécialisation armes de jet", icon = "inv_throwingaxe_03", description = "Vos chances de coup critique avec les armes de jet augmentent de 1%."}, 
	  Spell_5 = {name = "Spécialisation Arcs", icon = "inv_weapon_bow_12", description = "Vos chances de coup critique avec les arcs augmentent de 1%."},
	  Spell_6 = {name = "La danse vaudou", icon = "inv_misc_idol_02", description = "Réduit de 15% la durée de tous les effets qui entravent le déplacement. Les trolls s'échappent toujours, mon !"},
	 },
[10] = {
      Name = "Elfe de sang",
	  Description = "Il y a bien longtemps, les hauts-elfes exilés fondèrent Quel'Thalas et y créèrent une source magique, la Source de soleil. Si ses pouvoirs les rendirent plus forts, ils en devinrent aussi profondément dépendants.\n\nDes années plus tard, le Fléau mort-vivant détruisit la Source de soleil et la quasi-totalité de la population des hauts-elfes. Aujourd'hui connus sous le nom d'elfes de sang, ces réfugiés dispersés tentent de reconstruire Quel'Thalas tout en cherchant une nouvelle source magique pour apaiser leur douloureuse dépendance.",
	  Spell_1 = {name = "Torrent arcanique", icon = "spell_shadow_teleport", description = "Réduit au silence tous les ennemis dans un rayon de 8 m pendant 2 s et restaure 6% de votre mana. Interrompt en outre l'incantation des cibles non-joueurs pendant 3 s."},
	  Spell_2 = {name = "Affinité arcanique", icon = "inv_enchant_shardglimmeringlarge", description = "Compétence en Enchantement augmentée de 10."},
	  Spell_3 = {name = "Résistance à la magie", icon = "spell_shadow_antimagicshell", description = "Réduit de 2% les chances d'être touché par les sorts."},
	 },
}

local Class_Informations = {
[1] = {
      Name = "Guerrier",
      Description = "Les guerriers sont les maîtres du combat au corps à corps, capables de manier une grande variété d'armes et d'armures. Leur force et leur endurance en font des tanks redoutables, à même de protéger leurs alliés tout en infligeant des dégâts dévastateurs à leurs ennemis.",
      Roles = "Dégâts de mêlée, Tank.",
	 },
[2] = {
      Name = "Paladin",
      Description = "Les paladins sont des guerriers sacrés qui allient le combat au corps à corps à la magie divine. Voués à la justice et à la protection des innocents, ils peuvent soigner leurs alliés, les couvrir de bénédictions et châtier les impies par la puissance sacrée.",
      Roles = "Dégâts de mêlée, Tank, Soigneur.",
	 },
[3] = {
      Name = "Chasseur",
      Description = "Les chasseurs sont les maîtres du combat à distance et de la survie en pleine nature. Accompagnés de leurs fidèles compagnons animaux, ils pistent leurs ennemis, posent des pièges et frappent de loin à l'arc ou à l'arme à feu.",
      Roles = "Dégâts à distance.",
	 },
[4] = {
      Name = "Voleur",
      Description = "Les voleurs sont les maîtres de la discrétion et de la ruse, capables de se déplacer sans être détectés et de frapper depuis l'ombre. Leur agilité et leur dextérité leur permettent d'infliger des coups critiques mortels tout en évitant les attaques ennemies par des mouvements vifs.",
      Roles = "Dégâts de mêlée.",
	 },
[5] = {
      Name = "Prêtre",
      Description = "Les prêtres sont les maîtres de la magie divine, voués à soigner et à protéger leurs alliés. S'ils peuvent aussi canaliser des pouvoirs d'ombre, leur véritable force réside dans leur capacité à restaurer la vie et à offrir une protection spirituelle.",
      Roles = "Dégâts à distance, Soigneur.",
	 },
[6] = {
      Name = "Chevalier de la mort",
      Description = "Les chevaliers de la mort sont des guerriers morts-vivants qui ont maîtrisé les arts nécromantiques. Jadis serviteurs du Roi-liche, ils combattent désormais de leur propre volonté, alliant talents martiaux, magie d'ombre et pouvoirs sur la mort.",
      Roles = "Dégâts de mêlée, Tank.",
	  },
[7] = {
      Name = "Chaman",
      Description = "Les chamans sont les intermédiaires entre le monde des esprits et le monde physique, capables de canaliser les éléments et de communier avec les esprits. Ils peuvent soigner leurs alliés, invoquer de puissants totems et déchaîner la furie des éléments sur leurs ennemis.",
      Roles = "Dégâts de mêlée, Dégâts à distance, Soigneur.",
	  },
[8] = {
      Name = "Mage",
      Description = "Les mages sont les maîtres des arts arcaniques, capables de canaliser de puissants sorts élémentaires. Bien que physiquement fragiles, leur maîtrise de la magie en fait une force dévastatrice sur le champ de bataille, à même de commander la glace, le feu et les forces des Arcanes.",
      Roles = "Dégâts à distance.",
	  },
[9] = {
      Name = "Démoniste",
      Description = "Les démonistes ont scellé des pactes avec des forces démoniaques pour obtenir le pouvoir. Maîtres de la magie d'ombre et de la magie gangrenée, ils invoquent des démons, drainent la vie de leurs ennemis et canalisent des énergies corruptrices pour ravager le champ de bataille.",
      Roles = "Dégâts à distance.",
	  },
[10] = {
      Name = "Druide",
      Description = "Les druides sont les gardiens de la nature, capables de se transformer en différentes formes animales. Leur polyvalence leur permet d'assumer de multiples rôles : ils peuvent soigner comme des prêtres, encaisser comme des guerriers ou infliger des dégâts comme des mages, tout en gardant leur lien avec le monde naturel.",
      Roles = "Dégâts de mêlée, Dégâts à distance, Tank, Soigneur.",
      },
};

local RaceTooltipPositions = {
    Alliance = {
        [1] = { -- Human
            high = {point = "TOPLEFT", relPoint = "TOPRIGHT", x = 20, y = 20},
            low = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -80},
            veryLow = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -180},
            default = {point = "LEFT", relPoint = "RIGHT", x = 20, y = 0}
        },
        [2] = { -- Dwarf
            high = {point = "TOPLEFT", relPoint = "TOPRIGHT", x = 20, y = 40},
            low = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -60},
            veryLow = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -140},
            default = {point = "LEFT", relPoint = "RIGHT", x = 20, y = 20}
        },
        [3] = { -- Night elf
            high = {point = "TOPLEFT", relPoint = "TOPRIGHT", x = 20, y = 20},
            low = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -80},
            veryLow = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -180},
            default = {point = "LEFT", relPoint = "RIGHT", x = 20, y = 0}
        },
        [4] = { -- Gnome
            high = {point = "TOPLEFT", relPoint = "TOPRIGHT", x = 20, y = 40},
            low = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -60},
            veryLow = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -140},
            default = {point = "LEFT", relPoint = "RIGHT", x = 20, y = 20}
        },
        [5] = { -- Draenei
            high = {point = "TOPLEFT", relPoint = "TOPRIGHT", x = 20, y = 20},
            low = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -80},
            veryLow = {point = "BOTTOMLEFT", relPoint = "TOPRIGHT", x = 20, y = -180},
            default = {point = "LEFT", relPoint = "RIGHT", x = 20, y = 0}
        }
    },
    Horde = {
        [6] = { -- Orc
            high = {point = "TOPRIGHT", relPoint = "TOPLEFT", x = -20, y = 20},
            veryLow = {point = "BOTTOMRIGHT", relPoint = "TOPLEFT", x = -20, y = -120},
            default = {point = "RIGHT", relPoint = "LEFT", x = -20, y = 0}
        },
        [7] = { -- Undead
            high = {point = "TOPRIGHT", relPoint = "TOPLEFT", x = -20, y = 40},
            veryLow = {point = "BOTTOMRIGHT", relPoint = "TOPLEFT", x = -20, y = -80},
            default = {point = "RIGHT", relPoint = "LEFT", x = -20, y = 20}
        },
        [8] = { -- Tauren
            high = {point = "TOPRIGHT", relPoint = "TOPLEFT", x = -20, y = 20},
            veryLow = {point = "BOTTOMRIGHT", relPoint = "TOPLEFT", x = -20, y = -120},
            default = {point = "RIGHT", relPoint = "LEFT", x = -20, y = 0}
        },
        [9] = { -- Troll
            high = {point = "TOPRIGHT", relPoint = "TOPLEFT", x = -20, y = 40},
            veryLow = {point = "BOTTOMRIGHT", relPoint = "TOPLEFT", x = -20, y = -80},
            default = {point = "RIGHT", relPoint = "LEFT", x = -20, y = 20}
        },
        [10] = { -- Blood elf
            high = {point = "TOPRIGHT", relPoint = "TOPLEFT", x = -20, y = 20},
            veryLow = {point = "BOTTOMRIGHT", relPoint = "TOPLEFT", x = -20, y = -120},
            default = {point = "RIGHT", relPoint = "LEFT", x = -20, y = 0}
        }
    }
}

function GetRaceTooltipPosition(raceID, button)
    local screenHeight = GetScreenHeight()
    local buttonTop = button:GetTop()
    local buttonBottom = button:GetBottom()

    local faction = raceID <= 5 and "Alliance" or "Horde"
    local positions = RaceTooltipPositions[faction][raceID]
    
    if not positions then
        return {point = "CENTER", relPoint = "CENTER", x = 0, y = 0}
    end

    if buttonTop > screenHeight * 0.66 then
        return positions.high
    elseif buttonBottom < screenHeight * 0.44 and positions.low then
        return positions.low
    elseif buttonBottom < screenHeight * 0.33 and positions.veryLow then
        return positions.veryLow
    else
        return positions.default
    end
end

local RACE_DATA = {
    [1]  = { glueString = "HUMAN",     faction = "Alliance" },
    [2]  = { glueString = "DWARF",     faction = "Alliance" },
    [3]  = { glueString = "NIGHT_ELF", faction = "Alliance" },
    [4]  = { glueString = "GNOME",     faction = "Alliance" },
    [5]  = { glueString = "DRAENEI",   faction = "Alliance" },
    [6]  = { glueString = "ORC",       faction = "Horde" },
    [7]  = { glueString = "UNDEAD",    faction = "Horde" },
    [8]  = { glueString = "TAUREN",    faction = "Horde" },
    [9]  = { glueString = "TROLL",     faction = "Horde" },
    [10] = { glueString = "BLOOD_ELF", faction = "Horde" },
}

local ALLIANCE_RACES = {1, 2, 3, 4, 5}
local HORDE_RACES = {6, 7, 8, 9, 10}

local function GetRaceName(raceID)
    local raceData = RACE_DATA[raceID]
    if not raceData then
        return "Human"
    end

    return _G[raceData.glueString] or raceData.glueString
end

local function GetFactionForRaceID(raceID)
    local raceData = RACE_DATA[raceID]
    return raceData and raceData.faction or "Alliance"
end

local function GetRaceNamesByFaction(faction)
    local names = {}
    local raceList = (faction == "Alliance") and ALLIANCE_RACES or HORDE_RACES
    
    for _, raceID in ipairs(raceList) do
        table.insert(names, GetRaceName(raceID))
    end
    
    return names
end

local AllianceRaces = GetRaceNamesByFaction("Alliance")
local HordeRaces = GetRaceNamesByFaction("Horde")

local function GetCurrentRaceName()
    local raceID = GetSelectedRace()
    return GetRaceName(raceID)
end

local function GetRacesByFaction(allowedRaces)
    local allianceList = {}
    local hordeList = {}
    
    for _, race in ipairs(allowedRaces) do
        local isAlliance = false
        for _, allianceRace in ipairs(AllianceRaces) do
            if race == allianceRace then
                table.insert(allianceList, race)
                isAlliance = true
                break
            end
        end
        
        if not isAlliance then
            for _, hordeRace in ipairs(HordeRaces) do
                if race == hordeRace then
                    table.insert(hordeList, race)
                    break
                end
            end
        end
    end
    
    return allianceList, hordeList
end

local RACE_NAME_CACHE = {}

local function GetFactionForRaceName(raceName)
    if not raceName then return "Horde" end

    if RACE_NAME_CACHE[raceName] then
        return RACE_NAME_CACHE[raceName]
    end
    
    local currentLocale = GetLocale()
    local faction = nil

    for _, raceData in pairs(RACE_DATA) do
        local baseKey = raceData.glueString

        for _, genderKey in ipairs({"_MALE", "_FEMALE"}) do
            local fullKey = baseKey .. genderKey
            local localizedName = _G[fullKey]
            
            if localizedName and localizedName == raceName then
                faction = raceData.faction
                break
            end
        end
        
        if faction then
            break
        end
    end

    if not faction then
        faction = "Horde"
    end

    RACE_NAME_CACHE[raceName] = faction
    return faction
end

_G.RACE_1 = "Humain"
_G.RACE_2 = "Nain"
_G.RACE_3 = "Elfe de la nuit"
_G.RACE_4 = "Gnome"
_G.RACE_5 = "Draeneï"
_G.RACE_6 = "Orc"
_G.RACE_7 = "Mort-vivant"
_G.RACE_8 = "Tauren"
_G.RACE_9 = "Troll"
_G.RACE_10 = "Elfe de sang"

_G.Races_Informations = Races_Informations
_G.Class_Informations = Class_Informations
_G.ClassRaces = ClassRaces or {}
_G.GetRaceTooltipPosition = GetRaceTooltipPosition

_G.GetRaceName = GetRaceName
_G.GetFactionForRaceID = GetFactionForRaceID
_G.GetRaceNamesByFaction = GetRaceNamesByFaction
_G.GetCurrentRaceName = GetCurrentRaceName
_G.GetRacesByFaction = GetRacesByFaction

_G.ALLIANCE_RACES = ALLIANCE_RACES
_G.HORDE_RACES = HORDE_RACES
_G.RACE_DATA = RACE_DATA
_G.AllianceRaces = AllianceRaces
_G.HordeRaces = HordeRaces

_G.GetFactionForRaceName = GetFactionForRaceName