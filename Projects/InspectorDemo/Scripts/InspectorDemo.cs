using System;
using HelixEditor;
using HelixEditor.SceneManagement;
using HelixEngine;
using HelixEngine.SceneManagement;
using Object = HelixEngine.Object;

namespace InspectorDemo
{
    public enum SpinAxis
    {
        X,
        Y,
        Z,
    }

    // A range of values (its drawer shows both on one line).
    [Serializable]
    public struct Limits
    {
        public float min;
        public float max;
    }

    // How its object turns about an axis (the turn itself comes with play mode); its fields show the attributes the
    // Inspector reads.
    [AddComponentMenu("Demo/Spinner")]
    public sealed class Spinner : GameBehaviour
    {
        [Header("Motion")]
        [Tooltip("Degrees a second")] [Range(0, 360)] public float speed = 90;
        public SpinAxis axis = SpinAxis.Y;
        public bool clockwise = true;
        public Limits wobble = new Limits { min = -5, max = 5 };

        [Space(8)]
        [Header("Look")]
        public Color tint = new Color(0.3f, 0.6f, 1f, 1f);
        [Multiline(2)] public string note = "Turns its object.";
        [Min(1)] public int[] steps = { 15, 30, 45 };
        public Transform target;
        [HideInInspector] public int turns;
    }

    // A label at its object with a size and an offset (no editor of its own: the default inspector).
    [AddComponentMenu("Demo/Marker")]
    public sealed class Marker : GameBehaviour
    {
        public string label = "Marker";
        [Range(0, 1)] public float size = 0.5f;
        public Vector3 offset = new Vector3(0, 1, 0);
        public GameObject follow;
    }

    // The Limits of a field on one line: its label, then min and max.
    [CustomPropertyDrawer(typeof(Limits))]
    public sealed class LimitsDrawer : PropertyDrawer
    {
        public override void OnGui(Rect position, SerializedProperty property, GuiContent label)
        {
            using (new EditorGui.PropertyScope(position, label, property))
            {
                Rect field = EditorGui.PrefixLabel(position, label);
                float half = (field.width - 4) / 2;
                SerializedProperty min = property.FindPropertyRelative("min"), max = property.FindPropertyRelative("max");
                min.floatValue = EditorGui.FloatField(new Rect(field.x, field.y, half, field.height), min.floatValue);
                max.floatValue = EditorGui.FloatField(new Rect(field.x + half + 4, field.y, half, field.height), Math.Max(min.floatValue, max.floatValue));
            }
        }
    }

    // The Spinner's editor: its fields, a note of the selected spinners, a button that faces them forward.
    [CustomEditor(typeof(Spinner))]
    [CanEditMultipleObjects]
    public sealed class SpinnerEditor : Editor
    {
        public override void OnInspectorGui()
        {
            DrawDefaultInspector();
            var first = (Spinner)target;
            string turn = first.speed > 0 ? $"one turn takes {360 / first.speed:0.0} s" : "it stands still";
            EditorGuiLayout.HelpBox($"{targets.Length} spinner(s) selected; {turn}.", MessageType.Info);
            if (GuiLayout.Button("Face Forward"))
                foreach (Object o in targets)
                    if (o is Spinner s)
                    {
                        s.transform.localRotation = Quaternion.identity;
                        EditorUtility.SetDirty(s.transform);
                        EditorSceneManager.MarkSceneDirty(s.gameObject.scene);
                    }
        }
    }

    // The demo's scene: opened (made and saved the first time), a spinner selected.
    public static class DemoScene
    {
        const string kPath = "Assets/Scenes/InspectorDemo.helix";

        [MenuItem("Demo/Open Inspector Demo")]
        public static void Show()
        {
            // The script assets of the classes loaded first: the scene names the scripts through them.
            foreach (string guid in AssetDatabase.FindAssets("", new[] { "Assets/Scripts" }))
                AssetDatabase.LoadAllAssetsAtPath(AssetDatabase.GUIDToAssetPath(guid));
            string full = System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Application.dataPath), kPath);
            Scene scene = System.IO.File.Exists(full) ? EditorSceneManager.OpenScene(kPath) : Make(full);
            GameObject first = GameObject.Find("Spinner A");
            if (first != null)
                Selection.activeObject = first;
            Debug.Log($"Inspector demo: {scene.name} open, {scene.rootCount} objects.");
        }

        static Scene Make(string full)
        {
            Scene scene = EditorSceneManager.NewScene(NewSceneSetup.DefaultGameObjects);
            var a = new GameObject("Spinner A");
            a.transform.localPosition = new Vector3(-2, 0, 0);
            a.AddComponent<Spinner>();
            var b = new GameObject("Spinner B");
            b.transform.localPosition = new Vector3(2, 0, 0);
            Spinner sb = b.AddComponent<Spinner>();
            sb.speed = 45;
            sb.axis = SpinAxis.X;
            sb.tint = new Color(1f, 0.55f, 0.2f, 1f);
            sb.target = a.transform;
            var lamp = new GameObject("Lamp");
            lamp.transform.localPosition = new Vector3(0, 3, 0);
            Light l = lamp.AddComponent<Light>();
            l.type = LightType.Point;
            l.color = new Color(1f, 0.9f, 0.7f, 1f);
            Marker m = lamp.AddComponent<Marker>();
            m.label = "Lamp";
            m.follow = a;
            System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(full));
            EditorSceneManager.SaveScene(scene, kPath);
            AssetDatabase.Refresh();
            return scene;
        }
    }
}
