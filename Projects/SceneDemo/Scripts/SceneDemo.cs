using System;
using System.Collections.Generic;
using HelixEditor;
using HelixEditor.EditorTools;
using HelixEditor.Overlays;
using HelixEditor.SceneManagement;
using HelixEngine;
using HelixEngine.Rendering.Physical;
using HelixEngine.SceneManagement;
using HelixEngine.UI;
using Object = HelixEngine.Object;

namespace SceneDemo
{
    // A walk along waypoints (the walk itself comes with play mode): its gizmos show the path, selected also each
    // waypoint's reach.
    [AddComponentMenu("Demo/Patrol")]
    public sealed class Patrol : GameBehaviour
    {
        public Vector3[] waypoints = { new Vector3(-2, 0, 2), new Vector3(2, 0, 2), new Vector3(2, 0, -1) };
        public Color color = new Color(1f, 0.75f, 0.2f, 1f);
        [Range(0.1f, 2)] public float reach = 0.6f;
        public bool loop = true;

        public Vector3 WorldPoint(int i) => transform.position + waypoints[i];

        void OnDrawGizmos()
        {
            if (waypoints == null || waypoints.Length == 0)
                return;
            Gizmos.color = color;
            for (int i = 0; i < waypoints.Length; i++)
            {
                Gizmos.DrawSphere(WorldPoint(i), 0.12f);
                if (i + 1 < waypoints.Length)
                    Gizmos.DrawLine(WorldPoint(i), WorldPoint(i + 1));
            }
            if (loop && waypoints.Length > 2)
                Gizmos.DrawLine(WorldPoint(waypoints.Length - 1), WorldPoint(0));
            Gizmos.DrawIcon(transform.position + Vector3.up * 1.2f, "ComponentScript");
        }

        void OnDrawGizmosSelected()
        {
            if (waypoints == null)
                return;
            Gizmos.color = new Color(color.r, color.g, color.b, 0.4f);
            for (int i = 0; i < waypoints.Length; i++)
                Gizmos.DrawWireSphere(WorldPoint(i), reach);
        }

        /// <summary>A waypoint at a world point (the end of the path).</summary>
        public void Add(Vector3 world)
        {
            var list = new List<Vector3>(waypoints ?? Array.Empty<Vector3>()) { world - transform.position };
            waypoints = list.ToArray();
        }
    }

    // The Patrol's handles in the Scene view: a position handle and a label per waypoint while it is selected.
    [CustomEditor(typeof(Patrol))]
    public sealed class PatrolEditor : Editor
    {
        void OnSceneGui()
        {
            var patrol = (Patrol)target;
            if (patrol.waypoints == null)
                return;
            for (int i = 0; i < patrol.waypoints.Length; i++)
            {
                Vector3 at = patrol.WorldPoint(i);
                Vector3 moved = Handles.PositionHandle(at, Quaternion.identity);
                if (moved != at)
                {
                    patrol.waypoints[i] = moved - patrol.transform.position;
                    Changed(patrol);
                }
                Handles.color = Color.white;
                Handles.Label(at + Vector3.up * 0.45f, $"#{i + 1}");
            }
        }

        internal static void Changed(Patrol patrol)
        {
            EditorUtility.SetDirty(patrol);
            EditorSceneManager.MarkSceneDirty(patrol.gameObject.scene);
        }
    }

    // A tool of the Patrol (in the Tools overlay while one is selected): a click on the ground adds a waypoint, the
    // place it would go shown under the pointer.
    [EditorTool("Waypoint Tool", typeof(Patrol))]
    public sealed class WaypointTool : EditorTool
    {
        public override GuiContent toolbarIcon => new GuiContent("WP", "Waypoint Tool: a click on the ground adds a waypoint");

        public override void OnToolGUI(EditorWindow window)
        {
            if (!(target is Patrol patrol))
                return;
            Event e = Event.current;
            int id = GuiUtility.GetControlID(FocusType.Passive);
            if (e.type == EventType.Layout)
                HandleUtility.AddDefaultControl(id);
            Ray ray = HandleUtility.GUIPointToWorldRay(e.mousePosition);
            var ground = new Plane(Vector3.up, patrol.transform.position);
            bool hit = ground.Raycast(ray, out float distance);
            Vector3 point = ray.GetPoint(distance);
            if (e.type == EventType.Repaint && hit)
            {
                Handles.color = patrol.color;
                Handles.DrawWireDisc(point, Vector3.up, 0.3f);
                if (patrol.waypoints != null && patrol.waypoints.Length > 0)
                    Handles.DrawDottedLine(patrol.WorldPoint(patrol.waypoints.Length - 1), point, 4);
            }
            else if (e.type == EventType.MouseMove)
                HandleUtility.Repaint();
            else if (e.type == EventType.MouseDown && e.button == 0 && !e.alt && hit && HandleUtility.nearestControl == id)
            {
                patrol.Add(point);
                PatrolEditor.Changed(patrol);
                e.Use();
            }
        }
    }

    // An overlay of the Scene view: the selected Patrol's waypoints, a button that adds one ahead of the last, a button
    // that picks the Waypoint Tool.
    [Overlay(typeof(SceneView), "Patrol", "Patrol", true)]
    public sealed class PatrolOverlay : Overlay
    {
        Label m_Text;

        public override Element CreatePanelContent()
        {
            var root = new Element();
            root.style.paddingLeft = 6;
            root.style.paddingRight = 6;
            root.style.paddingTop = 4;
            root.style.paddingBottom = 4;
            m_Text = new Label();
            root.Add(m_Text);
            root.Add(new Button(AddAhead) { text = "Add a waypoint" });
            root.Add(new Button(() => ToolManager.SetActiveTool<WaypointTool>()) { text = "Waypoint Tool" });
            Show();
            return root;
        }

        public override void OnCreated() => Selection.selectionChanged += Show;
        public override void OnWillBeDestroyed() => Selection.selectionChanged -= Show;

        static Patrol Selected() => Selection.activeGameObject != null ? Selection.activeGameObject.GetComponent<Patrol>() : null;

        void Show()
        {
            if (m_Text == null)
                return;
            Patrol p = Selected();
            m_Text.text = p == null ? "No patrol selected" : $"{p.name}: {p.waypoints?.Length ?? 0} waypoints";
        }

        void AddAhead()
        {
            Patrol p = Selected();
            if (p == null)
                return;
            Vector3 last = p.waypoints != null && p.waypoints.Length > 0 ? p.WorldPoint(p.waypoints.Length - 1) : p.transform.position;
            p.Add(last + new Vector3(1, 0, 0));
            PatrolEditor.Changed(p);
            Show();
            SceneView.RepaintAll();
        }
    }

    // The scene: opened (made and saved the first time), its renderers given the materials of the pipeline the
    // editor runs (Assets/Materials/<pipeline>, made once each), the patrol selected.
    public static class DemoScene
    {
        const string kPath = "Assets/Scenes/SceneDemo.helix";

        [MenuItem("Demo/Open Scene Demo")]
        public static void Show()
        {
            foreach (string guid in AssetDatabase.FindAssets("", new[] { "Assets/Scripts" }))
                AssetDatabase.LoadAllAssetsAtPath(AssetDatabase.GUIDToAssetPath(guid));
            string full = System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Application.dataPath), kPath);
            Scene scene = System.IO.File.Exists(full) ? EditorSceneManager.OpenScene(kPath) : Make(full);
            // The pipeline is set once the editor runs its frames (-executeMethod comes before).
            EditorApplication.delayCall += Dress;
            GameObject guard = GameObject.Find("Guard");
            if (guard != null)
                Selection.activeObject = guard;
            SceneView view = EditorWindow.GetWindow<SceneView>();
            view.LookAt(new Vector3(0, 0.8f, 0.4f), Quaternion.Euler(28, -28, 0), 4.2f, false, true);
            Debug.Log($"Scene demo: {scene.name} open, {scene.rootCount} objects.");
        }

        static Mesh Model(string name) => AssetDatabase.LoadAssetAtPath<Mesh>($"Assets/Models/{name}.obj");

        static GameObject Thing(string name, string model, Vector3 at, Vector3 scale, Quaternion rotation)
        {
            var go = new GameObject(name);
            go.transform.localPosition = at;
            go.transform.localRotation = rotation;
            go.transform.localScale = scale;
            go.AddComponent<MeshFilter>().sharedMesh = Model(model);
            go.AddComponent<MeshRenderer>();
            return go;
        }

        static Scene Make(string full)
        {
            Scene scene = EditorSceneManager.NewScene(NewSceneSetup.DefaultGameObjects);
            Camera main = Camera.main;
            if (main != null)
            {
                main.transform.position = new Vector3(0, 3.2f, -8.5f);
                main.transform.rotation = Quaternion.Euler(18, 0, 0);
            }
            Thing("Ground", "Ground", Vector3.zero, Vector3.one, Quaternion.identity);
            Thing("Crate", "Cube", new Vector3(-2.2f, 0.5f, 0.6f), Vector3.one, Quaternion.Euler(0, 20, 0));
            Thing("Ball", "Sphere", new Vector3(0.2f, 0.6f, -0.8f), new Vector3(1.2f, 1.2f, 1.2f), Quaternion.identity);
            Thing("Pillar", "Cylinder", new Vector3(2.4f, 1.25f, 1.4f), new Vector3(0.8f, 2.5f, 0.8f), Quaternion.identity);
            Thing("Poster", "Poster", new Vector3(-0.4f, 1.6f, 3.4f), new Vector3(2, 2, 1), Quaternion.identity);
            GameObject guard = Thing("Guard", "Sphere", new Vector3(0, 0.3f, 0), new Vector3(0.6f, 0.6f, 0.6f), Quaternion.identity);
            guard.AddComponent<Patrol>();

            var lamp = new GameObject("Lamp");
            lamp.transform.position = new Vector3(-1.2f, 2.2f, -1.6f);
            Light point = lamp.AddComponent<Light>();
            point.type = LightType.Point;
            point.range = 6;
            point.intensity = 2;
            point.color = new Color(1f, 0.78f, 0.5f, 1f);
            var spot = new GameObject("Spot");
            spot.transform.position = new Vector3(2.4f, 4, -1.5f);
            spot.transform.rotation = Quaternion.Euler(60, -20, 0);
            Light s = spot.AddComponent<Light>();
            s.type = LightType.Spot;
            s.range = 9;
            s.spotAngle = 40;
            s.intensity = 3;
            s.color = new Color(0.55f, 0.75f, 1f, 1f);

            // A top view on Display 2 (the Game view's display dropdown shows it).
            var top = new GameObject("Top Camera");
            top.transform.position = new Vector3(0, 12, 0);
            top.transform.rotation = Quaternion.Euler(90, 0, 0);
            Camera topCamera = top.AddComponent<Camera>();
            topCamera.orthographic = true;
            topCamera.orthographicSize = 6.5f;
            topCamera.targetDisplay = 1;
            topCamera.clearFlags = CameraClearFlags.SolidColor;
            topCamera.backgroundColor = new Color(0.12f, 0.13f, 0.15f, 1);

            System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(full));
            EditorSceneManager.SaveScene(scene, kPath);
            AssetDatabase.Refresh();
            return scene;
        }

        // The physical pipeline takes lights in physical units (its light data beside each light, as the reference's
        // pipeline adds to every light): the sun in lux, the lamps in lumen. The standard pipeline leaves the data be.
        static void LightUnits()
        {
            foreach ((string name, LightUnit unit, float value) in new[] { ("Directional Light", LightUnit.Lux, 100000f), ("Lamp", LightUnit.Lumen, 12000f),
                                                                          ("Spot", LightUnit.Lumen, 30000f) })
            {
                GameObject go = GameObject.Find(name);
                if (go == null || go.GetComponent<Light>() == null || go.GetComponent<PhysicalAdditionalLightData>() != null)
                    continue;
                PhysicalAdditionalLightData data = go.AddComponent<PhysicalAdditionalLightData>();
                data.lightUnit = unit;
                data.intensity = value;
            }
        }

        // The materials of the pipeline the editor runs (its default material's shader, else its lit shader), each
        // made once as an asset.
        static void Dress()
        {
            var asset = HelixEngine.Rendering.GraphicsSettings.currentRenderPipeline;
            if (asset == null)
                return;
            bool physical = asset.GetType().Name.Contains("Physical");
            Material template = asset.defaultMaterial;
            if (template == null && Shader.Find(physical ? "Physical/OpenPBR Lite" : "Universal Render Pipeline/Lit") is Shader lit)
                template = new Material(lit);
            if (template == null)
                return;
            string pipeline = physical ? "Physical" : "Standard";
            LightUnits();
            Texture checker = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Textures/Checker.png");
            Texture panda = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Textures/CC0/KenneyPanda.png");
            var looks = new Dictionary<string, (Color color, Texture texture)>
            {
                ["Ground"] = (Color.white, checker),
                ["Crate"] = (new Color(0.85f, 0.6f, 0.35f, 1), checker),
                ["Ball"] = (new Color(0.35f, 0.6f, 0.95f, 1), null),
                ["Pillar"] = (new Color(0.8f, 0.8f, 0.78f, 1), null),
                ["Poster"] = (Color.white, panda),
                ["Guard"] = (new Color(1f, 0.75f, 0.2f, 1), null),
            };
            foreach (var kv in looks)
            {
                GameObject go = GameObject.Find(kv.Key);
                if (go == null || go.GetComponent<MeshRenderer>() is not MeshRenderer r)
                    continue;
                string path = $"Assets/Materials/{pipeline}/{kv.Key}.mat";
                Material m = AssetDatabase.LoadAssetAtPath<Material>(path);
                if (m == null)
                {
                    m = new Material(template) { name = kv.Key, color = kv.Value.color };
                    if (kv.Value.texture != null)
                        m.mainTexture = kv.Value.texture;
                    if (!AssetDatabase.IsValidFolder("Assets/Materials"))
                        AssetDatabase.CreateFolder("Assets", "Materials");
                    if (!AssetDatabase.IsValidFolder($"Assets/Materials/{pipeline}"))
                        AssetDatabase.CreateFolder("Assets/Materials", pipeline);
                    AssetDatabase.CreateAsset(m, path);
                }
                if (r.sharedMaterial != m)
                    r.sharedMaterial = m;
            }
        }
    }
}
