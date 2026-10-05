using DBCD;

namespace RetailImport;

/// <summary>Prints what the retail DB2s say about an item's looks, to pick the FileDataIDs for items.json.</summary>
public static class Probe
{
    public static void Item(Retail retail, int itemId)
    {
        var sparse = retail.Table("ItemSparse");
        var name = sparse.TryGetValue(itemId, out var itemRow) ? (string)itemRow["Display_lang"] : "?";
        var expansion = itemRow != null ? (int)itemRow["ExpansionID"] : -1;
        var inventoryType = itemRow != null ? (sbyte)itemRow["InventoryType"] : -1;
        Console.WriteLine($"Item {itemId} \"{name}\" expansion {expansion} inventory type {inventoryType}");

        foreach (var modified in retail.Table("ItemModifiedAppearance").Values
                     .Where(row => (int)row["ItemID"] == itemId)
                     .OrderBy(row => (int)row["ItemAppearanceModifierID"]))
        {
            var appearanceId = (int)modified["ItemAppearanceID"];
            if (!retail.Table("ItemAppearance").TryGetValue(appearanceId, out var appearance))
                continue;
            var displayId = (int)appearance["ItemDisplayInfoID"];
            var iconId = (uint)(int)appearance["DefaultIconFileDataID"];
            Console.WriteLine($"  modifier {modified["ItemAppearanceModifierID"]} appearance {appearanceId}"
                              + $" display {displayId}"
                              + $" icon {iconId} {retail.NameOf(iconId)}");
            Display(retail, displayId);
        }
    }

    /// <summary>The items whose name holds a text, with each appearance's display: a set's pieces by its name
    /// ("Cruel Gladiator's Felweave").</summary>
    public static void Find(Retail retail, string text)
    {
        var appearances = retail.Table("ItemAppearance");
        var looks = retail.Table("ItemModifiedAppearance").Values.GroupBy(row => (int)row["ItemID"])
            .ToDictionary(group => group.Key, group => group.OrderBy(row => (int)row["ItemAppearanceModifierID"])
                .Select(row => appearances.TryGetValue((int)row["ItemAppearanceID"], out var appearance)
                    ? $"{row["ItemAppearanceModifierID"]}:{appearance["ItemDisplayInfoID"]}" : "?").ToList());
        foreach (var row in retail.Table("ItemSparse").Values
                     .Where(row => ((string)row["Display_lang"]).Contains(text, StringComparison.OrdinalIgnoreCase))
                     .OrderBy(row => row.ID))
            Console.WriteLine($"  item {row.ID} \"{row["Display_lang"]}\" inventory type {row["InventoryType"]}"
                              + $" displays {string.Join(" ", looks.GetValueOrDefault(row.ID) ?? [])}");
    }

    /// <summary>
    /// The displays drawn from files whose listfile name holds a text (a set's look: "raidwarlockmythic_r_01"), by
    /// inventory type, with the items wearing each: a whole set's pieces, the belt, boots and bracers included.
    /// </summary>
    public static void Look(Retail retail, string text)
    {
        var files = retail.FilesNamed(text);
        var textureMaterials = retail.Table("TextureFileData").Values
            .Where(row => files.Contains((uint)(int)row["FileDataID"]))
            .Select(row => (int)row["MaterialResourcesID"]).ToHashSet();
        var modelResources = retail.Table("ModelFileData").Values
            .Where(row => files.Contains((uint)row.ID)).Select(row => (uint)row["ModelResourcesID"]).ToHashSet();
        var displays = retail.Table("ItemDisplayInfoMaterialRes").Values
            .Where(row => textureMaterials.Contains((int)row["MaterialResourcesID"]))
            .Select(row => (int)row["ItemDisplayInfoID"]).ToHashSet();
        displays.UnionWith(retail.Table("ItemDisplayInfo").Values
            .Where(row => ((int[])row["ModelMaterialResourcesID"]).Any(textureMaterials.Contains)
                          || ((uint[])row["ModelResourcesID"]).Any(modelResources.Contains)).Select(row => row.ID));
        var appearances = retail.Table("ItemAppearance").Values
            .Where(row => displays.Contains((int)row["ItemDisplayInfoID"])).ToDictionary(row => row.ID);
        var sparse = retail.Table("ItemSparse");
        Console.WriteLine($"{text}: {files.Count} files, {displays.Count} displays");
        foreach (var group in retail.Table("ItemModifiedAppearance").Values
                     .Where(row => appearances.ContainsKey((int)row["ItemAppearanceID"]))
                     .Select(row => (item: (int)row["ItemID"], modifier: (int)row["ItemAppearanceModifierID"],
                         display: (int)appearances[(int)row["ItemAppearanceID"]]["ItemDisplayInfoID"]))
                     .GroupBy(entry => sparse.TryGetValue(entry.item, out var itemRow)
                         ? (sbyte)itemRow["InventoryType"] : -1)
                     .OrderBy(group => group.Key))
        {
            Console.WriteLine($"  inventory type {group.Key}:");
            foreach (var byDisplay in group.GroupBy(entry => entry.display))
            {
                var items = byDisplay.Take(3).Select(entry => $"{entry.item}/{entry.modifier} \""
                    + (sparse.TryGetValue(entry.item, out var itemRow) ? (string)itemRow["Display_lang"] : "?") + "\"");
                Console.WriteLine($"    display {byDisplay.Key} ({byDisplay.Count()} items):"
                                  + $" {string.Join(", ", items)}");
            }
        }
    }

    /// <summary>The items whose looks use a model file, to find the retail item of a listfile path.</summary>
    public static void Model(Retail retail, uint modelFileDataId)
    {
        if (!retail.Table("ModelFileData").TryGetValue((int)modelFileDataId, out var modelRow))
        {
            Console.WriteLine($"{modelFileDataId}: no ModelFileData row");
            return;
        }
        var resourcesId = (uint)modelRow["ModelResourcesID"];
        Console.WriteLine($"{modelFileDataId} {retail.NameOf(modelFileDataId)}: model resources {resourcesId}");
        var displays = retail.Table("ItemDisplayInfo").Values
            .Where(row => ((uint[])row["ModelResourcesID"]).Contains(resourcesId)).Select(row => row.ID).ToHashSet();
        var appearances = retail.Table("ItemAppearance").Values
            .Where(row => displays.Contains((int)row["ItemDisplayInfoID"])).ToDictionary(row => row.ID);
        var sparse = retail.Table("ItemSparse");
        foreach (var modified in retail.Table("ItemModifiedAppearance").Values
                     .Where(row => appearances.ContainsKey((int)row["ItemAppearanceID"])))
        {
            var itemId = (int)modified["ItemID"];
            var name = sparse.TryGetValue(itemId, out var itemRow) ? (string)itemRow["Display_lang"] : "?";
            var level = itemRow != null ? (ushort)itemRow["ItemLevel"] : 0;
            Console.WriteLine($"  item {itemId} \"{name}\" ilvl {level} modifier {modified["ItemAppearanceModifierID"]}"
                              + $" display {appearances[(int)modified["ItemAppearanceID"]]["ItemDisplayInfoID"]}");
        }
    }

    /// <summary>
    /// The items whose looks use a texture file, as a body texture or a model's: to find the retail items of the
    /// pieces a set's textures belong to (a tier set's belt, boots and bracers are other items sharing its look).
    /// </summary>
    public static void Texture(Retail retail, uint textureFileDataId)
    {
        var materials = retail.Table("TextureFileData").Values
            .Where(row => (uint)(int)row["FileDataID"] == textureFileDataId)
            .Select(row => (int)row["MaterialResourcesID"]).ToHashSet();
        Console.WriteLine($"{textureFileDataId} {retail.NameOf(textureFileDataId)}: material resources"
                          + $" {string.Join(",", materials)}");
        var displays = retail.Table("ItemDisplayInfoMaterialRes").Values
            .Where(row => materials.Contains((int)row["MaterialResourcesID"]))
            .Select(row => (int)row["ItemDisplayInfoID"]).ToHashSet();
        displays.UnionWith(retail.Table("ItemDisplayInfo").Values
            .Where(row => ((int[])row["ModelMaterialResourcesID"]).Any(materials.Contains)).Select(row => row.ID));
        var appearances = retail.Table("ItemAppearance").Values
            .Where(row => displays.Contains((int)row["ItemDisplayInfoID"])).ToDictionary(row => row.ID);
        var sparse = retail.Table("ItemSparse");
        foreach (var modified in retail.Table("ItemModifiedAppearance").Values
                     .Where(row => appearances.ContainsKey((int)row["ItemAppearanceID"])))
        {
            var itemId = (int)modified["ItemID"];
            var name = sparse.TryGetValue(itemId, out var itemRow) ? (string)itemRow["Display_lang"] : "?";
            var inventoryType = itemRow != null ? (sbyte)itemRow["InventoryType"] : -1;
            Console.WriteLine($"  item {itemId} \"{name}\" inventory type {inventoryType} modifier"
                              + $" {modified["ItemAppearanceModifierID"]}"
                              + $" display {appearances[(int)modified["ItemAppearanceID"]]["ItemDisplayInfoID"]}");
        }
    }

    public static void Display(Retail retail, int displayId)
    {
        if (!retail.Table("ItemDisplayInfo").TryGetValue(displayId, out var display))
            return;
        var models = (uint[])display["ModelResourcesID"];
        var materials = (int[])display["ModelMaterialResourcesID"];
        Console.WriteLine($"    display {displayId}: ItemVisual {display["ItemVisual"]}"
                          + $" ParticleColor {display["ParticleColorID"]} Flags 0x{(int)display["Flags"]:X}"
                          + $" Geosets [{string.Join(",", (int[])display["GeosetGroup"])}]");
        for (var slot = 0; slot < 2; ++slot)
        {
            if (models[slot] != 0)
                foreach (var model in ModelFiles(retail, models[slot]))
                    Console.WriteLine($"      model[{slot}] res {models[slot]}: {model.fdid}"
                                      + $" {retail.NameOf(model.fdid)} {model.component}");
            if (materials[slot] != 0)
                foreach (var texture in TextureFiles(retail, materials[slot]))
                    Console.WriteLine($"      texture[{slot}] res {materials[slot]}: {texture.fdid}"
                                      + $" {retail.NameOf(texture.fdid)} {texture.component}");
        }
        // The body textures: one material per component section (the character texture's regions)
        foreach (var (section, materialId) in BodyMaterials(retail, displayId))
            foreach (var texture in TextureFiles(retail, materialId))
                Console.WriteLine($"      body {SectionNames[section]} res {materialId}: {texture.fdid}"
                                  + $" {retail.NameOf(texture.fdid)} {texture.component}");
    }

    /// <summary>
    /// ItemDisplayInfoMaterialRes.ComponentSection: the character texture's regions, in the order of 3.3.5
    /// ItemDisplayInfo.dbc's Texture[8] fields (ArmUpper, ArmLower, Hand, TorsoUpper, TorsoLower, LegUpper, LegLower,
    /// Foot); retail adds Accessory (8) and ScalpUpper / ScalpLower (9, 10), which 3.3.5 has no region for.
    /// </summary>
    public static readonly string[] SectionNames =
    [
        "ArmUpper", "ArmLower", "Hand", "TorsoUpper", "TorsoLower", "LegUpper", "LegLower", "Foot", "Accessory",
        "ScalpUpper", "ScalpLower",
    ];

    public static IEnumerable<(int section, int materialId)> BodyMaterials(Retail retail, int displayId) =>
        retail.Table("ItemDisplayInfoMaterialRes").Values
            .Where(row => (int)row["ItemDisplayInfoID"] == displayId)
            .Select(row => ((int)(sbyte)row["ComponentSection"], (int)row["MaterialResourcesID"]))
            .Where(entry => entry.Item1 >= 0 && entry.Item1 < SectionNames.Length)
            .OrderBy(entry => entry.Item1);

    public static IEnumerable<(uint fdid, string component)> ModelFiles(Retail retail, uint resourcesId)
    {
        var components = retail.Table("ComponentModelFileData");
        foreach (var row in retail.Table("ModelFileData").Values
                     .Where(row => (uint)row["ModelResourcesID"] == resourcesId))
        {
            var fdid = (uint)(int)row["FileDataID"];
            var component = components.TryGetValue((int)fdid, out var c)
                ? $"race {c["RaceID"]} gender {c["GenderIndex"]} class {c["ClassID"]} position {c["PositionIndex"]}"
                : "";
            yield return (fdid, component);
        }
    }

    public static IEnumerable<(uint fdid, string component)> TextureFiles(Retail retail, int materialResourcesId)
    {
        var components = retail.Table("ComponentTextureFileData");
        foreach (var row in retail.Table("TextureFileData").Values
                     .Where(row => (int)row["MaterialResourcesID"] == materialResourcesId))
        {
            var fdid = (uint)(int)row["FileDataID"];
            var component = components.TryGetValue((int)fdid, out var c)
                ? $"race {c["RaceID"]} gender {c["GenderIndex"]} class {c["ClassID"]}"
                : $"usage {row["UsageType"]}";
            yield return (fdid, component);
        }
    }

    /// <summary>The UI atlases whose name holds a text: the texture file and the pixel rectangle of each</summary>
    public static void Atlas(Retail retail, string text)
    {
        var atlases = retail.Table("UiTextureAtlas");
        var elements = retail.Table("UiTextureAtlasElement");
        foreach (var member in retail.Table("UiTextureAtlasMember").Values)
        {
            if (!elements.TryGetValue(Convert.ToInt32(member["UiTextureAtlasElementID"]), out var element))
                continue;
            var name = (string)element["Name"];
            if (!name.Contains(text, StringComparison.OrdinalIgnoreCase))
                continue;
            atlases.TryGetValue(Convert.ToInt32(member["UiTextureAtlasID"]), out var atlas);
            var file = atlas != null ? Convert.ToUInt32(atlas["FileDataID"]) : 0;
            Console.WriteLine($"  {name} file {file} {retail.NameOf(file)} rect {member["CommittedLeft"]},"
                              + $"{member["CommittedTop"]}-{member["CommittedRight"]},{member["CommittedBottom"]}"
                              + (atlas != null ? $" of {atlas["AtlasWidth"]}x{atlas["AtlasHeight"]}" : ""));
        }
    }
}
