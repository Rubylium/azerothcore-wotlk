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
    }

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
}
