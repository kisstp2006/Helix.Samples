using System;
using System.Collections.Generic;
using System.Globalization;
using HelixEditor;
using HelixEditor.AssetImporters;
using HelixEngine;
using Object = HelixEngine.Object;

namespace AssetDemo
{
    // A .palette file (one #rrggbb color a line) imported as a strip texture, a pixel a color.
    [ScriptedImporter(1, "palette")]
    public sealed class PaletteImporter : ScriptedImporter
    {
        public override void OnImportAsset(AssetImportContext ctx)
        {
            var colors = new List<Color>();
            string full = System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Application.dataPath), ctx.assetPath);
            foreach (string raw in System.IO.File.ReadAllLines(full))
            {
                string line = raw.Trim();
                if (line.Length == 7 && line[0] == '#' && int.TryParse(line.Substring(1), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out int rgb))
                    colors.Add(new Color(((rgb >> 16) & 255) / 255f, ((rgb >> 8) & 255) / 255f, (rgb & 255) / 255f, 1));
                else if (line.Length > 0 && line[0] != '#')
                    ctx.LogImportWarning($"'{line}' is no color");
            }
            if (colors.Count == 0)
            {
                ctx.LogImportError("a palette needs a color");
                return;
            }
            var strip = new Texture2D(colors.Count, 1) { name = System.IO.Path.GetFileNameWithoutExtension(ctx.assetPath), filterMode = FilterMode.Point };
            for (int i = 0; i < colors.Count; i++)
                strip.SetPixel(i, 0, colors[i]);
            strip.Apply();
            ctx.AddObjectToAsset("palette", strip);
            ctx.SetMainObject(strip);
        }
    }

    // The pixel art under Assets/Textures/Pixel/ keeps its pixels (point filtering for that import); every refresh's
    // changes are noted for the window.
    public sealed class PixelArtPostprocessor : AssetPostprocessor
    {
        internal static readonly List<string> s_Log = new List<string>();

        public override uint GetVersion() => 1;

        void OnPreprocessTexture()
        {
            if (!assetPath.StartsWith("Assets/Textures/Pixel/", StringComparison.Ordinal))
                return;
            var importer = (TextureImporter)assetImporter;
            importer.filterMode = FilterMode.Point;
            importer.mipmapEnabled = false;
        }

        void OnPostprocessTexture(Texture2D texture)
        {
            if (assetPath.StartsWith("Assets/Textures/Pixel/", StringComparison.Ordinal))
                s_Log.Add($"{assetPath}: {texture.width}x{texture.height}, {texture.filterMode}");
        }

        static void OnPostprocessAllAssets(string[] importedAssets, string[] deletedAssets, string[] movedAssets, string[] movedFromAssetPaths)
        {
            s_Log.Add($"refresh: {importedAssets.Length} imported, {deletedAssets.Length} deleted, {movedAssets.Length} moved");
            if (s_Log.Count > 12)
                s_Log.RemoveRange(0, s_Log.Count - 12);
        }
    }

    // Window > Asset Demo: the project's assets with their importers, previews and import hashes.
    public sealed class AssetDemoWindow : EditorWindow
    {
        Vector2 m_Scroll;

        // A tab beside the scene view.
        [MenuItem("Window/Asset Demo")]
        public static void Open() => GetWindow<AssetDemoWindow>("Asset Demo", typeof(SceneView));

        void OnGui()
        {
            GuiLayout.Label($"{AssetDatabase.FindAssets("", new[] { "Assets" }).Length} assets in the project");
            m_Scroll = GuiLayout.BeginScrollView(m_Scroll, GuiLayout.ExpandHeight(true));
            foreach (string guid in AssetDatabase.FindAssets("", new[] { "Assets" }))
            {
                string path = AssetDatabase.GUIDToAssetPath(guid);
                if (AssetDatabase.IsValidFolder(path))
                    continue;
                Type main = AssetDatabase.GetMainAssetTypeAtPath(path);
                Type importer = AssetDatabase.GetImporterType(path);
                GuiLayout.BeginHorizontal();
                Rect preview = GuiLayoutUtility.GetRect(40, 40, GuiLayout.Width(40), GuiLayout.Height(40));
                if (main == typeof(Texture2D) && AssetDatabase.LoadAssetAtPath<Texture2D>(path) is Texture2D t)
                    Gui.DrawTexture(preview, t, ScaleMode.ScaleToFit);
                else
                    Gui.Box(preview, main == null ? "-" : main.Name.Substring(0, Math.Min(4, main.Name.Length)));
                GuiLayout.BeginVertical();
                GuiLayout.Label(path);
                string hash = AssetDatabase.GetAssetDependencyHash(path).ToString();
                GuiLayout.Label($"{main?.Name ?? "no main object"} by {importer?.Name ?? "no importer"}, import {hash.Substring(0, 8)}");
                GuiLayout.EndVertical();
                GuiLayout.EndHorizontal();
            }
            GuiLayout.EndScrollView();
            GuiLayout.Label("Postprocessor:");
            foreach (string line in PixelArtPostprocessor.s_Log)
                GuiLayout.Label(line);
            if (GuiLayout.Button("Reimport the textures"))
                foreach (string guid in AssetDatabase.FindAssets("t:Texture2D", new[] { "Assets/Textures" }))
                    AssetDatabase.ImportAsset(AssetDatabase.GUIDToAssetPath(guid));
        }
    }
}
