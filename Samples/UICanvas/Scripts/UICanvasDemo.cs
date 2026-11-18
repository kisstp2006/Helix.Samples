using System.Collections.Generic;
using HelixEngine;
using HelixEngine.CanvasUI;
using HelixEngine.Rendering;
using HelixEngine.Text;
using HelixEngine.UIEvents;

namespace HelixSamples
{
    // The canvas UI sample: a 3D yard (crates, a terminal post) with world text labels and a world space terminal
    // clicked and typed into through the main camera; over it a HUD (a player card with a health bar), an inventory of
    // icons in a scrolled grid, settings (a toggle, a slider, a dropdown, a text input) and a status line. The C++
    // script UICanvasCpp adds a panel of its own controls. Its own images are drawn by Tools/GenerateData.py beside
    // CC0 icons of Kenney (Samples/UICanvas/CREDITS.txt).
    public class UICanvasDemo : GameBehaviour
    {
        public Texture2D frame, avatar, star, slot;
        public Texture2D gear, trophy, medal, sound, music, wrench, panda, penguin, owl, rabbit, narwhal, frog;

        static readonly Color Ink = new Color(0.92f, 0.94f, 0.98f, 1);
        static readonly Color Dim = new Color(0.62f, 0.68f, 0.78f, 1);
        static readonly Color Accent = new Color(1f, 0.72f, 0.28f, 1);

        Sprite m_Frame, m_Slot;
        Camera m_Camera;
        CanvasText m_Status, m_PlayerName, m_Score, m_TerminalLog;
        Image m_Health;
        TextInput m_TerminalInput;
        GameObject m_Inventory, m_Settings;
        readonly List<WorldText> m_Labels = new List<WorldText>();
        readonly List<Transform> m_Crates = new List<Transform>();
        // The solids and their colors: their materials are the pipeline's, made again when it changes.
        readonly List<(MeshRenderer renderer, Color color)> m_Solids = new List<(MeshRenderer, Color)>();
        bool m_MaterialsMade;
        readonly List<string> m_Log = new List<string>();
        int m_ScoreValue = 1250;

        void Start()
        {
            TextSettings.defaultFontAsset = MachineFont();
            m_Frame = Sliced(frame, 16);
            m_Slot = Sliced(slot, 8);
            SetUpCamera();
            BuildYard();
            new GameObject("EventSystem").AddComponent<StandaloneInputModule>();
            Canvas hud = MakeOverlay("HUD", 0);
            BuildPlayerCard(hud.transform);
            BuildInventory(hud.transform);
            BuildSettings(hud.transform);
            BuildTopButtons(hud.transform);
            m_Status = Label(hud.transform, "Status", "Click the terminal in the yard, type a command and press Enter.", 18, Dim,
                             new Vector2(0, 0), new Vector2(1, 0), new Vector2(24, 12), new Vector2(-24, 40));
        }

        void Update()
        {
            // The solids take their materials once the shaders are loaded (after the scene starts).
            if (!m_MaterialsMade && Shader.Find(LitShaderName()) != null)
            {
                m_MaterialsMade = true;
                foreach ((MeshRenderer renderer, Color color) in m_Solids)
                    renderer.sharedMaterial = Lit(color);
            }
            // The crates turn slowly (not in a batch run, whose screenshot is still).
            if (Application.isBatchMode)
                return;
            for (int i = 0; i < m_Crates.Count; i++)
                m_Crates[i].Rotate(0, (10 + 6 * i) * Time.deltaTime, 0);
        }

        // ------------------------------------------------------------------------------------------- set up

        static FontAsset MachineFont()
        {
            foreach (string family in new[] { "Segoe UI", "Arial", "DejaVu Sans", "Noto Sans", "Liberation Sans" })
            {
                FontAsset f = FontAsset.CreateFontAsset(family, "Regular");
                if (f != null)
                    return f;
            }
            return null;
        }

        static Sprite Sliced(Texture2D t, float border) =>
            t == null ? null : Sprite.Create(t, new Rect(0, 0, t.width, t.height), new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect,
                                             new Vector4(border, border, border, border));

        static Sprite Whole(Texture2D t) =>
            t == null ? null : Sprite.Create(t, new Rect(0, 0, t.width, t.height), new Vector2(0.5f, 0.5f), 100);

        void SetUpCamera()
        {
            m_Camera = Camera.main;
            if (m_Camera == null)
                return;
            m_Camera.transform.position = new Vector3(0, 2.2f, -7.5f);
            m_Camera.transform.rotation = Quaternion.Euler(12, 0, 0);
            m_Camera.fieldOfView = 55;
            var sun = new GameObject("Sun").AddComponent<Light>();
            sun.type = LightType.Directional;
            // The Physical pipeline's lights are in physical units (lux) under its default exposure (EV100 15).
            sun.intensity = LitShaderName().StartsWith("Physical") ? 80000f : 1.2f;
            sun.transform.rotation = Quaternion.Euler(50, -30, 0);
        }

        // --------------------------------------------------------------------------------------------- yard

        // The lit shader of the launcher's pipeline (-pipeline standard|physical|none): the pipelines are set up by the
        // launcher in C++, the shader is picked by its name.
        static string LitShaderName()
        {
            string[] args = System.Environment.GetCommandLineArgs();
            string pipeline = "standard";
            for (int i = 0; i + 1 < args.Length; i++)
                if (args[i] == "-pipeline")
                    pipeline = args[i + 1];
            return pipeline switch
            {
                "physical" => "Physical/OpenPBR Lite",
                "none" => "UI/Default",
                _ => "Universal Render Pipeline/Lit",
            };
        }

        static Material Lit(Color color)
        {
            Shader shader = Shader.Find(LitShaderName()) ?? Shader.Find("UI/Default");
            var m = new Material(shader);
            m.SetColor("_BaseColor", color);
            m.color = color;
            return m;
        }

        // A box mesh of the size, its faces with their own normals.
        static Mesh Box(Vector3 size)
        {
            Vector3 h = size * 0.5f;
            var vertices = new List<Vector3>();
            var normals = new List<Vector3>();
            var uvs = new List<Vector2>();
            var triangles = new List<int>();
            Vector3[] axes = { Vector3.right, Vector3.left, Vector3.up, Vector3.down, Vector3.forward, Vector3.back };
            foreach (Vector3 n in axes)
            {
                Vector3 u = Mathf.Abs(n.y) > 0.5f ? Vector3.right : Vector3.up;
                Vector3 v = Vector3.Cross(n, u);
                int b = vertices.Count;
                foreach (Vector2 c in new[] { new Vector2(-1, -1), new Vector2(1, -1), new Vector2(1, 1), new Vector2(-1, 1) })
                {
                    Vector3 p = n + u * c.x + v * c.y;
                    vertices.Add(new Vector3(p.x * h.x, p.y * h.y, p.z * h.z));
                    normals.Add(n);
                    uvs.Add(new Vector2((c.x + 1) * 0.5f, (c.y + 1) * 0.5f));
                }
                triangles.AddRange(new[] { b, b + 1, b + 2, b, b + 2, b + 3 });
            }
            var mesh = new Mesh();
            mesh.SetVertices(vertices);
            mesh.SetNormals(normals);
            mesh.SetUVs(0, uvs);
            mesh.SetTriangles(triangles, 0);
            mesh.RecalculateBounds();
            mesh.RecalculateTangents();
            return mesh;
        }

        GameObject Solid(string name, Vector3 position, Vector3 size, Color color)
        {
            var go = new GameObject(name);
            go.transform.position = position;
            go.AddComponent<MeshFilter>().sharedMesh = Box(size);
            m_Solids.Add((go.AddComponent<MeshRenderer>(), color));
            return go;
        }

        void BuildYard()
        {
            Solid("Ground", new Vector3(0, -0.05f, 4), new Vector3(24, 0.1f, 20), new Color(0.32f, 0.42f, 0.3f, 1));
            Color[] colors = { new Color(0.72f, 0.48f, 0.26f, 1), new Color(0.3f, 0.5f, 0.75f, 1), new Color(0.75f, 0.3f, 0.32f, 1) };
            string[] names = { "Supplies", "Water", "Tools" };
            for (int i = 0; i < 3; i++)
            {
                GameObject crate = Solid("Crate " + names[i], new Vector3(-1.9f + i * 1.35f, 0.5f, 3 + i * 0.6f), new Vector3(1, 1, 1), colors[i]);
                m_Crates.Add(crate.transform);
                m_Labels.Add(WorldLabel(names[i], crate.transform.position + new Vector3(0, 0.95f, 0)));
            }
            BuildTerminal();
        }

        // A label in the scene: world text a unit and a half wide, facing the camera.
        WorldText WorldLabel(string text, Vector3 at)
        {
            var go = new GameObject("Label " + text);
            var rect = go.AddComponent<RectTransform>();
            rect.sizeDelta = new Vector2(3, 0.8f);
            go.transform.position = at;
            go.transform.rotation = Quaternion.Euler(12, 0, 0);
            var label = go.AddComponent<WorldText>();
            label.text = text;
            label.fontSize = 5;
            label.alignment = TextAlignmentOptions.Center;
            label.color = Accent;
            label.outlineWidth = 0.2f;
            label.outlineColor = new Color32(20, 24, 32, 255);
            return label;
        }

        // The terminal: a post with a world space canvas on its face, turned toward the yard's middle.
        void BuildTerminal()
        {
            GameObject post = Solid("Terminal Post", new Vector3(1.7f, 1.35f, 0.4f), new Vector3(2.4f, 2.4f, 0.3f), new Color(0.2f, 0.22f, 0.27f, 1));
            post.transform.rotation = Quaternion.Euler(0, -18, 0);
            Solid("Terminal Foot", new Vector3(1.7f, 0.08f, 0.4f), new Vector3(0.5f, 0.16f, 0.5f), new Color(0.2f, 0.22f, 0.27f, 1));
            var go = new GameObject("Terminal");
            var canvas = go.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.WorldSpace;
            canvas.worldCamera = m_Camera;
            go.AddComponent<GraphicRaycaster>();
            var rect = (RectTransform)go.transform;
            rect.sizeDelta = new Vector2(440, 400);
            go.transform.SetParent(post.transform, false);
            go.transform.localPosition = new Vector3(0, 0, -0.16f);
            go.transform.localScale = new Vector3(0.005f, 0.005f, 0.005f);

            Image screen = Panel(go.transform, "Screen", new Color(0.05f, 0.09f, 0.08f, 0.96f));
            Stretch(screen.rectTransform, 8, 8, 8, 8);
            Label(screen.transform, "Title", "YARD TERMINAL", 30, new Color(0.45f, 1f, 0.6f, 1), new Vector2(0, 1), new Vector2(1, 1),
                  new Vector2(20, -56), new Vector2(-20, -14));
            m_TerminalLog = Label(screen.transform, "Log", "", 22, new Color(0.62f, 0.95f, 0.7f, 1), new Vector2(0, 0), new Vector2(1, 1),
                                  new Vector2(20, 110), new Vector2(-20, -64));
            m_TerminalLog.alignment = TextAlignmentOptions.TopLeft;
            Log("> status");
            Log("3 crates, all sealed.");

            GameObject field = TextDefaultControls.CreateInputField(new TextDefaultControls.Resources { inputField = m_Slot });
            field.transform.SetParent(screen.transform, false);
            Place((RectTransform)field.transform, new Vector2(0, 0), new Vector2(1, 0), new Vector2(20, 56), new Vector2(-150, 100));
            m_TerminalInput = field.GetComponent<TextInput>();
            m_TerminalInput.textComponent.color = new Color(0.62f, 0.95f, 0.7f, 1);
            m_TerminalInput.textComponent.fontSize = 22;
            ((TextBase)m_TerminalInput.placeholder).text = "Type a command...";
            ((TextBase)m_TerminalInput.placeholder).fontSize = 22;
            m_TerminalInput.onSubmit.AddListener(Run);

            Button run = MakeButton(screen.transform, "Run", new Vector2(1, 0), new Vector2(1, 0), new Vector2(-140, 56), new Vector2(-20, 100));
            run.onClick.AddListener(() => Run(m_TerminalInput.text));
        }

        void Log(string line)
        {
            m_Log.Add(line);
            while (m_Log.Count > 7)
                m_Log.RemoveAt(0);
            m_TerminalLog.text = string.Join("\n", m_Log);
        }

        void Run(string command)
        {
            command = (command ?? "").Trim();
            if (command.Length == 0)
                return;
            Log("> " + command);
            switch (command.ToLowerInvariant())
            {
            case "open":
                Log("Crates opened: +150 score.");
                m_ScoreValue += 150;
                m_Score.text = $"Score <b>{m_ScoreValue}</b>";
                break;
            case "heal":
                m_Health.fillAmount = 1;
                Log("Health restored.");
                break;
            default:
                Log("Unknown command. Try open or heal.");
                break;
            }
            m_TerminalInput.text = "";
            Status($"Terminal ran '{command}'.");
        }

        // ---------------------------------------------------------------------------------------------- HUD

        static Canvas MakeOverlay(string name, int order)
        {
            var go = new GameObject(name);
            var canvas = go.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.sortingOrder = order;
            var scaler = go.AddComponent<CanvasScaler>();
            scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            scaler.referenceResolution = new Vector2(1280, 720);
            scaler.matchWidthOrHeight = 0.5f;
            go.AddComponent<GraphicRaycaster>();
            return canvas;
        }

        Image Panel(Transform parent, string name, Color color)
        {
            var go = new GameObject(name);
            go.AddComponent<RectTransform>();
            go.transform.SetParent(parent, false);
            var image = go.AddComponent<Image>();
            image.sprite = m_Frame;
            image.type = Image.Type.Sliced;
            image.color = color;
            return image;
        }

        static void Stretch(RectTransform r, float left, float bottom, float right, float top)
        {
            r.anchorMin = Vector2.zero;
            r.anchorMax = Vector2.one;
            r.offsetMin = new Vector2(left, bottom);
            r.offsetMax = new Vector2(-right, -top);
        }

        static void Place(RectTransform r, Vector2 anchorMin, Vector2 anchorMax, Vector2 offsetMin, Vector2 offsetMax)
        {
            r.anchorMin = anchorMin;
            r.anchorMax = anchorMax;
            r.offsetMin = offsetMin;
            r.offsetMax = offsetMax;
        }

        static CanvasText Label(Transform parent, string name, string text, float size, Color color, Vector2 anchorMin, Vector2 anchorMax,
                                Vector2 offsetMin, Vector2 offsetMax)
        {
            var go = new GameObject(name);
            go.AddComponent<RectTransform>();
            go.transform.SetParent(parent, false);
            Place((RectTransform)go.transform, anchorMin, anchorMax, offsetMin, offsetMax);
            var label = go.AddComponent<CanvasText>();
            label.text = text;
            label.fontSize = size;
            label.color = color;
            label.alignment = TextAlignmentOptions.Left;
            label.raycastTarget = false;
            return label;
        }

        Button MakeButton(Transform parent, string text, Vector2 anchorMin, Vector2 anchorMax, Vector2 offsetMin, Vector2 offsetMax)
        {
            GameObject go = TextDefaultControls.CreateButton(new TextDefaultControls.Resources { standard = m_Frame });
            go.transform.SetParent(parent, false);
            Place((RectTransform)go.transform, anchorMin, anchorMax, offsetMin, offsetMax);
            go.GetComponent<Image>().color = new Color(0.34f, 0.46f, 0.7f, 1);
            var label = go.GetComponentInChildren<CanvasText>();
            label.text = text;
            label.color = Ink;
            label.fontSize = 20;
            return go.GetComponent<Button>();
        }

        void Status(string text)
        {
            if (m_Status != null)
                m_Status.text = text;
        }

        void BuildPlayerCard(Transform hud)
        {
            Image card = Panel(hud, "Player Card", new Color(0.13f, 0.16f, 0.22f, 0.92f));
            Place(card.rectTransform, new Vector2(0, 1), new Vector2(0, 1), new Vector2(20, -136), new Vector2(380, -20));
            var face = new GameObject("Avatar").AddComponent<Image>();
            face.transform.SetParent(card.transform, false);
            face.sprite = Whole(avatar);
            Place(face.rectTransform, new Vector2(0, 0.5f), new Vector2(0, 0.5f), new Vector2(14, -42), new Vector2(98, 42));
            m_PlayerName = Label(card.transform, "Name", "Wanderer", 26, Ink, new Vector2(0, 1), new Vector2(1, 1), new Vector2(112, -46), new Vector2(-14, -10));
            m_Score = Label(card.transform, "Score", $"Score <b>{m_ScoreValue}</b>", 20, Accent, new Vector2(0, 1), new Vector2(1, 1),
                            new Vector2(112, -74), new Vector2(-14, -46));
            // The health bar: a filled image over a dark track.
            Image track = Panel(card.transform, "Health Track", new Color(0.06f, 0.07f, 0.1f, 1));
            Place(track.rectTransform, new Vector2(0, 0), new Vector2(1, 0), new Vector2(112, 16), new Vector2(-14, 38));
            m_Health = new GameObject("Health").AddComponent<Image>();
            m_Health.transform.SetParent(track.transform, false);
            Stretch(m_Health.rectTransform, 3, 3, 3, 3);
            m_Health.color = new Color(0.36f, 0.86f, 0.42f, 1);
            m_Health.type = Image.Type.Filled;
            m_Health.fillMethod = Image.FillMethod.Horizontal;
            m_Health.fillAmount = 0.68f;
            var hearts = new GameObject("Star").AddComponent<Image>();
            hearts.transform.SetParent(card.transform, false);
            hearts.sprite = Whole(star);
            hearts.color = Accent;
            Place(hearts.rectTransform, new Vector2(1, 1), new Vector2(1, 1), new Vector2(-44, -44), new Vector2(-14, -14));
        }

        void BuildTopButtons(Transform hud)
        {
            Button inventory = MakeButton(hud, "Inventory", new Vector2(1, 1), new Vector2(1, 1), new Vector2(-340, -64), new Vector2(-190, -20));
            inventory.onClick.AddListener(() =>
            {
                m_Inventory.SetActive(!m_Inventory.activeSelf);
                Status(m_Inventory.activeSelf ? "Inventory shown." : "Inventory hidden.");
            });
            Button settings = MakeButton(hud, "Settings", new Vector2(1, 1), new Vector2(1, 1), new Vector2(-170, -64), new Vector2(-20, -20));
            settings.onClick.AddListener(() =>
            {
                m_Settings.SetActive(!m_Settings.activeSelf);
                Status(m_Settings.activeSelf ? "Settings shown." : "Settings hidden.");
            });
        }

        // The inventory: a scrolled grid of slots, each an icon with a count; a click names the item.
        void BuildInventory(Transform hud)
        {
            Image window = Panel(hud, "Inventory", new Color(0.13f, 0.16f, 0.22f, 0.94f));
            m_Inventory = window.gameObject;
            Place(window.rectTransform, new Vector2(0, 0.5f), new Vector2(0, 0.5f), new Vector2(20, -230), new Vector2(380, 190));
            Label(window.transform, "Title", "Inventory", 24, Ink, new Vector2(0, 1), new Vector2(1, 1), new Vector2(16, -44), new Vector2(-16, -8));

            GameObject view = DefaultControls.CreateScrollView(new DefaultControls.Resources { background = m_Slot, standard = m_Frame });
            view.transform.SetParent(window.transform, false);
            Stretch((RectTransform)view.transform, 12, 12, 12, 52);
            view.GetComponent<Image>().color = new Color(0.08f, 0.1f, 0.14f, 1);
            var scroll = view.GetComponent<ScrollRect>();
            scroll.horizontal = false;
            scroll.horizontalScrollbar = null;
            Object.Destroy(scroll.transform.Find("Scrollbar Horizontal").gameObject);
            RectTransform content = scroll.content;
            var grid = content.gameObject.AddComponent<GridLayoutGroup>();
            grid.cellSize = new Vector2(70, 70);
            grid.spacing = new Vector2(8, 8);
            grid.padding = new RectOffset(8, 8, 8, 8);
            grid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
            grid.constraintCount = 4;
            var fitter = content.gameObject.AddComponent<ContentSizeFitter>();
            fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

            (Texture2D icon, string name, int count, Color tint)[] items =
            {
                (trophy, "Trophy", 1, Accent), (medal, "Medal", 3, Accent), (gear, "Gear", 12, Ink), (wrench, "Wrench", 2, Ink),
                (sound, "Horn", 1, Ink), (music, "Song", 4, Ink), (panda, "Panda", 1, Color.white), (penguin, "Penguin", 2, Color.white),
                (owl, "Owl", 1, Color.white), (rabbit, "Rabbit", 5, Color.white), (narwhal, "Narwhal", 1, Color.white), (frog, "Frog", 3, Color.white),
                (star, "Star", 20, Accent), (gear, "Spare gear", 6, Dim), (trophy, "Cup", 2, Dim), (medal, "Badge", 9, Dim),
                (panda, "Panda cub", 2, Dim), (owl, "Owlet", 1, Dim), (frog, "Tadpole", 7, Dim), (wrench, "Spanner", 1, Dim),
                (music, "Tune", 3, Accent), (sound, "Bell", 1, Accent), (rabbit, "Bunny", 4, Dim), (narwhal, "Tusk", 1, Dim),
            };
            foreach ((Texture2D icon, string name, int count, Color tint) in items)
            {
                GameObject cell = TextDefaultControls.CreateButton(new TextDefaultControls.Resources { standard = m_Slot });
                cell.name = name;
                cell.transform.SetParent(content, false);
                Object.Destroy(cell.GetComponentInChildren<CanvasText>().gameObject);
                var picture = new GameObject("Icon").AddComponent<Image>();
                picture.transform.SetParent(cell.transform, false);
                picture.sprite = Whole(icon);
                picture.color = tint;
                picture.preserveAspect = true;
                picture.raycastTarget = false;
                Stretch(picture.rectTransform, 10, 10, 10, 10);
                CanvasText amount = Label(cell.transform, "Count", "x" + count, 16, Ink, new Vector2(0, 0), new Vector2(1, 0), new Vector2(4, 2), new Vector2(-6, 22));
                amount.alignment = TextAlignmentOptions.Right;
                string itemName = name;
                int itemCount = count;
                cell.GetComponent<Button>().onClick.AddListener(() => Status($"{itemName}: {itemCount} in the pack."));
            }
        }

        // The settings: labels on the left, the controls on the right.
        void BuildSettings(Transform hud)
        {
            Image window = Panel(hud, "Settings", new Color(0.13f, 0.16f, 0.22f, 0.94f));
            m_Settings = window.gameObject;
            Place(window.rectTransform, new Vector2(1, 0.5f), new Vector2(1, 0.5f), new Vector2(-420, -110), new Vector2(-20, 210));
            Label(window.transform, "Title", "Settings", 24, Ink, new Vector2(0, 1), new Vector2(1, 1), new Vector2(16, -44), new Vector2(-16, -8));
            var resources = new TextDefaultControls.Resources { standard = m_Frame, background = m_Slot, inputField = m_Slot, knob = Whole(star),
                                                                checkmark = Whole(star), dropdown = Whole(star), mask = m_Slot };
            float y = -70;
            RectTransform Row(string title)
            {
                Label(window.transform, title, title, 20, Dim, new Vector2(0, 1), new Vector2(0, 1), new Vector2(16, y - 34), new Vector2(170, y));
                var row = new GameObject(title + " Control").AddComponent<RectTransform>();
                row.SetParent(window.transform, false);
                Place(row, new Vector2(0, 1), new Vector2(1, 1), new Vector2(176, y - 34), new Vector2(-16, y));
                y -= 54;
                return row;
            }

            GameObject toggle = DefaultControls.CreateToggle(new DefaultControls.Resources { standard = m_Slot, checkmark = Whole(star) });
            toggle.transform.SetParent(Row("Labels"), false);
            Stretch((RectTransform)toggle.transform, 0, 0, 0, 0);
            toggle.GetComponentInChildren<CanvasText>().text = "Show in the yard";
            toggle.GetComponentInChildren<CanvasText>().color = Ink;
            toggle.GetComponent<Toggle>().onValueChanged.AddListener(on =>
            {
                foreach (WorldText l in m_Labels)
                    l.enabled = on;
                Status(on ? "Labels shown." : "Labels hidden.");
            });

            GameObject slider = DefaultControls.CreateSlider(new DefaultControls.Resources { background = m_Slot, standard = m_Frame, knob = Whole(star) });
            slider.transform.SetParent(Row("Field of view"), false);
            Stretch((RectTransform)slider.transform, 0, 6, 0, 6);
            var fov = slider.GetComponent<Slider>();
            fov.minValue = 35;
            fov.maxValue = 80;
            fov.value = m_Camera != null ? m_Camera.fieldOfView : 55;
            fov.onValueChanged.AddListener(v =>
            {
                if (m_Camera != null)
                    m_Camera.fieldOfView = v;
                Status($"Field of view {v:0} degrees.");
            });

            GameObject dropdown = TextDefaultControls.CreateDropdown(resources);
            dropdown.transform.SetParent(Row("Quality"), false);
            Stretch((RectTransform)dropdown.transform, 0, 0, 0, 0);
            var quality = dropdown.GetComponent<Dropdown>();
            quality.ClearOptions();
            quality.AddOptions(new List<string> { "Low", "Medium", "High", "Ultra" });
            quality.captionText.color = Ink;
            quality.value = 2;
            quality.onValueChanged.AddListener(v => Status($"Quality: {quality.options[v].text}."));

            GameObject field = TextDefaultControls.CreateInputField(resources);
            field.transform.SetParent(Row("Name"), false);
            Stretch((RectTransform)field.transform, 0, 0, 0, 0);
            var input = field.GetComponent<TextInput>();
            input.textComponent.color = Ink;
            ((TextBase)input.placeholder).color = Dim;
            input.text = m_PlayerName.text;
            input.characterLimit = 16;
            input.onEndEdit.AddListener(v =>
            {
                if (v.Length > 0)
                    m_PlayerName.text = v;
                Status($"Name set to {m_PlayerName.text}.");
            });
        }
    }
}
